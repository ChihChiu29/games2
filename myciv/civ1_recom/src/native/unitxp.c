/*
 * Unit XP, Level-Up & Hero Ascension (both human and AI units):
 *   - Every 3 combat wins: +1 Level and +1 to a random stat (Atk, Def, or Mov).
 *   - Every 3 Levels (Lv3, Lv6, Lv9...): Hero Ascension!
 *     Automatically gains Veteran status plus one random Hero perk:
 *       (a) Remove support need (home city = NONE)
 *       (b) +1 random stat to self and all friendly units within 1 tile (3x3)
 *       (c) +1 random stat on self x3
 *       (d) +1 Extra Life (survives a lethal combat blow)
 *
 * State is stored per (player, unit_slot) and persisted in CIVILn.NAT.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native.h"
#include "runtime.h"
#include "addons.h"

#define UNIT(p, u) (0x90f4 + 0x600 * (p) + 12 * (u))
#define UNIT_FLAGS 0
#define UNIT_X 1
#define UNIT_Y 2
#define UNIT_TYPE 3
#define UNIT_MOVES 4
#define UNIT_HOME 11

#define MAX_MOV_BONUS 3

struct unit_xp {
    uint8_t active;
    uint8_t type;
    uint8_t wins;
    uint8_t level;
    uint8_t atk;
    uint8_t def;
    uint8_t mov;
    uint8_t lives;
    uint8_t hero;
};

static struct unit_xp xp[8][128];
static uint32_t rng_state = 0x19910475u;
static int last_xp_turn = -1;

static void check_new_game_reset(void)
{
    int turn = (int16_t)GW(0x90f2);
    if (turn == 0 && last_xp_turn > 0)
        memset(xp, 0, sizeof xp);
    last_xp_turn = turn;
}

static uint32_t xp_rand(void)
{
    uint32_t x = rng_state ^ (uint32_t)now_us();
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x ? x : 0x19910475u;
    return rng_state;
}

static const char *unit_name(uint8_t t)
{
    static const char *names[28] = {
        "Settlers", "Militia", "Phalanx", "Legion", "Musketeers", "Riflemen",
        "Cavalry", "Knights", "Catapult", "Cannon", "Chariot", "Armor",
        "Mech. Inf.", "Artillery", "Fighter", "Bomber", "Trireme", "Sail",
        "Frigate", "Ironclad", "Cruiser", "Battleship", "Submarine", "Carrier",
        "Transport", "Nuclear", "Diplomat", "Caravan"
    };
    return t < 28 ? names[t] : "Unit";
}

static struct unit_xp *get_xp(int p, int u)
{
    check_new_game_reset();
    if (p < 0 || p >= 8 || u < 0 || u >= 128)
        return NULL;
    uint8_t t = GB(UNIT(p, u) + UNIT_TYPE);
    struct unit_xp *e = &xp[p][u];
    if (t == 0xff) {
        if (e->active)
            memset(e, 0, sizeof *e);
        return NULL;
    }
    if (!e->active || e->type != t) {
        memset(e, 0, sizeof *e);
        e->active = 1;
        e->type = t;
    }
    return e;
}

int unit_xp_atk(int p, int u)
{
    struct unit_xp *e = get_xp(p, u);
    return e ? (int)e->atk : 0;
}

int unit_xp_def(int p, int u)
{
    struct unit_xp *e = get_xp(p, u);
    return e ? (int)e->def : 0;
}

int unit_xp_mov(int p, int u)
{
    struct unit_xp *e = get_xp(p, u);
    return e ? (int)e->mov : 0;
}

void unit_xp_clear(int p, int u)
{
    if (p >= 0 && p < 8 && u >= 0 && u < 128)
        memset(&xp[p][u], 0, sizeof xp[p][u]);
}

/* Called right before a unit is destroyed in combat.
   Returns 1 if an Extra Life saved the unit (caller must skip destroying it). */
int unit_xp_consume_life(int p, int u)
{
    struct unit_xp *e = get_xp(p, u);
    if (!e || e->lives == 0)
        return 0;
    e->lives--;
    int human = (int16_t)GW(0xe710);
    if (p == human)
        ovl_msg("HERO %s survived! (%d lives left)", unit_name(e->type), e->lives);
    else
        ovl_msg("Enemy HERO %s survived!", unit_name(e->type));
    return 1;
}

static const char *add_random_stat(struct unit_xp *e, int stat_override)
{
    int can_atk = GW((uint16_t)(0x113e + 0x22 * e->type)) > 0;
    int can_mov = e->mov < MAX_MOV_BONUS;
    int s = stat_override;
    if (s < 0) {
        int pool[3], n = 0;
        if (can_atk) pool[n++] = 0;
        pool[n++] = 1;
        if (can_mov) pool[n++] = 2;
        s = pool[xp_rand() % (uint32_t)n];
    } else {
        if (s == 0 && !can_atk)
            s = 1;
        if (s == 2 && !can_mov)
            s = can_atk ? (int)(xp_rand() % 2u) : 1;
    }
    if (s == 0) {
        if (e->atk < 20) e->atk++;
        return "Atk";
    } else if (s == 1) {
        if (e->def < 20) e->def++;
        return "Def";
    } else {
        e->mov++;
        return "Mov";
    }
}

void unit_xp_on_win(int p, int u)
{
    struct unit_xp *e = get_xp(p, u);
    if (!e)
        return;
    if (e->wins < 255)
        e->wins++;
    if (e->wins % 3 != 0)
        return;

    /* Level up! */
    e->level++;
    const char *stat_name = add_random_stat(e, -1);
    int human = (int16_t)GW(0xe710);

    if (e->level % 3 != 0) {
        if (p == human)
            ovl_msg("%s Lv%d: +1 %s!", unit_name(e->type), e->level, stat_name);
        return;
    }

    /* Every 3rd level: HERO ASCENSION! */
    e->hero++;
    uint16_t r = UNIT(p, u);
    WB(DS, r + UNIT_FLAGS, GB(r + UNIT_FLAGS) | 0x20); /* auto-Veteran */

    /* Pick one of 4 Hero perks:
       0 = remove support need (skip if already NONE)
       1 = +1 random stat to self + friendly units within 1 tile (3x3)
       2 = +1 random stat on self x3
       3 = +1 extra life */
    int has_support = GB(r + UNIT_HOME) != 0xff;
    int perk;
    if (has_support)
        perk = (int)(xp_rand() % 4u);
    else
        perk = 1 + (int)(xp_rand() % 3u);

    if (perk == 0) {
        WB(DS, (uint16_t)(r + UNIT_HOME), 0xff);
        if (p == human)
            ovl_msg("HERO %s Lv%d: No Support!", unit_name(e->type), e->level);
    } else if (perk == 1) {
        int s = (int)(xp_rand() % 3u);
        int cx = GB(r + UNIT_X), cy = GB(r + UNIT_Y);
        const char *aname = (s == 0) ? "Atk" : (s == 1) ? "Def" : "Mov";
        for (int u2 = 0; u2 < 128; u2++) {
            struct unit_xp *e2 = get_xp(p, u2);
            if (!e2)
                continue;
            uint16_t r2 = UNIT(p, u2);
            int dx = abs((int)GB(r2 + UNIT_X) - cx);
            if (dx > MAP_W / 2)
                dx = MAP_W - dx;
            int dy = abs((int)GB(r2 + UNIT_Y) - cy);
            if (dx <= 1 && dy <= 1) {
                int s2 = s;
                /* Non-combat units (base Atk == 0) get Def instead of Atk */
                if (s2 == 0 && GW((uint16_t)(0x113e + 0x22 * e2->type)) == 0)
                    s2 = 1;
                add_random_stat(e2, s2);
            }
        }
        if (p == human)
            ovl_msg("HERO Lv%d Aura: +1 %s (3x3)!", e->level, aname);
    } else if (perk == 2) {
        add_random_stat(e, -1);
        add_random_stat(e, -1);
        add_random_stat(e, -1);
        if (p == human)
            ovl_msg("HERO %s Lv%d: +3 Stats!", unit_name(e->type), e->level);
    } else {
        if (e->lives < 9)
            e->lives++;
        if (p == human)
            ovl_msg("HERO %s Lv%d: +1 Life!", unit_name(e->type), e->level);
    }
}

/* Format a short status badge for the active unit (returns 1 if non-empty). */
int unit_xp_badge(int p, int u, char *buf, int bufsz, int *is_hero)
{
    struct unit_xp *e = get_xp(p, u);
    if (!e || (!e->wins && !e->level && !e->atk && !e->def && !e->mov && !e->lives && !e->hero))
        return 0;
    if (is_hero)
        *is_hero = e->hero > 0;
    if (e->level == 0 && !e->atk && !e->def && !e->mov && !e->lives) {
        snprintf(buf, (size_t)bufsz, "WINS %d/3", e->wins);
        return 1;
    }
    int n = 0;
    if (e->hero)
        n += snprintf(buf + n, (size_t)(bufsz - n), "H%d", e->level);
    else
        n += snprintf(buf + n, (size_t)(bufsz - n), "L%d", e->level);
    if ((e->atk || e->def || e->mov) && n < bufsz - 1)
        n += snprintf(buf + n, (size_t)(bufsz - n), " ");
    if (e->atk && n < bufsz - 1)
        n += snprintf(buf + n, (size_t)(bufsz - n), "%dA", e->atk);
    if (e->def && n < bufsz - 1)
        n += snprintf(buf + n, (size_t)(bufsz - n), "%dD", e->def);
    if (e->mov && n < bufsz - 1)
        n += snprintf(buf + n, (size_t)(bufsz - n), "%dM", e->mov);
    if (e->lives && n < bufsz - 1)
        n += snprintf(buf + n, (size_t)(bufsz - n), " %dL", e->lives);
    if (bufsz > 13)
        buf[12] = '\0';
    return 1;
}

void unit_xp_save(void *fp)
{
    FILE *f = fp;
    fwrite(xp, sizeof xp, 1, f);
}

void unit_xp_load(void *fp)
{
    FILE *f = fp;
    last_xp_turn = -1;
    if (!f || fread(xp, sizeof xp, 1, f) != 1)
        memset(xp, 0, sizeof xp);
}
