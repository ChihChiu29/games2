/*
 * Settler automation: press A on an active Settlers unit to have it improve
 * an area on its own.  The area is the 21 square radius of the nearest own
 * city within 2 squares, otherwise the 5x5 squares around the unit.  For
 * every land square it builds a road, then a railroad, then irrigation
 * (only on terrain where irrigating does not change the terrain type).
 *
 * The unit is driven with the game's own orders: when it waits for orders
 * the next key (a move or R / I) is supplied, so the game applies all its
 * rules (technology, terrain, water).  An order the game refuses is
 * skipped.  Pressing any key while the unit is active cancels the job.
 */
#include <stdio.h>
#include <string.h>
#include "native.h"
#include "runtime.h"
#include "addons.h"

#define UNIT(p, u) (0x90f4 + 0x600 * (p) + 12 * (u))
#define CITY(c) (0x706c + 0x1c * (c))

/* map square improvement bits (map layer in graphics page 2) */
#define IMP_CITY 0x01
#define IMP_IRRIGATION 0x02
#define IMP_MINE 0x04
#define IMP_ROAD 0x08
#define IMP_RAILROAD 0x10

enum { T_ROAD, T_RAIL, T_IRRIGATE, T_COUNT };

struct square { uint8_t x, y, failed; };   /* failed: bit per task */
struct job {
    uint8_t active, player, type;
    uint8_t n, stuck, tries, progress;
    uint8_t last_x, last_y, last_task, last_sq;   /* last_task 0xff = move */
    struct square sq[25];
};
static struct job jobs[128];

/* Auto-Explore state for non-Settler units (triggered via EXPL button or Shift+G) */
struct explore_job {
    uint8_t active, player, type;
    uint8_t last_x, last_y, stuck;
};
static struct explore_job ejobs[128];

/* Settler "Road To" state (triggered via ROAD button or Shift+R, then map click) */
struct roadto_job {
    uint8_t active, player;
    uint8_t gx, gy;
    uint8_t last_x, last_y, stuck;
    uint8_t skip_x, skip_y, tried_road;
};
static struct roadto_job rjobs[128];
static int roadto_pick_unit = -1, roadto_pick_player = -1, roadto_pick_turn = -1;

/* Shared BFS buffers for Auto-Explore, Road-To, and full-map Goto (max 160x50 = 8000) */
static uint8_t bfs_vis[160 * 50];
static uint8_t bfs_fdir[160 * 50];
static uint16_t bfs_q[160 * 50];

/* call a lifted helper without disturbing the interrupted code */
static uint16_t map_call(func_t fn, int x, int y)
{
    cpu_t saved = cpu;
    call_lifted(0x1b05, fn, 2, x, y);
    uint16_t r = AX;
    cpu = saved;
    return r;
}

static int terrain(int x, int y) { return map_call(f_1b05_133c, x, y); }
static int improvements(int x, int y) { return map_call(f_1b05_1577, x, y); }

static int wrapx(int x) { return (x % MAP_W + MAP_W) % MAP_W; }

static int dist(int ax, int ay, int bx, int by)
{
    int dx = wrapx(ax - bx);
    if (dx > MAP_W / 2)
        dx = MAP_W - dx;
    int dy = ay > by ? ay - by : by - ay;
    return dx > dy ? dx : dy;
}

static int irrigable(int t)
{
    /* desert, plains, grassland, hills, tundra, river */
    return t == 0 || t == 1 || t == 2 || t == 4 || t == 6 || t == 11;
}

static int needs(struct square *s, int task)
{
    if (s->failed & (1 << task))
        return 0;
    int t = terrain(s->x, s->y);
    if (t == 10)                    /* ocean */
        return 0;
    int imp = improvements(s->x, s->y);
    if (imp & IMP_CITY)
        return 0;
    switch (task) {
    case T_ROAD: return !(imp & IMP_ROAD);
    case T_RAIL: return (imp & IMP_ROAD) && !(imp & IMP_RAILROAD);
    default: return irrigable(t) && !(imp & (IMP_IRRIGATION | IMP_MINE));
    }
}

static void start(int player, int unit, int ux, int uy)
{
    struct job *j = &jobs[unit];
    memset(j, 0, sizeof *j);
    j->active = 1;
    j->player = (uint8_t)player;
    j->last_task = 0xfe;
    /* nearest own city within two squares */
    int best = -1, bd = 3;
    for (int c = 0; c < 128; c++) {
        uint16_t r = CITY(c);
        if (GB(r + 6) == 0xff || GB(r + 11) != player)
            continue;
        int d = dist(GB(r + 4), GB(r + 5), ux, uy);
        if (d < bd) {
            bd = d;
            best = c;
        }
    }
    int cx = ux, cy = uy, radius = 2;
    if (best >= 0) {
        cx = GB(CITY(best) + 4);
        cy = GB(CITY(best) + 5);
    }
    /* squares sorted by distance from the unit */
    for (int d = 0; d <= 4 && j->n < 25; d++)
        for (int dy = -radius; dy <= radius; dy++)
            for (int dx = -radius; dx <= radius; dx++) {
                if (best >= 0 && (dx == 2 || dx == -2) && (dy == 2 || dy == -2))
                    continue;       /* city radius has no corners */
                int x = wrapx(cx + dx), y = cy + dy;
                if (y < 0 || y > 49 || dist(x, y, ux, uy) != d || j->n >= 25)
                    continue;
                j->sq[j->n].x = (uint8_t)x;
                j->sq[j->n].y = (uint8_t)y;
                j->n++;
            }
    ovl_msg(best >= 0 ? "Settlers: improving the city area (%d squares)"
                      : "Settlers: improving the area (%d squares)", j->n);
}

/* direction keys clockwise from north */
static const uint16_t dir_key[8] = {0x4800, 0x4900, 0x4d00, 0x5100,
                                    0x5000, 0x4f00, 0x4b00, 0x4700};
static const int dir_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int dir_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

/* step towards the target; after failed attempts try the neighbouring
   directions (0, +1, -1, +2, -2) to get around water */
static uint16_t move_key(int ux, int uy, int tx, int ty, int attempt)
{
    int dx = wrapx(tx - ux);
    if (dx > MAP_W / 2)
        dx -= MAP_W;
    int dy = ty - uy;
    int sx = dx > 0 ? 1 : dx < 0 ? -1 : 0, sy = dy > 0 ? 1 : dy < 0 ? -1 : 0;
    int d = 0;
    for (int k = 0; k < 8; k++)
        if (dir_dx[k] == sx && dir_dy[k] == sy)
            d = k;
    static const int turn[5] = {0, 1, -1, 2, -2};
    return dir_key[(d + turn[attempt % 5] + 8) % 8];
}

static uint16_t next_key(struct job *j, int ux, int uy)
{
    /* did the last order have an effect? */
    if (j->last_task < T_COUNT) {
        struct square *s = &j->sq[j->last_sq];
        if (s->x == ux && s->y == uy && needs(s, j->last_task))
            s->failed |= (uint8_t)(1 << j->last_task);   /* refused */
        else
            j->progress = 1;
        j->tries = 0;
    } else if (j->last_task == 0xff) {
        if (j->last_x == ux && j->last_y == uy) {
            if (++j->tries >= 5) {                        /* cannot get there */
                j->sq[j->last_sq].failed = 0xff;
                j->tries = 0;
            }
        } else
            j->tries = 0;
    }
    /* first square (nearest first) that still needs work */
    for (int i = 0; i < j->n; i++) {
        struct square *s = &j->sq[i];
        for (int t = 0; t < T_COUNT; t++) {
            if (!needs(s, t))
                continue;
            j->last_sq = (uint8_t)i;
            j->last_x = (uint8_t)ux;
            j->last_y = (uint8_t)uy;
            if (s->x != ux || s->y != uy) {
                j->last_task = 0xff;
                logmsg("autowork: unit at %d,%d moves towards %d,%d (task %d, imp %02x)",
                       ux, uy, s->x, s->y, t, improvements(s->x, s->y));
                return move_key(ux, uy, s->x, s->y, j->tries);
            }
            j->last_task = (uint8_t)t;
            j->tries = 0;
            logmsg("autowork: unit at %d,%d: %s (imp %02x, terrain %d)", ux, uy,
                   t == T_ROAD ? "road" : t == T_RAIL ? "railroad" : "irrigation",
                   improvements(ux, uy), terrain(ux, uy));
            return t == T_IRRIGATE ? 'i' : 'r';
        }
    }
    /* everything done or refused: work done in this pass may have made
       refused orders possible (irrigation next to new irrigation), so go
       round again; unreachable squares stay skipped */
    if (j->progress) {
        j->progress = 0;
        for (int i = 0; i < j->n; i++)
            if (j->sq[i].failed != 0xff)
                j->sq[i].failed = 0;
        j->last_task = 0xfe;
        return next_key(j, ux, uy);
    }
    j->active = 0;
    ovl_msg("Settlers: area finished");
    return 0;
}

static uint16_t pending_key;      /* order put into the keyboard buffer */
static int pending_unit = -1;

static int tile_explored(int player, int x, int y)
{
    uint16_t off = (uint16_t)(0x0032 * (uint16_t)x + (uint16_t)y + 0x7ff8);
    uint8_t vis = RB((uint16_t)VISSEG, off);
    return (vis & (1u << (player & 7))) != 0;
}

static int has_enemy_unit_at(int player, int x, int y)
{
    for (int p = 0; p < 8; p++) {
        if (p == player)
            continue;
        for (int u = 0; u < 128; u++) {
            uint16_t r = UNIT(p, u);
            if (GB(r + 3) != 0xff && GB(r + 1) == x && GB(r + 2) == y)
                return 1;
        }
    }
    return 0;
}

static int has_enemy_city_at(int player, int x, int y)
{
    for (int c = 0; c < 128; c++) {
        uint16_t r = CITY(c);
        if (GB(r + 6) != 0xff && GB(r + 11) != player &&
            GB(r + 4) == x && GB(r + 5) == y)
            return 1;
    }
    return 0;
}

static int has_own_city_at(int player, int x, int y)
{
    for (int c = 0; c < 128; c++) {
        uint16_t r = CITY(c);
        if (GB(r + 6) != 0xff && GB(r + 11) == player &&
            GB(r + 4) == x && GB(r + 5) == y)
            return 1;
    }
    return 0;
}

static int is_coastal(int x, int y)
{
    for (int d = 0; d < 8; d++) {
        int nx = wrapx(x + dir_dx[d]), ny = y + dir_dy[d];
        if (ny >= 0 && ny < 50 && terrain(nx, ny) != 10)
            return 1;
    }
    return 0;
}

static int explore_passable(int player, int ut, int x, int y)
{
    if (y < 0 || y >= 50)
        return 0;
    if (has_enemy_unit_at(player, x, y) || has_enemy_city_at(player, x, y))
        return 0;
    int t = terrain(x, y);
    if (ut <= 13 || ut >= 26)       /* land */
        return t != 10;
    if (ut >= 16 && ut <= 24) {     /* naval */
        if (has_own_city_at(player, x, y))
            return 1;
        if (t != 10)
            return 0;
        if (ut == 16 && !is_coastal(x, y))
            return 0;               /* keep Triremes near coast */
        return 1;
    }
    return 0;
}

static int count_unexplored_around(int player, int x, int y)
{
    int cnt = 0;
    for (int dy = -1; dy <= 1; dy++) {
        int ny = y + dy;
        if (ny < 0 || ny >= 50)
            continue;
        for (int dx = -1; dx <= 1; dx++) {
            int nx = wrapx(x + dx);
            if (!tile_explored(player, nx, ny))
                cnt++;
        }
    }
    return cnt;
}

static uint16_t explore_next_key(struct explore_job *e, int player, int ut, int ux, int uy)
{
    /* Stop if an enemy unit is adjacent so the player can decide */
    for (int d = 0; d < 8; d++) {
        int nx = wrapx(ux + dir_dx[d]), ny = uy + dir_dy[d];
        if (ny >= 0 && ny < 50 && has_enemy_unit_at(player, nx, ny)) {
            e->active = 0;
            ovl_msg("Auto-Explore: enemy spotted!");
            return 0;
        }
    }
    if (e->last_x == ux && e->last_y == uy) {
        if (++e->stuck >= 2) {
            e->active = 0;
            ovl_msg("Auto-Explore: blocked");
            return 0;
        }
    } else {
        e->stuck = 0;
    }
    int prev_x = e->last_x, prev_y = e->last_y;
    e->last_x = (uint8_t)ux;
    e->last_y = (uint8_t)uy;

    /* 1. Prefer an immediate neighbor that reveals the most fog of war */
    int best_d = -1, best_score = 0;
    for (int d = 0; d < 8; d++) {
        int nx = wrapx(ux + dir_dx[d]), ny = uy + dir_dy[d];
        if (!explore_passable(player, ut, nx, ny))
            continue;
        int fog = count_unexplored_around(player, nx, ny);
        if (fog <= 0)
            continue;
        int score = fog * 10;
        if (nx == prev_x && ny == prev_y)
            score -= 5;
        if (score > best_score) {
            best_score = score;
            best_d = d;
        }
    }
    if (best_d >= 0)
        return dir_key[best_d];

    /* 2. BFS across explored passable tiles to the nearest fog frontier */
    int mw = MAP_W;
    if (mw > 160)
        mw = 160;
    memset(bfs_vis, 0, (size_t)mw * 50);
    int head = 0, tail = 0;
    int start = uy * mw + ux;
    bfs_vis[start] = 1;
    bfs_q[tail++] = (uint16_t)start;

    while (head < tail) {
        int cur = bfs_q[head++];
        int cx = cur % mw, cy = cur / mw;
        for (int d = 0; d < 8; d++) {
            int nx = wrapx(cx + dir_dx[d]), ny = cy + dir_dy[d];
            if (ny < 0 || ny >= 50)
                continue;
            int idx = ny * mw + nx;
            if (bfs_vis[idx])
                continue;
            if (!tile_explored(player, nx, ny))
                continue;
            if (!explore_passable(player, ut, nx, ny))
                continue;
            uint8_t fdir = (cur == start) ? (uint8_t)d : bfs_fdir[cur];
            if (count_unexplored_around(player, nx, ny) > 0)
                return dir_key[fdir];
            bfs_vis[idx] = 1;
            bfs_fdir[idx] = fdir;
            bfs_q[tail++] = (uint16_t)idx;
        }
    }
    e->active = 0;
    ovl_msg("Auto-Explore: area fully explored");
    return 0;
}

/* Full-map BFS Goto step for human units (fixes long-range off-screen Goto on
   160-wide maps where Civ1's 4x4 block pathfinder is disabled).
   Returns Civ1 direction 1..8, 0 if arrived, or -1 to fall back to Civ1. */
int native_goto_step(int player, int unit)
{
    if (player != (int16_t)GW(0xe710) || unit < 0 || unit >= 128)
        return -1;
    uint16_t r = UNIT(player, unit);
    uint8_t ut = GB(r + 3);
    if (ut == 0xff || ut == 14 || ut == 15 || ut == 25)
        return -1;
    int gx = GB(r + 6), gy = GB(r + 7);
    if (gx == 0xff || gy >= 50)
        return -1;
    int ux = GB(r + 1), uy = GB(r + 2);
    if (ux == gx && uy == gy) {
        WB(DS, (uint16_t)(r + 6), 0xff);
        return 0;
    }

    int mw = MAP_W;
    if (mw > 160)
        mw = 160;
    memset(bfs_vis, 0, (size_t)mw * 50);
    int head = 0, tail = 0;
    int start = uy * mw + ux;
    bfs_vis[start] = 1;
    bfs_q[tail++] = (uint16_t)start;

    int found_d = -1;
    while (head < tail && found_d < 0) {
        int cur = bfs_q[head++];
        int cx = cur % mw, cy = cur / mw;
        for (int d = 0; d < 8; d++) {
            int nx = wrapx(cx + dir_dx[d]), ny = cy + dir_dy[d];
            if (ny < 0 || ny >= 50)
                continue;
            int idx = ny * mw + nx;
            if (bfs_vis[idx])
                continue;
            uint8_t fdir = (cur == start) ? (uint8_t)d : bfs_fdir[cur];
            if (nx == gx && ny == gy) {
                /* Verify destination tile domain if explored */
                if (tile_explored(player, nx, ny)) {
                    int t = terrain(nx, ny);
                    if ((ut <= 13 || ut >= 26) && t == 10)
                        continue;
                    if (ut >= 16 && ut <= 24 && t != 10 && !(improvements(nx, ny) & IMP_CITY))
                        continue;
                }
                found_d = fdir;
                break;
            }
            if (tile_explored(player, nx, ny)) {
                int t = terrain(nx, ny);
                if (ut <= 13 || ut >= 26) {
                    if (t == 10)
                        continue;
                } else {
                    if (t != 10 && !has_own_city_at(player, nx, ny))
                        continue;
                }
                if (has_enemy_unit_at(player, nx, ny) || has_enemy_city_at(player, nx, ny))
                    continue;
            }
            bfs_vis[idx] = 1;
            bfs_fdir[idx] = fdir;
            bfs_q[tail++] = (uint16_t)idx;
        }
    }
    if (found_d < 0)
        return -1;

    /* Map (dir_dx, dir_dy) to Civ1's 1..8 direction index (DS:0x1882 / 0x18e4) */
    int civ_dir = -1;
    for (int k = 1; k <= 8; k++) {
        if ((int16_t)RW(DS, (uint16_t)(0x1882 + 2 * k)) == dir_dx[found_d] &&
            (int16_t)RW(DS, (uint16_t)(0x18e4 + 2 * k)) == dir_dy[found_d]) {
            civ_dir = k;
            break;
        }
    }
    if (civ_dir < 0)
        return -1;
    if (wrapx(ux + dir_dx[found_d]) == gx && uy + dir_dy[found_d] == gy)
        WB(DS, (uint16_t)(r + 6), 0xff);
    else
        WB(DS, (uint16_t)(r + 8), (uint8_t)civ_dir);
    return civ_dir;
}

int roadto_picking(void)
{
    if (roadto_pick_unit < 0 || roadto_pick_unit >= 128 || city_screen_city() >= 0) {
        roadto_pick_unit = -1;
        return 0;
    }
    int player = (int16_t)GW(0xe710);
    int turn = (int16_t)GW(0x90f2);
    if (player != roadto_pick_player || turn != roadto_pick_turn) {
        roadto_pick_unit = -1;
        return 0;
    }
    uint16_t r = UNIT(player, roadto_pick_unit);
    if (GB(r + 3) != 0) {
        roadto_pick_unit = -1;
        return 0;
    }
    return 1;
}

void roadto_cancel_pick(void)
{
    if (roadto_pick_unit >= 0) {
        roadto_pick_unit = -1;
        ovl_msg("Road To cancelled");
    }
}

int roadto_click(int x, int y)
{
    if (!roadto_picking())
        return 0;
    if (x < 80 || y < 8)
        return 0;
    int tx = wrapx((int)VIEW_X + (x - 80) / 16);
    int ty = (int)VIEW_Y + (y - 8) / 16;
    if (ty < 0 || ty >= 50)
        return 1;
    int player = roadto_pick_player;
    int unit = roadto_pick_unit;
    if (tile_explored(player, tx, ty) && terrain(tx, ty) == 10) {
        ovl_msg("Cannot build road on ocean");
        return 1;
    }
    jobs[unit].active = 0;
    struct roadto_job *rj = &rjobs[unit];
    memset(rj, 0, sizeof *rj);
    rj->active = 1;
    rj->player = (uint8_t)player;
    rj->gx = (uint8_t)tx;
    rj->gy = (uint8_t)ty;
    rj->last_x = 0xff;
    rj->last_y = 0xff;
    rj->skip_x = 0xff;
    rj->skip_y = 0xff;
    roadto_pick_unit = -1;
    ovl_msg("Settlers: building road to (%d,%d)", tx, ty);
    /* Wake the order loop so the Settler starts right away */
    bios_key_push(0xFFFE);
    return 1;
}

static uint16_t roadto_next_key(struct roadto_job *rj, int player, int ux, int uy)
{
    /* Stop if an enemy unit is adjacent */
    for (int d = 0; d < 8; d++) {
        int nx = wrapx(ux + dir_dx[d]), ny = uy + dir_dy[d];
        if (ny >= 0 && ny < 50 && has_enemy_unit_at(player, nx, ny)) {
            rj->active = 0;
            ovl_msg("Road To: enemy spotted!");
            return 0;
        }
    }
    int imp = improvements(ux, uy);
    /* If we tried building a road here last step and it's still not built, skip roading this tile */
    if (rj->tried_road) {
        rj->tried_road = 0;
        if (rj->last_x == ux && rj->last_y == uy && !(imp & (IMP_ROAD | IMP_CITY))) {
            rj->skip_x = (uint8_t)ux;
            rj->skip_y = (uint8_t)uy;
        }
    }
    /* Build road on current tile if needed and not skipped */
    if (!(imp & (IMP_ROAD | IMP_CITY)) && !(rj->skip_x == ux && rj->skip_y == uy)) {
        rj->last_x = (uint8_t)ux;
        rj->last_y = (uint8_t)uy;
        rj->tried_road = 1;
        rj->stuck = 0;
        return 'r';
    }
    /* Arrived at destination and road is done */
    if (ux == rj->gx && uy == rj->gy) {
        rj->active = 0;
        ovl_msg("Settlers: road completed!");
        return 0;
    }
    if (rj->last_x == ux && rj->last_y == uy) {
        if (++rj->stuck >= 2) {
            rj->active = 0;
            ovl_msg("Road To: blocked");
            return 0;
        }
    } else {
        rj->stuck = 0;
    }
    rj->last_x = (uint8_t)ux;
    rj->last_y = (uint8_t)uy;

    /* BFS over land tiles to (gx, gy) */
    int mw = MAP_W;
    if (mw > 160)
        mw = 160;
    memset(bfs_vis, 0, (size_t)mw * 50);
    int head = 0, tail = 0;
    int start = uy * mw + ux;
    bfs_vis[start] = 1;
    bfs_q[tail++] = (uint16_t)start;

    int found_d = -1;
    while (head < tail && found_d < 0) {
        int cur = bfs_q[head++];
        int cx = cur % mw, cy = cur / mw;
        for (int d = 0; d < 8; d++) {
            int nx = wrapx(cx + dir_dx[d]), ny = cy + dir_dy[d];
            if (ny < 0 || ny >= 50)
                continue;
            int idx = ny * mw + nx;
            if (bfs_vis[idx])
                continue;
            uint8_t fdir = (cur == start) ? (uint8_t)d : bfs_fdir[cur];
            if (tile_explored(player, nx, ny)) {
                if (terrain(nx, ny) == 10)
                    continue;
                if (has_enemy_unit_at(player, nx, ny) || has_enemy_city_at(player, nx, ny))
                    continue;
            }
            if (nx == rj->gx && ny == rj->gy) {
                found_d = fdir;
                break;
            }
            bfs_vis[idx] = 1;
            bfs_fdir[idx] = fdir;
            bfs_q[tail++] = (uint16_t)idx;
        }
    }
    if (found_d < 0) {
        rj->active = 0;
        ovl_msg("Road To: no land path");
        return 0;
    }
    return dir_key[found_d];
}

/* BIOS keyboard word for an order key */
static uint16_t bios_word(uint16_t key)
{
    if (key == 'r') return 0x1372;
    if (key == 'i') return 0x1769;
    return key;                     /* direction keys are scan << 8 */
}

/* hook before 0402:0645: the active unit is about to wait for orders */
void hook_unit_await(void)
{
    int player = (int16_t)RW(SS, (uint16_t)(BP + 6));
    if (player == (int16_t)GW(0xe710))
        realtime_order_wait();
    int unit = (int16_t)RW(SS, (uint16_t)(BP - 0x32));
    if (player != (int16_t)GW(0xe710) || unit < 0 || unit >= 128)
        return;
    if (pending_key)
        return;
    struct job *j = &jobs[unit];
    if (j->active) {
        uint16_t u = UNIT(player, unit);
        if (j->player != player || j->type != GB(u + 3)) {
            j->active = 0;          /* the slot now holds another unit */
            return;
        }
        uint16_t k = next_key(j, GB(u + 1), GB(u + 2));
        if (k) {
            pending_key = k;
            pending_unit = unit;
            bios_key_push(bios_word(k));
        }
        return;
    }
    struct roadto_job *rj = &rjobs[unit];
    if (rj->active) {
        uint16_t u = UNIT(player, unit);
        if (rj->player != player || GB(u + 3) != 0) {
            rj->active = 0;
            return;
        }
        uint16_t k = roadto_next_key(rj, player, GB(u + 1), GB(u + 2));
        if (k) {
            pending_key = k;
            pending_unit = unit;
            bios_key_push(bios_word(k));
        }
        return;
    }
    struct explore_job *e = &ejobs[unit];
    if (e->active) {
        uint16_t u = UNIT(player, unit);
        uint8_t ut = GB(u + 3);
        if (e->player != player || e->type != ut) {
            e->active = 0;
            return;
        }
        uint16_t k = explore_next_key(e, player, ut, GB(u + 1), GB(u + 2));
        if (k) {
            pending_key = k;
            pending_unit = unit;
            bios_key_push(bios_word(k));
        }
    }
}

/* hook before 0402:0677 "mov [bp-0x44], ax": AX = key read for the
   active unit (0 = none) */
void hook_unit_order_key(void)
{
    int player = (int16_t)RW(SS, (uint16_t)(BP + 6));
    int unit = (int16_t)RW(SS, (uint16_t)(BP - 0x32));
    if (player != (int16_t)GW(0xe710) || unit < 0 || unit >= 128)
        return;
    if (mapnav_is_marker(AX)) {
        AX = 0;                 /* view changed: just start over */
        realtime_apply_select();
        return;
    }
    uint16_t u = UNIT(player, unit);
    int type = GB(u + 3);
    struct job *j = &jobs[unit];
    struct explore_job *e = &ejobs[unit];
    if (pending_key) {
        int ours = AX == pending_key && unit == pending_unit;
        pending_key = 0;
        if (ours)
            return;             /* our order: let the game execute it */
    }
    if (AX == 'a' || AX == 'A') {
        if (type == 0) {        /* Settlers */
            rjobs[unit].active = 0;
            roadto_pick_unit = -1;
            start(player, unit, GB(u + 1), GB(u + 2));
            j->type = (uint8_t)type;
            AX = 0;
        }
        return;
    }
    if (AX == 'R') {            /* Shift+R (or ROAD button): Settlers Road-To */
        if (type == 0) {
            roadto_pick_unit = unit;
            roadto_pick_player = player;
            roadto_pick_turn = (int16_t)GW(0x90f2);
            ovl_msg("Road To: click destination tile");
            AX = 0;
        }
        return;
    }
    if (AX == 'G') {            /* Shift+G (or EXPL button): Auto-Explore */
        if (type != 0 && type != 14 && type != 15 && type != 25) {
            memset(e, 0, sizeof *e);
            e->active = 1;
            e->player = (uint8_t)player;
            e->type = (uint8_t)type;
            e->last_x = 0xff;
            e->last_y = 0xff;
            ovl_msg("Auto-Explore: ON (Shift+G)");
            AX = 0;
        }
        return;
    }
    if (j->active && AX != 0) {
        j->active = 0;
        ovl_msg("Settlers: automation cancelled");
    }
    if (rjobs[unit].active && AX != 0) {
        rjobs[unit].active = 0;
        ovl_msg("Road To cancelled");
    }
    if (e->active && AX != 0) {
        e->active = 0;
        ovl_msg("Auto-Explore cancelled");
    }
}

int autowork_cancel(int unit)
{
    int cancelled = 0;
    if (unit >= 0 && unit < 128 && jobs[unit].active) {
        jobs[unit].active = 0;
        ovl_msg("Settlers: automation cancelled");
        cancelled = 1;
    }
    if (unit >= 0 && unit < 128 && rjobs[unit].active) {
        rjobs[unit].active = 0;
        if (!cancelled)
            ovl_msg("Road To cancelled");
        cancelled = 1;
    }
    if (unit >= 0 && unit < 128 && ejobs[unit].active) {
        ejobs[unit].active = 0;
        if (!cancelled)
            ovl_msg("Auto-Explore cancelled");
        cancelled = 1;
    }
    return cancelled;
}

void autowork_save(void *fp)
{
    FILE *f = fp;
    fwrite(jobs, sizeof jobs, 1, f);
}

void autowork_load(void *fp)
{
    FILE *f = fp;
    memset(ejobs, 0, sizeof ejobs);
    memset(rjobs, 0, sizeof rjobs);
    roadto_pick_unit = -1;
    if (fread(jobs, sizeof jobs, 1, f) != 1)
        memset(jobs, 0, sizeof jobs);
}
