/*
 * "Produce gold": a city can turn its production into money instead of
 * building something.  Chosen as an entry of the production list, or
 * toggled on the city screen with the GOLD button or the G key.  Each turn the shields the city would add to its production
 * box go to the owner's treasury; the box does not fill, so nothing is
 * completed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native.h"
#include "runtime.h"
#include "addons.h"
#include "mp.h"

#define CITY(c) (0x706c + 0x1c * (c))     /* 28 byte city records */
#define CITY_X 4
#define CITY_Y 5
#define CITY_SIZE 7
#define CITY_OWNER 11
#define GOLD(p) (0xc0f6 + 2 * (p))        /* treasury of each player */
#define UNIT(p, u) (0x90f4 + 0x600 * (p) + 12 * (u))

static struct { uint8_t on, x, y, owner; } cap[128];
static struct { uint8_t on, x, y, owner; } gov[128];
static int screen_city = -1;

int city_screen_city(void)
{
    return screen_city;
}

int city_produces_gold(int c)
{
    if (c < 0 || c >= 128 || !cap[c].on)
        return 0;
    uint16_t r = CITY(c);
    /* the flag belongs to the city that was there when it was set */
    return cap[c].x == GB(r + CITY_X) && cap[c].y == GB(r + CITY_Y) &&
           cap[c].owner == GB(r + CITY_OWNER);
}

static void set_gold(int c, int on)
{
    uint16_t r = CITY(c);
    cap[c].on = (uint8_t)on;
    cap[c].x = GB(r + CITY_X);
    cap[c].y = GB(r + CITY_Y);
    cap[c].owner = GB(r + CITY_OWNER);
    ovl_msg(on ? "Production goes to the treasury" : "City builds again");
}

/* without a message (multiplayer: the other player's settings) */
void city_set_gold(int c, int on)
{
    if (c < 0 || c >= 128 || city_produces_gold(c) == !!on)
        return;
    uint16_t r = CITY(c);
    cap[c].on = (uint8_t)!!on;
    cap[c].x = GB(r + CITY_X);
    cap[c].y = GB(r + CITY_Y);
    cap[c].owner = GB(r + CITY_OWNER);
}

static void toggle(int c)
{
    set_gold(c, !city_produces_gold(c));
}

int city_has_governor(int c)
{
    if (c < 0 || c >= 128 || !gov[c].on)
        return 0;
    uint16_t r = CITY(c);
    return gov[c].x == GB(r + CITY_X) && gov[c].y == GB(r + CITY_Y) &&
           gov[c].owner == GB(r + CITY_OWNER);
}

static void toggle_gov(int c)
{
    int on = !city_has_governor(c);
    uint16_t r = CITY(c);
    gov[c].on = (uint8_t)on;
    gov[c].x = GB(r + CITY_X);
    gov[c].y = GB(r + CITY_Y);
    gov[c].owner = GB(r + CITY_OWNER);
    ovl_msg(on ? "City Governor: ON (Development)" : "City Governor: OFF");
}

/* ---- "Gold" in the "What shall we build" list (f_0ad5_041c) ----
   The list is text at DS:C926 ("title?\n item (...)\n item ...") plus the
   item ids in a word array at BP-0xDE (entry 0 = current build, entry i =
   line i), count+1 at BP-0xA.  The choice comes back as an index. */
#define MENU_TEXT 0xc926
#define GOLD_ITEM 0x70                  /* unused item id */
static int menu_city = -1;

int city_menu_open(void)
{
    return menu_city >= 0;
}

void hook_build_menu(void)              /* 0ad5:1433, human player's list */
{
    int city = (int16_t)RW(SS, (uint16_t)(BP + 8));
    menu_city = -1;
    if (getenv("CIV_ORIGINAL") || city < 0 || city >= 128)
        return;
    int len = 0, title = -1;
    while (len < 2000 && GB(MENU_TEXT + len)) {
        if (title < 0 && GB(MENU_TEXT + len) == '\n')
            title = len + 2;            /* after "?\n " */
        len++;
    }
    const char *line = city_produces_gold(city) ? "GOLD (now): production to treasury\n "
                                                : "Gold: production to treasury\n ";
    int n = (int)strlen(line);
    int count = (int16_t)RW(SS, (uint16_t)(BP - 0xa));
    if (title < 0 || title > len || len + n > 1400 || count < 1 || count > 60)
        return;
    for (int i = len; i >= title; i--)  /* includes the terminating 0 */
        WB(DS, (uint16_t)(MENU_TEXT + i + n), GB(MENU_TEXT + i));
    for (int i = 0; i < n; i++)
        WB(DS, (uint16_t)(MENU_TEXT + title + i), (uint8_t)line[i]);
    uint16_t list = (uint16_t)(BP - 0xde);
    for (int i = count - 1; i >= 1; i--)
        WW(SS, (uint16_t)(list + 2 * (i + 1)), RW(SS, (uint16_t)(list + 2 * i)));
    WW(SS, (uint16_t)(list + 2), GOLD_ITEM);
    WW(SS, (uint16_t)(BP - 0xa), (uint16_t)(count + 1));
    menu_city = city;
}

static int list_has(uint16_t list, int count, int16_t item)
{
    for (int i = 1; i < count; i++)
        if ((int16_t)RW(SS, (uint16_t)(list + 2 * i)) == item)
            return 1;
    return 0;
}

static const char *gov_item_name(int16_t item)
{
    switch (item) {
    case 1: return "Militia";
    case 2: return "Phalanx";
    case 4: return "Musketeers";
    case 5: return "Riflemen";
    case 12: return "Mech. Inf.";
    case -3: return "Granary";
    case -4: return "Temple";
    case -5: return "Marketplace";
    case -6: return "Library";
    case -7: return "Courthouse";
    case -8: return "City Walls";
    case -9: return "Aqueduct";
    case -10: return "Bank";
    case -11: return "Cathedral";
    case -12: return "University";
    case -13: return "Mass Transit";
    case -14: return "Colosseum";
    case -15: return "Factory";
    case -16: return "Mfg. Plant";
    case -18: return "Recycling";
    case -19: return "Power Plant";
    case -20: return "Hydro Plant";
    default: return "Item";
    }
}

/* Called at 0ad5:1433 right before showing "What shall we build?" dialog.
   If Governor is ON for this city (and we're not inside the city screen's
   manual Change dialog), pick the next development item automatically. */
int governor_pick_build(int16_t *out)
{
    int city = (int16_t)RW(SS, (uint16_t)(BP + 8));
    if (screen_city >= 0 || !city_has_governor(city))
        return 0;
    int count = (int16_t)RW(SS, (uint16_t)(BP - 0xa));
    uint16_t list = (uint16_t)(BP - 0xde);
    uint16_t r = CITY(city);
    int player = GB(r + CITY_OWNER) & 7;
    int cx = GB(r + CITY_X), cy = GB(r + CITY_Y);
    int size = GB(r + CITY_SIZE);
    uint32_t bld = (uint32_t)GW(r) | ((uint32_t)GW((uint16_t)(r + 2)) << 16);

    /* 1. Ensure at least 1 garrison defender */
    int defenders = 0;
    for (int u = 0; u < 128; u++) {
        uint16_t ur = UNIT(player, u);
        uint8_t ut = GB(ur + 3);
        if (ut != 0xff && ut != 0 && ut < 26 && GB(ur + 1) == cx && GB(ur + 2) == cy)
            defenders++;
    }
    if (defenders == 0) {
        static const int16_t defs[] = {12, 5, 4, 2, 1};
        for (size_t i = 0; i < sizeof defs / sizeof defs[0]; i++) {
            if (list_has(list, count, defs[i])) {
                city_set_gold(city, 0);
                *out = defs[i];
                menu_city = -1;
                ovl_msg("Gov: %s", gov_item_name(defs[i]));
                return 1;
            }
        }
    }

    /* 2. Urgent Aqueduct if city size >= 7 */
    if (size >= 7 && list_has(list, count, -9)) {
        city_set_gold(city, 0);
        *out = -9;
        menu_city = -1;
        ovl_msg("Gov: Aqueduct");
        return 1;
    }

    /* 3. Development buildings in priority order */
    static const int16_t prio[] = {
        -3,  /* Granary */
        -4,  /* Temple */
        -5,  /* Marketplace */
        -6,  /* Library */
        -9,  /* Aqueduct (size >= 5) */
        -7,  /* Courthouse (non-capital) */
        -11, /* Cathedral */
        -10, /* Bank */
        -12, /* University */
        -14, /* Colosseum */
        -15, /* Factory */
        -20, /* Hydro Plant */
        -19, /* Power Plant */
        -16, /* Mfg. Plant */
        -13, /* Mass Transit */
        -18, /* Recycling Center */
        -8,  /* City Walls */
    };
    for (size_t i = 0; i < sizeof prio / sizeof prio[0]; i++) {
        int16_t it = prio[i];
        if (it == -9 && size < 5)
            continue;
        if (it == -7 && (bld & 1u))     /* capital already has Palace */
            continue;
        if (list_has(list, count, it)) {
            city_set_gold(city, 0);
            *out = it;
            menu_city = -1;
            ovl_msg("Gov: %s", gov_item_name(it));
            return 1;
        }
    }

    /* 4. All development buildings done: produce Gold */
    city_set_gold(city, 1);
    *out = (int16_t)RW(SS, list);       /* keep current underlying build */
    menu_city = -1;
    ovl_msg("Gov: Producing Gold");
    return 1;
}

void hook_build_choice(void)            /* 0ad5:1459, AX = chosen item */
{
    int city = menu_city;
    menu_city = -1;
    if (city < 0)
        return;
    uint16_t item = AX;
    if (item == GOLD_ITEM) {
        if (!city_produces_gold(city))
            set_gold(city, 1);
        /* keep building what was set before: the box does not fill anyway */
        AX = RW(SS, (uint16_t)(BP - 0xde));
        WW(SS, (uint16_t)(BP - 0x134), AX);
    } else if (item != 0x62 && item != 0x63 && SI != 0xfffe && city_produces_gold(city))
        set_gold(city, 0);              /* a real build was chosen */
}

/* f_0d07_0043(city, mode): city turn processing (mode 0) and the city
   screen (mode 1).  Wrapped to know which city is on screen. */
void f_0d07_0043(void)
{
    uint16_t city = FARG(0), lo = FARG(1), hi = FARG(2);
    int prev = screen_city;
    if (lo == 1 && hi == 0)
        screen_city = city;
    lifted_f_0d07_0043();       /* in the caller's frame, returns for it */
    screen_city = prev;
}

/* hook before 0d07:1520 "add [city.shields], ax" */
void hook_city_production(void)
{
    int city = (int16_t)RW(SS, (uint16_t)(BP + 6));
    if (RW(SS, (uint16_t)(BP + 8)) != 0 || !city_produces_gold(city))
        return;
    int16_t amount = (int16_t)AX;
    if (amount <= 0)
        return;
    int owner = GB(CITY(city) + CITY_OWNER) & 7;
    int32_t g = (int16_t)GW(GOLD(owner)) + amount;
    if (g > 30000)
        g = 30000;
    WW(DS, GOLD(owner), (uint16_t)g);
    AX = 0;
}

static int key_filter(uint16_t key)
{
    uint8_t ascii = (uint8_t)key;
    if (mp_key(key))
        return 1;
    if (screen_city < 0 && realtime_key(key))
        return 1;
    if (screen_city >= 0 && (ascii == 'g' || ascii == 'G')) {
        toggle(screen_city);
        return 1;
    }
    if (screen_city >= 0 && (ascii == 'm' || ascii == 'M')) {
        toggle_gov(screen_city);
        return 1;
    }
    return 0;
}

static int mouse_filter(int button, int x, int y)
{
    if (screen_city < 0 && button == 0 && realtime_click(x, y))
        return 1;
    if (screen_city < 0 || button != 0)
        return 0;
    if (x >= GOV_BTN_X && x < GOV_BTN_X + GOV_BTN_W &&
        y >= GOV_BTN_Y && y < GOV_BTN_Y + GOV_BTN_H) {
        toggle_gov(screen_city);
        return 1;
    }
    if (x >= GOLD_BTN_X && x < GOLD_BTN_X + GOLD_BTN_W &&
        y >= GOLD_BTN_Y && y < GOLD_BTN_Y + GOLD_BTN_H) {
        toggle(screen_city);
        return 1;
    }
    return 0;
}

void native_save_state(void *fp)
{
    FILE *f = fp;
    fwrite("CIVN2", 1, 5, f);
    fwrite(cap, sizeof cap, 1, f);
    autowork_save(f);
    fwrite(gov, sizeof gov, 1, f);
}

void native_load_state(void *fp)
{
    FILE *f = fp;
    char tag[5];
    memset(cap, 0, sizeof cap);
    memset(gov, 0, sizeof gov);
    if (!f || fread(tag, 1, 5, f) != 5 ||
        (memcmp(tag, "CIVN1", 5) && memcmp(tag, "CIVN2", 5)))
        return;
    if (fread(cap, sizeof cap, 1, f) != 1)
        memset(cap, 0, sizeof cap);
    autowork_load(f);
    if (!memcmp(tag, "CIVN2", 5) && fread(gov, sizeof gov, 1, f) != 1)
        memset(gov, 0, sizeof gov);
}

/* The filters run in the keyboard interrupt / mouse event, where DS is
   whatever the interrupted code had: game data is reached through DGROUP. */
int native_key_filter(uint16_t key)
{
    uint16_t ds = DS;
    DS = DGROUP;
    int r = key_filter(key);
    DS = ds;
    return r;
}

int native_mouse_filter(int button, int x, int y)
{
    uint16_t ds = DS;
    DS = DGROUP;
    int r = mouse_filter(button, x, y);
    DS = ds;
    return r;
}
