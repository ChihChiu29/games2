#include "game.h"
#include <string.h>

/* Convert local (fwd, right) relative to entity center & facing into world (wx, wy) */
static void local_to_world(const Entity *e, int fwd, int right, int *wx, int *wy)
{
    int dx = 0, dy = 0;
    switch (e->dir) {
    case DIR_UP:
        dx = right;
        dy = -fwd;
        break;
    case DIR_DOWN:
        dx = -right;
        dy = fwd;
        break;
    case DIR_RIGHT:
        dx = fwd;
        dy = right;
        break;
    case DIR_LEFT:
        dx = -fwd;
        dy = -right;
        break;
    }
    *wx = e->cx + dx;
    *wy = e->cy + dy;
}

/* Rotate directional ASCII character from DIR_UP reference into entity's facing direction */
static char rot_char(char c, Direction dir)
{
    if (dir == DIR_UP) return c;

    if (dir == DIR_DOWN) {
        switch (c) {
        case '^': return 'v';
        case 'v': return '^';
        case '<': return '>';
        case '>': return '<';
        case '[': return ']';
        case ']': return '[';
        case 'A': return 'V';
        default:  return c;
        }
    }

    if (dir == DIR_RIGHT) {
        switch (c) {
        case '^': return '>';
        case '>': return 'v';
        case 'v': return '<';
        case '<': return '^';
        case '|': return '-';
        case '-': return '|';
        case '/': return '\\';
        case '\\': return '/';
        case '[': return '=';
        case ']': return '=';
        case 'A': return '>';
        default:  return c;
        }
    }

    if (dir == DIR_LEFT) {
        switch (c) {
        case '^': return '<';
        case '<': return 'v';
        case 'v': return '>';
        case '>': return '^';
        case '|': return '-';
        case '-': return '|';
        case '/': return '\\';
        case '\\': return '/';
        case '[': return '=';
        case ']': return '=';
        case 'A': return '<';
        default:  return c;
        }
    }
    return c;
}

static void add_hitbox_local(Entity *e, int fwd, int right, char base_ch, uint8_t attr)
{
    if (e->num_hitbox >= MAX_HITBOX_CELLS) return;
    int wx, wy;
    local_to_world(e, fwd, right, &wx, &wy);
    if (wx <= ARENA_LEFT || wx >= ARENA_RIGHT || wy <= ARENA_TOP || wy >= ARENA_BOTTOM) return;
    e->hitbox[e->num_hitbox].x = wx;
    e->hitbox[e->num_hitbox].y = wy;
    e->hitbox[e->num_hitbox].ch = rot_char(base_ch, e->dir);
    e->hitbox[e->num_hitbox].attr = attr;
    e->num_hitbox++;
}

static void add_shield_local(Entity *e, int fwd, int right, char base_ch, uint8_t attr)
{
    if (e->num_shield >= 16) return;
    int wx, wy;
    local_to_world(e, fwd, right, &wx, &wy);
    if (wx <= ARENA_LEFT || wx >= ARENA_RIGHT || wy <= ARENA_TOP || wy >= ARENA_BOTTOM) return;
    e->shield[e->num_shield].x = wx;
    e->shield[e->num_shield].y = wy;
    e->shield[e->num_shield].ch = rot_char(base_ch, e->dir);
    e->shield[e->num_shield].attr = attr;
    e->num_shield++;
}

static void put_local(const Entity *e, int fwd, int right, char base_ch, uint8_t attr)
{
    int wx, wy;
    local_to_world(e, fwd, right, &wx, &wy);
    if (wx <= ARENA_LEFT || wx >= ARENA_RIGHT || wy <= ARENA_TOP || wy >= ARENA_BOTTOM) return;
    platform_put_cell(wx, wy, rot_char(base_ch, e->dir), attr);
}

static const AttackDef *get_current_attack_def(const Entity *e)
{
    if (e->action != ACT_ATTACK) return NULL;
    if (e->is_player) {
        return (e->attack_idx == 1) ? &player_cfg.heavy_atk : &player_cfg.light_atk;
    } else {
        if (e->type_idx < 0 || e->type_idx >= num_enemy_types) return NULL;
        const EnemyType *et = &enemy_types[e->type_idx];
        if (e->attack_idx < 0 || e->attack_idx >= et->num_attacks) return NULL;
        return &et->attacks[e->attack_idx];
    }
}

void build_entity_hitboxes(Entity *e)
{
    int i, r;
    e->num_hitbox = 0;
    e->num_shield = 0;
    if (!e->active) return;

    /* 1. Player Left-Hand Shield Barrier when Blocking / Parrying */
    if (e->is_player && e->action == ACT_BLOCK) {
        bool is_parry = (e->block_hold_frames <= player_cfg.parry_window_frames);
        uint8_t sh_attr = is_parry
            ? ATTR(COL_YELLOW, COL_CYAN)
            : ATTR(COL_LIGHTCYAN, COL_BLUE);
        char edge_ch = is_parry ? '*' : '[';
        char mid_ch  = is_parry ? '#' : '=';

        for (r = -2; r <= 2; r++) {
            char c = (r == -2 || r == 2) ? edge_ch : mid_ch;
            add_shield_local(e, 2, r, c, sh_attr);
            add_shield_local(e, 3, r, c, sh_attr);
        }
    }

    /* 2. Data-driven Active Weapon Hitboxes */
    const AttackDef *atk = get_current_attack_def(e);
    if (atk) {
        int active_start = atk->windup_frames + 1;
        int active_end   = atk->windup_frames + atk->active_frames;
        if (e->action_frame >= active_start && e->action_frame <= active_end) {
            int rel = e->action_frame - active_start;
            int accum = 0;
            for (i = 0; i < atk->num_phases; i++) {
                const AttackPhase *ph = &atk->phases[i];
                if (rel < accum + ph->duration_frames) {
                    int c;
                    for (c = 0; c < ph->num_cells; c++) {
                        add_hitbox_local(e, ph->cells[c].fwd, ph->cells[c].right,
                                         ph->cells[c].ch, ph->cells[c].attr);
                    }
                    break;
                }
                accum += ph->duration_frames;
            }
        }
    }
}

static void draw_cell_list(const Entity *e, const SpriteCell *cells, int count, int8_t override_attr)
{
    int i;
    for (i = 0; i < count; i++) {
        uint8_t at = (override_attr >= 0) ? (uint8_t)override_attr : cells[i].attr;
        put_local(e, cells[i].fwd, cells[i].right, cells[i].ch, at);
    }
}

void draw_entity_sprite(const Entity *e)
{
    int i, f, r;
    if (!e->active) return;

    if (e->is_player) {
        /* Roll Animation */
        if (e->action == ACT_ROLL) {
            bool iframe = (e->action_frame <= player_cfg.roll_iframes);
            uint8_t roll_attr = iframe
                ? ATTR(COL_LIGHTGREEN, COL_BLUE)
                : ATTR(COL_LIGHTCYAN, COL_BLACK);
            char spin = (e->action_frame & 2) ? '/' : '\\';
            for (f = -1; f <= 1; f++) {
                for (r = -1; r <= 1; r++) {
                    put_local(e, f, r, (f == 0 && r == 0) ? '@' : spin, roll_attr);
                }
            }
            put_local(e,  2,  0, '^', ATTR(COL_YELLOW, COL_BLUE));
            put_local(e, -2,  0, '.', roll_attr);
            put_local(e,  0, -2, '(', roll_attr);
            put_local(e,  0,  2, ')', roll_attr);
            return;
        }

        int8_t body_override = -1;
        if (e->hurt_flash > 0) body_override = (int8_t)ATTR(COL_WHITE, COL_RED);
        else if (e->action == ACT_STAGGER) body_override = (int8_t)ATTR(COL_YELLOW, COL_BROWN);

        draw_cell_list(e, player_cfg.body_cells, player_cfg.num_body_cells, body_override);

        if (e->action != ACT_BLOCK) {
            draw_cell_list(e, player_cfg.idle_shield, player_cfg.num_idle_shield, -1);
        }

        const AttackDef *atk = get_current_attack_def(e);
        if (atk) {
            if (e->action_frame <= atk->windup_frames) {
                draw_cell_list(e, atk->windup_cells, atk->num_windup_cells, -1);
            } else if (e->action_frame > atk->windup_frames + atk->active_frames) {
                draw_cell_list(e, atk->recovery_cells, atk->num_recovery_cells, -1);
            }
        } else {
            draw_cell_list(e, player_cfg.idle_weapon, player_cfg.num_idle_weapon, -1);
        }
    } else {
        if (e->type_idx < 0 || e->type_idx >= num_enemy_types) return;
        const EnemyType *et = &enemy_types[e->type_idx];

        int8_t body_override = -1;
        if (e->hurt_flash > 0) {
            body_override = (int8_t)ATTR(COL_WHITE, COL_RED);
        } else if (e->action == ACT_STAGGER) {
            body_override = (int8_t)ATTR(COL_LIGHTCYAN, COL_BLUE);
        }

        draw_cell_list(e, et->body_cells, et->num_body_cells, body_override);

        if (e->action == ACT_STAGGER) {
            draw_cell_list(e, et->stagger_cells, et->num_stagger_cells, -1);
        } else {
            const AttackDef *atk = get_current_attack_def(e);
            if (atk) {
                if (e->action_frame <= atk->windup_frames) {
                    int8_t flash_override = -1;
                    if (e->action_frame > atk->windup_frames - 8 && (e->action_frame & 1)) {
                        flash_override = (int8_t)ATTR(COL_WHITE, COL_RED);
                    }
                    draw_cell_list(e, atk->windup_cells, atk->num_windup_cells, flash_override);
                } else if (e->action_frame > atk->windup_frames + atk->active_frames) {
                    draw_cell_list(e, atk->recovery_cells, atk->num_recovery_cells, -1);
                }
            } else {
                draw_cell_list(e, et->idle_weapon, et->num_idle_weapon, -1);
            }
        }
    }

    /* Draw Active Shield Barrier */
    for (i = 0; i < e->num_shield; i++) {
        platform_put_cell(e->shield[i].x, e->shield[i].y, e->shield[i].ch, e->shield[i].attr);
    }

    /* Draw Active Weapon Hitbox Cells */
    for (i = 0; i < e->num_hitbox; i++) {
        platform_put_cell(e->hitbox[i].x, e->hitbox[i].y, e->hitbox[i].ch, e->hitbox[i].attr);
    }
}
