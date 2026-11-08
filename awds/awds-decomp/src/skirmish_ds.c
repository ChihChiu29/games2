#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

/* ARM9 Hook Trampolines (called from ARM mode at 0x020C6874 and 0x020C6A80) */
__asm__(
    ".syntax unified\n"
    ".arm\n"
    ".align 2\n"
    ".global HookGetMapName\n"
    ".type HookGetMapName, %function\n"
    "HookGetMapName:\n"
    "    push    {r1, lr}\n"
    "    ldr     r1, [sp, #12]\n"
    "    ldr     r2, [sp, #44]\n"
    "    blx     SkirmishGetMapName\n"
    "    pop     {r1, lr}\n"
    "    cmp     r0, #0\n"
    "    bxne    lr\n"
    "    ldr     r0, =0x0235DF9C\n"
    "    ldr     r0, [r0, r1]\n"
    "    b       0x0233A7C4\n"
    "\n"
    ".global HookDecompressMap\n"
    ".type HookDecompressMap, %function\n"
    "HookDecompressMap:\n"
    "    push    {r0, r1, r2, lr}\n"
    "    mov     r0, r1\n"
    "    mov     r1, r5\n"
    "    ldr     r2, [sp, #28]\n"
    "    ldr     r3, [sp, #60]\n"
    "    blx     SkirmishDecompressMap\n"
    "    cmp     r0, #0\n"
    "    pop     {r0, r1, r2, lr}\n"
    "    bxne    lr\n"
    "    b       0x02112FD4\n"
    ".pool\n"
    ".thumb\n"
    ".align 2\n"
);

#define MAX_W 30
#define MAX_H 20
#define MAX_TILES (MAX_W * MAX_H)

/* Difficulty levels (0..4 within each Versus tab) */
#define DIFF_EASY    0
#define DIFF_NORMAL  1
#define DIFF_MIRROR  2
#define DIFF_HARD    3
#define DIFF_INSANE  4

/* Biome archetypes */
#define BIOME_RIVER  0
#define BIOME_NAVAL  1
#define BIOME_PIPE   2

/* Logical terrain types used in scratch grid during generation */
#define T_PLAIN   0
#define T_ROAD    1
#define T_RIVER   2
#define T_BRIDGE  3
#define T_WOOD    4
#define T_MTN     5
#define T_SEA     6
#define T_REEF    7
#define T_SHOAL   8
#define T_PIPE    9
#define T_SEAM    10

/* Exact AWDS visual tile IDs (verified from runtime t2t table & OV9 5 maps) */
#define TILE_PLAIN         1
#define TILE_PLAIN_SHADOW  33
#define TILE_BRIDGE_EW     21
#define TILE_BRIDGE_NS     22
#define TILE_WOOD          32
#define TILE_SEA           42
#define TILE_ROAD_NS_SHAD  128
#define TILE_MTN_A         134
#define TILE_MTN_B         135
#define TILE_ROAD_CROSS    256
#define TILE_SEAM_EW       354
#define TILE_SEAM_NS       355
#define TILE_REEF          360
#define TILE_SILO          384

#define TILE_COM_NEUTRAL   441
/* Owned Com Towers: 442 (OS), 443 (BM), 444 (GE), 445 (YC) */
#define TILE_COM_OWNED(a)  (441 + (a))

#define TILE_BASE_NEUTRAL  449
#define TILE_CITY_NEUTRAL  450
#define TILE_AIRP_NEUTRAL  451
#define TILE_PORT_NEUTRAL  452

/* Army a in 1..4 (OS=1, BM=2, GE=3, YC=4):
 * HQ:      453 + (a-1)*5
 * Base:    454 + (a-1)*5
 * City:    455 + (a-1)*5
 * Airport: 456 + (a-1)*5
 * Port:    457 + (a-1)*5
 */
#define TILE_ARMY_HQ(a)    (448 + (a) * 5)
#define TILE_ARMY_BASE(a)  (449 + (a) * 5)
#define TILE_ARMY_CITY(a)  (450 + (a) * 5)
#define TILE_ARMY_AIRP(a)  (451 + (a) * 5)
#define TILE_ARMY_PORT(a)  (452 + (a) * 5)

/* Auto-tile lookup tables indexed by 4-bit cardinal mask (N=1, S=2, W=4, E=8) */
static const u16 sRoadTileTable[16] = {
    97,  64,  64,  64,  97,  98,  66, 193,
    97,  96,  65, 225,  97, 192, 224, 256
};

static const u16 sRiverTileTable[16] = {
    24,  24,  24,  24,  25, 123,  91, 219,
    25, 120,  88, 216,  25, 283, 280,  90
};

static const u16 sPipeTileTable[16] = {
    322, 259, 258, 323, 289, 353, 321, 323,
    288, 352, 320, 323, 322, 322, 322, 322
};

/* Sea & Shoal tables indexed by 4-bit cardinal LAND mask (N=1, S=2, W=4, E=8) */
static const u16 sSeaTileTable[16] = {
    42,  74,  10, 108,  44,  75,  43, 108,
    40,  73,  41, 139,  12, 104, 137, 108
};

static const u16 sShoalTileTable[16] = {
    16,  80,  16, 268,  50, 215, 183,  83,
    46, 214, 182,  84,  46, 236, 268, 236
};

static const char * const sDifficultyNames[5] = {
    "Random Easy",
    "Random Norm",
    "Mirror Norm",
    "Random Hard",
    "Random Insane"
};

/* Persistent state inside OV9 5 */
static u32 gLastBattleSeed = 0xA55A3C69;
static u32 gEntropySalt    = 0x13579BDF;

/* Decode mapId into (numPlayers, difficulty). Returns 1 if Skirmish map, 0 otherwise. */
static int SkirmishDecodeMapId(u16 mapId, int *outPlayers, int *outDiff)
{
    if (mapId >= 1 && mapId <= 5) {
        *outPlayers = 2;
        *outDiff = mapId - 1;
        return 1;
    }
    if (mapId >= 18 && mapId <= 22) {
        *outPlayers = 2;
        *outDiff = mapId - 18;
        return 1;
    }
    if (mapId >= 76 && mapId <= 80) {
        *outPlayers = 3;
        *outDiff = mapId - 76;
        return 1;
    }
    if (mapId >= 103 && mapId <= 107) {
        *outPlayers = 4;
        *outDiff = mapId - 103;
        return 1;
    }
    return 0;
}

static u32 SkirmishStepRng(u32 *rng)
{
    u32 x = *rng;
    if (x == 0)
        x = 0x6C078965;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *rng = x;
    return x;
}

static int SkirmishRandRange(u32 *rng, int bound)
{
    if (bound <= 1)
        return 0;
    return (int)((((u32)(SkirmishStepRng(rng) & 0xFFFF)) * (u32)bound) >> 16);
}

static u8 *GetSessionPtr(void)
{
    u32 table = *(volatile u32 *)0x027E027C;
    u32 idx   = *(volatile u32 *)0x027E0284;
    if (table < 0x02000000 || table >= 0x02800000)
        return 0;
    return *(u8 **)(table + idx * 4 + 0x48);
}

static u8 *GetMapCfgPtr(void)
{
    u8 *session = GetSessionPtr();
    if (!session)
        return 0;
    return *(u8 **)(session + 0x1C4);
}

static void ComputeDefaultTeams(int isSkirmish, int numPlayers, int diff, u8 outTeams[4])
{
    outTeams[0] = 0;
    outTeams[1] = 1;
    outTeams[2] = 2;
    outTeams[3] = 3;

    if (isSkirmish) {
        if (numPlayers == 4 && diff == DIFF_EASY) {
            /* 4P Easy: 2v2 Allied (OS + BM on Team A vs GE + YC on Team B) */
            outTeams[0] = 0;
            outTeams[1] = 0;
            outTeams[2] = 1;
            outTeams[3] = 1;
        } else if (numPlayers >= 3 && (diff == DIFF_HARD || diff == DIFF_INSANE)) {
            /* 3P/4P Hard & Insane: 1v2 / 1v3 AI Coalition (OS on Team A vs all AIs on Team B) */
            outTeams[0] = 0;
            outTeams[1] = 1;
            outTeams[2] = 1;
            outTeams[3] = 1;
        }
    }
}

/* Pre-populate Team Select menu defaults (0x022CD0F0 + 0x2C8 = 0x022CD3B8..0x022CD3BB)
 * when browsing/selecting a map on SELECT MAP.
 */
static void SkirmishApplyMenuTeams(int isSkirmish, int numPlayers, int diff)
{
    volatile u8 *menuTeams = (volatile u8 *)0x022CD3B8;
    u8 teams[4];
    int i;

    ComputeDefaultTeams(isSkirmish, numPlayers, diff, teams);
    for (i = 0; i < 4; i++) {
        menuTeams[i] = teams[i];
    }
}

/* Called by GetMapName hook at 0x020C6874 */
const char *SkirmishGetMapName(u16 mapId, u32 callerLr, u32 parentLr)
{
    int numPlayers = 2;
    int diff = 0;
    int isSkirmish = SkirmishDecodeMapId(mapId, &numPlayers, &diff);

    /* When called from InitBattleMap (0x020C7824 -> callerLr = 0x020C7828),
     * persist gLastBattleSeed into mapCfg + 0x14 (bytes 20..23 of the 26-byte map name
     * buffer, untouched by strcpy since our names are <= 14 bytes and saved in SRAM).
     * When InitBattleMap was called from GetMapArmyPresent (0x0206EC18 -> 0x020C73C0,
     * parentLr = 0x020C73C4) entering CO Select, also set session + 0xA2..0xA5 to
     * this map's default teams. We do NOT overwrite session + 0xA2..0xA5 when
     * starting the battle (parentLr = 0x020BDAC8), preserving any manual team
     * customizations the player made on the CO Select screen!
     */
    if (callerLr == 0x020C7828) {
        u8 *session = GetSessionPtr();
        u8 *mapCfg  = GetMapCfgPtr();
        if (mapCfg && isSkirmish) {
            *(u32 *)(mapCfg + 0x14) = gLastBattleSeed;
        }
        if (session && parentLr == 0x020C73C4) {
            u8 teams[4];
            ComputeDefaultTeams(isSkirmish, numPlayers, diff, teams);
            session[0xA2] = teams[0];
            session[0xA3] = teams[1];
            session[0xA4] = teams[2];
            session[0xA5] = teams[3];
        }
    }

    if (!isSkirmish)
        return 0;

    return sDifficultyNames[diff];
}

/* Generate the "Surprise Me!" Giant '?' Question Mark minimap preview (17x13) */
static void SkirmishGenerateQuestionPreview(u8 *destBuf, int numPlayers)
{
    const int w = 17;
    const int h = 13;
    u16 *tiles = (u16 *)(destBuf + 2);
    int x, y;

    /* 5x9 bitmap for the Giant '?' at x = 6..10, y = 2..10 */
    static const u8 sQuestionRows[9] = {
        0x0E, /* . # # # . */
        0x1B, /* # # . # # */
        0x03, /* . . . # # */
        0x06, /* . . # # . */
        0x04, /* . . # . . */
        0x04, /* . . # . . */
        0x00, /* . . . . . */
        0x04, /* . . # . . */
        0x04  /* . . # . . */
    };

    destBuf[0] = (u8)w;
    destBuf[1] = (u8)h;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            u16 tid = TILE_PLAIN;
            if (x == 0 || x == w - 1 || y == 0 || y == h - 1) {
                tid = TILE_SEA;
            }
            tiles[y * w + x] = tid;
        }
    }

    /* Draw dark mountain drop-shadow behind the '?' so it pops on the minimap */
    for (y = 0; y < 9; y++) {
        u8 rowBits = sQuestionRows[y];
        for (x = 0; x < 5; x++) {
            if (rowBits & (1 << (4 - x))) {
                int gx = 6 + x;
                int gy = 2 + y;
                if (gx - 1 >= 1)
                    tiles[gy * w + (gx - 1)] = TILE_MTN_A;
                if (gy + 1 < h - 1)
                    tiles[(gy + 1) * w + gx] = TILE_MTN_A;
            }
        }
    }

    /* Draw bright white road tiles for the Giant '?' */
    for (y = 0; y < 9; y++) {
        u8 rowBits = sQuestionRows[y];
        for (x = 0; x < 5; x++) {
            if (rowBits & (1 << (4 - x))) {
                int gx = 6 + x;
                int gy = 2 + y;
                tiles[gy * w + gx] = TILE_ROAD_CROSS;
            }
        }
    }

    /* Place active Army HQs & starting Bases in the corners so GetMapArmyPresent
     * enables the exact 2, 3, or 4 CO slots on the CO Selection screen!
     */
    tiles[2 * w + 2] = TILE_ARMY_HQ(1);
    tiles[2 * w + 3] = TILE_ARMY_BASE(1);
    tiles[3 * w + 2] = TILE_ARMY_BASE(1);

    tiles[10 * w + 14] = TILE_ARMY_HQ(2);
    tiles[10 * w + 13] = TILE_ARMY_BASE(2);
    tiles[9 * w + 14]  = TILE_ARMY_BASE(2);

    if (numPlayers >= 3) {
        tiles[2 * w + 14] = TILE_ARMY_HQ(3);
        tiles[2 * w + 13] = TILE_ARMY_BASE(3);
        tiles[3 * w + 14] = TILE_ARMY_BASE(3);
    }
    if (numPlayers >= 4) {
        tiles[10 * w + 2] = TILE_ARMY_HQ(4);
        tiles[10 * w + 3] = TILE_ARMY_BASE(4);
        tiles[9 * w + 2]  = TILE_ARMY_BASE(4);
    }
}

static int AbsInt(int v)
{
    return v < 0 ? -v : v;
}

static int ManhattanDist(int x1, int y1, int x2, int y2)
{
    return AbsInt(x1 - x2) + AbsInt(y1 - y2);
}

static int IsWaterTerrain(u8 t)
{
    return (t == T_SEA || t == T_SHOAL || t == T_REEF || t == T_RIVER || t == T_BRIDGE);
}

/* Helper to set terrain with optional symmetry (2P rotational or 3P/4P 4-way reflection) */
static void SetTerrainSym(u8 *tgrid, int w, int h, int x, int y, u8 t, int isMirror, int numPlayers)
{
    if (x < 0 || x >= w || y < 0 || y >= h)
        return;
    tgrid[y * w + x] = t;
    if (isMirror) {
        int mx = w - 1 - x;
        int my = h - 1 - y;
        tgrid[my * w + mx] = t;
        if (numPlayers >= 3) {
            tgrid[y * w + mx]  = t;
            tgrid[my * w + x]  = t;
        }
    }
}

/* Carve a straight/dogleg road segment between (x0, y0) and (x1, y1),
 * turning River/Sea into Bridge and Pipe into Seam.
 */
static void CarveRoadSegment(u8 *tgrid, u16 *tiles, int w, int h, int x0, int y0, int x1, int y1)
{
    int x = x0;
    int y = y0;
    while (1) {
        if (x > 0 && x < w - 1 && y > 0 && y < h - 1 && tiles[y * w + x] == 0) {
            u8 cur = tgrid[y * w + x];
            if (cur == T_RIVER || cur == T_SEA || cur == T_SHOAL || cur == T_REEF || cur == T_BRIDGE) {
                tgrid[y * w + x] = T_BRIDGE;
            } else if (cur == T_PIPE || cur == T_SEAM) {
                tgrid[y * w + x] = T_SEAM;
            } else {
                tgrid[y * w + x] = T_ROAD;
            }
        }
        if (x == x1 && y == y1)
            break;
        if (AbsInt(x - x1) >= AbsInt(y - y1)) {
            x += (x < x1) ? 1 : -1;
        } else {
            y += (y < y1) ? 1 : -1;
        }
    }
}

/* Place a single property (and mirror it if in Mirror mode) if valid */
static int TryPlaceProperty(
    u8 *tgrid, u16 *tiles, int w, int h,
    int x, int y, u16 propTile,
    int isMirror, int numPlayers, int minBuildingDist)
{
    int pts[4][2];
    int count = 1;
    int i, dy, dx;

    if (x <= 1 || x >= w - 2 || y <= 1 || y >= h - 2)
        return 0;

    pts[0][0] = x;
    pts[0][1] = y;

    if (isMirror) {
        int mx = w - 1 - x;
        int my = h - 1 - y;
        if (numPlayers == 2) {
            pts[1][0] = mx;
            pts[1][1] = my;
            count = 2;
        } else {
            pts[1][0] = mx;
            pts[1][1] = y;
            pts[2][0] = x;
            pts[2][1] = my;
            pts[3][0] = mx;
            pts[3][1] = my;
            count = 4;
        }
    }

    for (i = 0; i < count; i++) {
        int px = pts[i][0];
        int py = pts[i][1];
        /* Never overwrite roads, bridges, rivers, sea, shoals, or pipes! */
        if (tgrid[py * w + px] != T_PLAIN)
            return 0;
        for (dy = -minBuildingDist; dy <= minBuildingDist; dy++) {
            for (dx = -minBuildingDist; dx <= minBuildingDist; dx++) {
                int nx = px + dx;
                int ny = py + dy;
                if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                    if (tiles[ny * w + nx] != 0)
                        return 0;
                }
            }
        }
    }

    for (i = 0; i < count; i++) {
        int px = pts[i][0];
        int py = pts[i][1];
        u16 tid = propTile;
        if (isMirror && propTile >= 453 && propTile <= 472) {
            int offsetInArmy = propTile - 453;
            int armyIdx = i + 1;
            while (offsetInArmy >= 5)
                offsetInArmy -= 5;
            if (armyIdx <= numPlayers) {
                tid = (u16)(453 + (armyIdx - 1) * 5 + offsetInArmy);
            } else {
                tid = (offsetInArmy == 0) ? (u16)TILE_COM_NEUTRAL : (u16)(449 + offsetInArmy - 1);
            }
        } else if (isMirror && propTile >= 442 && propTile <= 445) {
            int armyIdx = i + 1;
            tid = (armyIdx <= numPlayers) ? (u16)TILE_COM_OWNED(armyIdx) : (u16)TILE_COM_NEUTRAL;
        }
        tgrid[py * w + px] = T_PLAIN;
        tiles[py * w + px] = tid;
    }

    return 1;
}

static void SkirmishGenerateBattleMap(u8 *destBuf, int numPlayers, int diff, u32 seed)
{
    u32 rng = seed;
    int w, h;
    int isMirror = (diff == DIFF_MIRROR);
    int biome;
    u16 *tiles = (u16 *)(destBuf + 2);
    /* destBuf is a 2580-byte heap buffer (0x020C6AA0); bytes 1400..2000 are unused scratch space */
    u8 *tgrid = destBuf + 1400;
    int hqX[5], hqY[5];
    int x, y, i, a;
    int midX, midY;

    /* 1. Choose Odd Map Dimensions so (midX, midY) is the exact geometric center
     * (w - 1 - midX == midX and h - 1 - midY == midY), equidistant from all 4 corners!
     */
    if (diff == DIFF_EASY) {
        w = (numPlayers <= 2) ? 21 : 25;
        h = (numPlayers <= 2) ? 15 : 17;
    } else if (diff == DIFF_NORMAL) {
        w = (numPlayers <= 2) ? 23 : 27;
        h = (numPlayers <= 2) ? 15 : 17;
    } else if (diff == DIFF_MIRROR) {
        w = (numPlayers <= 2) ? 25 : 27;
        h = (numPlayers <= 2) ? 17 : 19;
    } else if (diff == DIFF_HARD) {
        w = (numPlayers <= 2) ? 27 : 29;
        h = (numPlayers <= 2) ? 17 : 19;
    } else {
        w = 29;
        h = 19;
    }

    destBuf[0] = (u8)w;
    destBuf[1] = (u8)h;
    midX = w / 2;
    midY = h / 2;

    for (i = 0; i < w * h; i++) {
        tiles[i] = 0;
        tgrid[i] = T_PLAIN;
    }

    /* 2. Choose Biome Archetype (River Heartland, Naval Strait, or Pipe Fortress) */
    biome = SkirmishRandRange(&rng, 3);

    /* 3. Determine HQ Corner Coordinates (aligned at 3 and w-4 / h-4 so highways are 100% straight)
     * Corner 1 (OS): Top-Left
     * Corner 2 (BM): Bottom-Right
     * Corner 3 (GE): Top-Right
     * Corner 4 (YC): Bottom-Left
     */
    hqX[1] = 3;      hqY[1] = 3;
    hqX[2] = w - 4;  hqY[2] = h - 4;
    hqX[3] = w - 4;  hqY[3] = 3;
    hqX[4] = 3;      hqY[4] = h - 4;

    /* 4. Paint Sea Border around the entire perimeter */
    for (x = 0; x < w; x++) {
        tgrid[0 * w + x] = T_SEA;
        tgrid[(h - 1) * w + x] = T_SEA;
    }
    for (y = 0; y < h; y++) {
        tgrid[y * w + 0] = T_SEA;
        tgrid[y * w + (w - 1)] = T_SEA;
    }

    /* 5. Generate Biome Backbone */
    if (biome == BIOME_NAVAL) {
        /* Naval Strait / Inland Sea Basin:
         * Unobstructed 5x(h-8) Open Sea Basin (x = midX-2..midX+2, y = 4..h-5) surrounded by a
         * Shoal beach ring (x = midX-3..midX+3, y = 3..h-4), leaving dry coastal land corridors
         * at y = 1..2 (North) and y = h-3..h-2 (South) so ships never hit bridges and ground units
         * can still flank around the sea!
         */
        for (y = 3; y <= h - 4; y++) {
            for (x = midX - 3; x <= midX + 3; x++) {
                if (y == 3 || y == h - 4 || x == midX - 3 || x == midX + 3) {
                    tgrid[y * w + x] = T_SHOAL;
                } else {
                    tgrid[y * w + x] = T_SEA;
                }
            }
        }
        /* Scatter symmetric Reefs in the Open Sea Basin */
        SetTerrainSym(tgrid, w, h, midX - 1, 5,        T_REEF, 1, 4);
        SetTerrainSym(tgrid, w, h, midX,     midY - 2, T_REEF, 1, 4);
    } else if (biome == BIOME_PIPE) {
        /* Pipe Fortress: 1-tile-thick Black Hole Pipe wall along x = midX (and y = midY in 3P/4P) */
        for (y = 1; y < h - 1; y++) {
            if (AbsInt(y - midY) <= 1)
                continue; /* Central courtyard gap for Silo objective */
            tgrid[y * w + midX] = T_PIPE;
        }
        if (numPlayers >= 3) {
            for (x = 1; x < w - 1; x++) {
                if (AbsInt(x - midX) <= 2)
                    continue;
                tgrid[midY * w + x] = T_PIPE;
            }
        }
    } else {
        /* River Heartland: Continuous 1-tile-wide N-S river along x = midX (plus E-W river on 3P/4P) */
        for (y = 1; y < h - 1; y++) {
            tgrid[y * w + midX] = T_RIVER;
        }
        if (numPlayers >= 3) {
            for (x = 1; x < w - 1; x++) {
                tgrid[midY * w + x] = T_RIVER;
            }
        }
    }

    /* 6. Carve Primary Single-Track Road Network */
    if (biome == BIOME_NAVAL) {
        /* Clean single-track highway loop around the Open Sea Basin:
         * - West Trunk at x = 5 (right outside East Bases at x = 4)
         * - East Trunk at x = w - 6 (right outside West Bases at x = w - 5)
         * - North Coastal Highway at y = 2 (x = 5 .. w - 6)
         * - South Coastal Highway at y = h - 3 (x = 5 .. w - 6)
         * - Mid-Basin Port Spurs at y = midY (x = 5 .. midX - 5 and midX + 5 .. w - 6)
         */
        int westX = 5;
        int eastX = w - 6;
        CarveRoadSegment(tgrid, tiles, w, h, westX, 2,     eastX, 2);
        CarveRoadSegment(tgrid, tiles, w, h, westX, h - 3, eastX, h - 3);
        CarveRoadSegment(tgrid, tiles, w, h, westX, 2,     westX, h - 3);
        CarveRoadSegment(tgrid, tiles, w, h, eastX, 2,     eastX, h - 3);
        if (midX - 5 >= westX) {
            CarveRoadSegment(tgrid, tiles, w, h, westX,    midY, midX - 5, midY);
            CarveRoadSegment(tgrid, tiles, w, h, midX + 5, midY, eastX,    midY);
        }
    } else {
        /* Clean perimeter highway loop connecting all 4 corners (x = 3..w-4, y = 3..h-4),
         * crossing the River (into straight Bridges) or Pipe (into single Pipe Seams):
         */
        CarveRoadSegment(tgrid, tiles, w, h, 3,     3,     w - 4, 3);
        CarveRoadSegment(tgrid, tiles, w, h, 3,     h - 4, w - 4, h - 4);
        CarveRoadSegment(tgrid, tiles, w, h, 3,     3,     3,     h - 4);
        CarveRoadSegment(tgrid, tiles, w, h, w - 4, 3,     w - 4, h - 4);
        if (numPlayers <= 2) {
            /* In 2P, y = midY is dry land, so run a central E-W highway across (midX, midY) */
            CarveRoadSegment(tgrid, tiles, w, h, 3, midY, w - 4, midY);
        } else if (biome == BIOME_RIVER) {
            /* In 3P/4P River Heartland, carve an inner rotary road loop around the central
             * river crossroads, creating 4 inner bridges with road approaches on all banks!
             */
            CarveRoadSegment(tgrid, tiles, w, h, midX - 3, midY - 3, midX + 3, midY - 3);
            CarveRoadSegment(tgrid, tiles, w, h, midX - 3, midY + 3, midX + 3, midY + 3);
            CarveRoadSegment(tgrid, tiles, w, h, midX - 3, midY - 3, midX - 3, midY + 3);
            CarveRoadSegment(tgrid, tiles, w, h, midX + 3, midY - 3, midX + 3, midY + 3);
        }
    }

    /* 7. Place HQs & Starting Bases / Airports / Cities */
    for (a = 1; a <= numPlayers; a++) {
        int hx = hqX[a];
        int hy = hqY[a];
        int dx = (hx < midX) ? 1 : -1;
        int dy = (hy < midY) ? 1 : -1;

        tiles[hy * w + hx] = TILE_ARMY_HQ(a);
        tgrid[hy * w + hx] = T_PLAIN;

        /* 2 core starting Bases adjacent to HQ (opening directly onto the E-W and N-S highways!) */
        tiles[hy * w + (hx + dx)] = TILE_ARMY_BASE(a);
        tgrid[hy * w + (hx + dx)] = T_PLAIN;

        tiles[(hy + dy) * w + hx] = TILE_ARMY_BASE(a);
        tgrid[(hy + dy) * w + hx] = T_PLAIN;

        /* Starting Airport or extra Base depending on difficulty
         * (Bonus properties placed within the 3x3 HQ corner at (hx-dx, hy-dy) and (hx+dx, hy+dy)
         * so they never block the y=2, y=3, x=3, or x=5 highways!)
         */
        if (a == 1 && diff == DIFF_EASY) {
            /* Player 1 advantage on Easy: extra Base + starting Airport + 2 Cities */
            tiles[(hy - dy) * w + hx] = TILE_ARMY_BASE(1);
            tgrid[(hy - dy) * w + hx] = T_PLAIN;
            tiles[hy * w + (hx - dx)] = TILE_ARMY_AIRP(1);
            tgrid[hy * w + (hx - dx)] = T_PLAIN;
            tiles[(hy - dy) * w + (hx - dx)] = TILE_ARMY_CITY(1);
            tgrid[(hy - dy) * w + (hx - dx)] = T_PLAIN;
            tiles[(hy + dy) * w + (hx + dx)] = TILE_ARMY_CITY(1);
            tgrid[(hy + dy) * w + (hx + dx)] = T_PLAIN;
        } else if (a >= 2 && (diff == DIFF_HARD || diff == DIFF_INSANE)) {
            /* AI advantage on Hard/Insane: extra Base + starting Airport + City */
            tiles[(hy - dy) * w + hx] = TILE_ARMY_BASE(a);
            tgrid[(hy - dy) * w + hx] = T_PLAIN;
            tiles[hy * w + (hx - dx)] = TILE_ARMY_AIRP(a);
            tgrid[hy * w + (hx - dx)] = T_PLAIN;
            if (diff == DIFF_INSANE) {
                tiles[(hy + dy) * w + (hx + dx)] = TILE_ARMY_CITY(a);
                tgrid[(hy + dy) * w + (hx + dx)] = T_PLAIN;
            }
        } else {
            /* Balanced starting City + neutral expansion Airport nearby */
            tiles[(hy - dy) * w + hx] = TILE_ARMY_CITY(a);
            tgrid[(hy - dy) * w + hx] = T_PLAIN;
            tiles[hy * w + (hx - dx)] = TILE_AIRP_NEUTRAL;
            tgrid[hy * w + (hx - dx)] = T_PLAIN;
        }

        /* On Insane, give AI armies a pre-owned Com Tower right behind their HQ! */
        if (a >= 2 && diff == DIFF_INSANE) {
            tiles[(hy - dy) * w + (hx - dx)] = TILE_COM_OWNED(a);
            tgrid[(hy - dy) * w + (hx - dx)] = T_PLAIN;
        }
    }

    /* If 3P, place Neutral cities/bases in the 4th corner so the map remains balanced */
    if (numPlayers == 3) {
        int hx = hqX[4];
        int hy = hqY[4];
        tiles[hy * w + hx]       = TILE_COM_NEUTRAL;
        tgrid[hy * w + hx]       = T_PLAIN;
        tiles[hy * w + (hx + 1)] = TILE_BASE_NEUTRAL;
        tgrid[hy * w + (hx + 1)] = T_PLAIN;
        tiles[(hy - 1) * w + hx] = TILE_BASE_NEUTRAL;
        tgrid[(hy - 1) * w + hx] = T_PLAIN;
        tiles[(hy + 1) * w + hx] = TILE_CITY_NEUTRAL;
        tgrid[(hy + 1) * w + hx] = T_PLAIN;
        tiles[hy * w + (hx - 1)] = TILE_AIRP_NEUTRAL;
        tgrid[hy * w + (hx - 1)] = T_PLAIN;
    }

    /* 8. Place Naval Ports if in Naval Strait biome */
    if (biome == BIOME_NAVAL) {
        int pyTop = 4;
        int pyBot = h - 5;
        /* Starting Ports on the West & East Shoal coasts of the Open Sea Basin */
        tiles[pyTop * w + (midX - 3)] = TILE_ARMY_PORT(1);
        tgrid[pyTop * w + (midX - 3)] = T_SEA;

        tiles[pyBot * w + (midX + 3)] = TILE_ARMY_PORT(2);
        tgrid[pyBot * w + (midX + 3)] = T_SEA;

        tiles[pyTop * w + (midX + 3)] = (numPlayers >= 3) ? TILE_ARMY_PORT(3) : TILE_PORT_NEUTRAL;
        tgrid[pyTop * w + (midX + 3)] = T_SEA;

        tiles[pyBot * w + (midX - 3)] = (numPlayers >= 4) ? TILE_ARMY_PORT(4) : TILE_PORT_NEUTRAL;
        tgrid[pyBot * w + (midX - 3)] = T_SEA;

        /* Contested Neutral Ports at mid-basin */
        tiles[midY * w + (midX - 3)] = TILE_PORT_NEUTRAL;
        tgrid[midY * w + (midX - 3)] = T_SEA;

        tiles[midY * w + (midX + 3)] = TILE_PORT_NEUTRAL;
        tgrid[midY * w + (midX + 3)] = T_SEA;
    }

    /* 9. Place Centerpiece Objective: Central Missile Silo (384) & Contested Com Towers (441) */
    if (biome == BIOME_NAVAL) {
        /* Twin coastal Missile Silos overlooking the Naval Basin */
        tiles[midY * w + (midX - 4)] = TILE_SILO;
        tgrid[midY * w + (midX - 4)] = T_PLAIN;
        tiles[midY * w + (midX + 4)] = TILE_SILO;
        tgrid[midY * w + (midX + 4)] = T_PLAIN;
    } else if (biome == BIOME_RIVER) {
        /* Symmetric riverbank Missile Silos at (midX - 2, midY - 2) & mirrors
         * so they never block the central E-W road (in 2P) or the E-W river (in 3P/4P)!
         */
        TryPlaceProperty(tgrid, tiles, w, h, midX - 2, midY - 2, TILE_SILO, 1, numPlayers, 0);
    } else {
        /* Exact center King-of-the-Hill Missile Silo at (midX, midY) in Pipe Fortress */
        tiles[midY * w + midX] = TILE_SILO;
        tgrid[midY * w + midX] = T_PLAIN;
    }

    /* Place symmetric Contested Neutral Com Towers on the flanks (without overwriting roads) */
    if (!TryPlaceProperty(tgrid, tiles, w, h, midX - 4, 2, TILE_COM_NEUTRAL, 1, numPlayers, 1)) {
        TryPlaceProperty(tgrid, tiles, w, h, midX - 4, 3, TILE_COM_NEUTRAL, 1, numPlayers, 0);
    }

    /* 10. Place Neutral Bases and Cities across the map */
    {
        int targetNeutralBases = isMirror ? 2 : 4;
        int targetCities       = isMirror ? 5 : (10 + diff * 2);
        int placed = 0;
        int tries;

        for (tries = 0; tries < 80 && placed < targetNeutralBases; tries++) {
            int rx = 3 + SkirmishRandRange(&rng, (isMirror ? (midX - 4) : (w - 6)));
            int ry = 3 + SkirmishRandRange(&rng, (isMirror && numPlayers >= 3) ? (midY - 4) : (h - 6));
            if (TryPlaceProperty(tgrid, tiles, w, h, rx, ry, TILE_BASE_NEUTRAL, isMirror, numPlayers, 2)) {
                placed++;
            }
        }

        placed = 0;
        for (tries = 0; tries < 160 && placed < targetCities; tries++) {
            int rx = 2 + SkirmishRandRange(&rng, (isMirror ? (midX - 2) : (w - 4)));
            int ry = 2 + SkirmishRandRange(&rng, (isMirror && numPlayers >= 3) ? (midY - 2) : (h - 4));
            if (TryPlaceProperty(tgrid, tiles, w, h, rx, ry, TILE_CITY_NEUTRAL, isMirror, numPlayers, 1)) {
                placed++;
            }
        }
    }

    /* 11. Scatter Tactical Woods & Mountains on Remaining Plain Tiles */
    for (y = 1; y < h - 1; y++) {
        for (x = 1; x < w - 1; x++) {
            int roll;
            if (isMirror && x > midX)
                continue;
            if (isMirror && x == midX && y > midY)
                continue;
            if (isMirror && numPlayers >= 3 && y > midY)
                continue;
            if (tgrid[y * w + x] != T_PLAIN || tiles[y * w + x] != 0)
                continue;

            /* Keep immediate 2-tile access around HQ and central courtyard clear of mountains */
            if (AbsInt(x - midX) <= 2 && AbsInt(y - midY) <= 2)
                continue;
            {
                int nearHq = 0;
                for (a = 1; a <= 4; a++) {
                    if (ManhattanDist(x, y, hqX[a], hqY[a]) <= 2)
                        nearHq = 1;
                }
                if (nearHq)
                    continue;
            }

            roll = SkirmishRandRange(&rng, 100);
            if (roll < 16) {
                SetTerrainSym(tgrid, w, h, x, y, T_WOOD, isMirror, numPlayers);
            } else if (roll < 28) {
                SetTerrainSym(tgrid, w, h, x, y, T_MTN, isMirror, numPlayers);
            }
        }
    }

    /* Make sure land properties sit on T_PLAIN and ports sit on T_SEA */
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            u16 tid = tiles[y * w + x];
            if (tid != 0) {
                if (tid == TILE_PORT_NEUTRAL || tid == TILE_ARMY_PORT(1) ||
                    tid == TILE_ARMY_PORT(2) || tid == TILE_ARMY_PORT(3) ||
                    tid == TILE_ARMY_PORT(4)) {
                    tgrid[y * w + x] = T_SEA;
                } else {
                    tgrid[y * w + x] = T_PLAIN;
                }
            }
        }
    }

    /* 12. Resolve Final AWDS Visual Tile IDs with Full Auto-Tiling & Shadows */
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            u8 t = tgrid[y * w + x];
            u16 existing = tiles[y * w + x];

            /* Already placed Property / Silo / Com Tower */
            if (existing != 0)
                continue;

            if (t == T_WOOD) {
                tiles[y * w + x] = TILE_WOOD;
            } else if (t == T_MTN) {
                tiles[y * w + x] = ((x + y) & 1) ? TILE_MTN_A : TILE_MTN_B;
            } else if (t == T_REEF) {
                tiles[y * w + x] = TILE_REEF;
            } else if (t == T_BRIDGE) {
                /* If water (River/Sea) is to the North or South, the channel flows N-S -> Bridge is E-W */
                int nsWater = 0;
                if (y > 0     && (tgrid[(y - 1) * w + x] == T_RIVER || tgrid[(y - 1) * w + x] == T_SEA || tgrid[(y - 1) * w + x] == T_REEF))
                    nsWater = 1;
                if (y < h - 1 && (tgrid[(y + 1) * w + x] == T_RIVER || tgrid[(y + 1) * w + x] == T_SEA || tgrid[(y + 1) * w + x] == T_REEF))
                    nsWater = 1;
                tiles[y * w + x] = nsWater ? TILE_BRIDGE_EW : TILE_BRIDGE_NS;
            } else if (t == T_SEAM) {
                int nsPipe = 0;
                if (y > 0     && (tgrid[(y - 1) * w + x] == T_PIPE || tgrid[(y - 1) * w + x] == T_SEAM))
                    nsPipe = 1;
                if (y < h - 1 && (tgrid[(y + 1) * w + x] == T_PIPE || tgrid[(y + 1) * w + x] == T_SEAM))
                    nsPipe = 1;
                tiles[y * w + x] = nsPipe ? TILE_SEAM_NS : TILE_SEAM_EW;
            } else if (t == T_PIPE) {
                int m = 0;
                if (y > 0     && (tgrid[(y - 1) * w + x] == T_PIPE || tgrid[(y - 1) * w + x] == T_SEAM)) m |= 1;
                if (y < h - 1 && (tgrid[(y + 1) * w + x] == T_PIPE || tgrid[(y + 1) * w + x] == T_SEAM)) m |= 2;
                if (x > 0     && (tgrid[y * w + (x - 1)] == T_PIPE || tgrid[y * w + (x - 1)] == T_SEAM)) m |= 4;
                if (x < w - 1 && (tgrid[y * w + (x + 1)] == T_PIPE || tgrid[y * w + (x + 1)] == T_SEAM)) m |= 8;
                tiles[y * w + x] = sPipeTileTable[m];
            } else if (t == T_ROAD) {
                int m = 0;
                if (y > 0     && (tgrid[(y - 1) * w + x] == T_ROAD || tgrid[(y - 1) * w + x] == T_BRIDGE || tgrid[(y - 1) * w + x] == T_SEAM)) m |= 1;
                if (y < h - 1 && (tgrid[(y + 1) * w + x] == T_ROAD || tgrid[(y + 1) * w + x] == T_BRIDGE || tgrid[(y + 1) * w + x] == T_SEAM)) m |= 2;
                if (x > 0     && (tgrid[y * w + (x - 1)] == T_ROAD || tgrid[y * w + (x - 1)] == T_BRIDGE || tgrid[y * w + (x - 1)] == T_SEAM)) m |= 4;
                if (x < w - 1 && (tgrid[y * w + (x + 1)] == T_ROAD || tgrid[y * w + (x + 1)] == T_BRIDGE || tgrid[y * w + (x + 1)] == T_SEAM)) m |= 8;
                tiles[y * w + x] = sRoadTileTable[m];
            } else if (t == T_RIVER) {
                int m = 0;
                if (y <= 1     || IsWaterTerrain(tgrid[(y - 1) * w + x])) m |= 1;
                if (y >= h - 2 || IsWaterTerrain(tgrid[(y + 1) * w + x])) m |= 2;
                if (x <= 1     || IsWaterTerrain(tgrid[y * w + (x - 1)])) m |= 4;
                if (x >= w - 2 || IsWaterTerrain(tgrid[y * w + (x + 1)])) m |= 8;
                tiles[y * w + x] = sRiverTileTable[m];
            } else if (t == T_SHOAL) {
                int m = 0;
                if (y > 0     && !IsWaterTerrain(tgrid[(y - 1) * w + x])) m |= 1;
                if (y < h - 1 && !IsWaterTerrain(tgrid[(y + 1) * w + x])) m |= 2;
                if (x > 0     && !IsWaterTerrain(tgrid[y * w + (x - 1)])) m |= 4;
                if (x < w - 1 && !IsWaterTerrain(tgrid[y * w + (x + 1)])) m |= 8;
                tiles[y * w + x] = sShoalTileTable[m];
            } else if (t == T_SEA) {
                int m = 0;
                if (y > 0     && !IsWaterTerrain(tgrid[(y - 1) * w + x])) m |= 1;
                if (y < h - 1 && !IsWaterTerrain(tgrid[(y + 1) * w + x])) m |= 2;
                if (x > 0     && !IsWaterTerrain(tgrid[y * w + (x - 1)])) m |= 4;
                if (x < w - 1 && !IsWaterTerrain(tgrid[y * w + (x + 1)])) m |= 8;
                if (m != 0) {
                    tiles[y * w + x] = sSeaTileTable[m];
                } else {
                    u16 st = TILE_SEA;
                    if (x < w - 1 && y < h - 1 && !IsWaterTerrain(tgrid[(y + 1) * w + (x + 1)]))
                        st = 9;
                    else if (x > 0 && y < h - 1 && !IsWaterTerrain(tgrid[(y + 1) * w + (x - 1)]))
                        st = 11;
                    else if (x < w - 1 && y > 0 && !IsWaterTerrain(tgrid[(y - 1) * w + (x + 1)]))
                        st = 72;
                    else if (x > 0 && y > 0 && !IsWaterTerrain(tgrid[(y - 1) * w + (x - 1)]))
                        st = 76;
                    tiles[y * w + x] = st;
                }
            } else {
                tiles[y * w + x] = TILE_PLAIN;
            }
        }
    }

    /* 13. Apply West-of-Tall-Tile Shadows to Plains (1 -> 33) and N-S Roads (64 -> 128) */
    for (y = 0; y < h; y++) {
        for (x = 1; x < w; x++) {
            u16 west = tiles[y * w + (x - 1)];
            int westTall = (west == TILE_WOOD || west == TILE_MTN_A || west == TILE_MTN_B ||
                            (west >= 258 && west <= 355) || west == TILE_SILO || west >= 441);
            if (westTall) {
                if (tiles[y * w + x] == TILE_PLAIN)
                    tiles[y * w + x] = TILE_PLAIN_SHADOW;
                else if (tiles[y * w + x] == 64)
                    tiles[y * w + x] = TILE_ROAD_NS_SHAD;
            }
        }
    }
}

/* Called by DecompressMap hook at 0x020C6A80 */
int SkirmishDecompressMap(u8 *destBuf, u16 mapId, u32 callerLr, u32 parentLr)
{
    int numPlayers = 2;
    int diff = 0;
    int isSkirmish = SkirmishDecodeMapId(mapId, &numPlayers, &diff);

    /* When browsing/confirming a map on the SELECT MAP screen (0x0202CAE0 / 0x0202CE2C),
     * pre-populate the Team Select screen (0x022CD3B8..0x022CD3BB) with the difficulty's team preset!
     */
    if (callerLr >= 0x0202CA00 && callerLr < 0x0202D700) {
        SkirmishApplyMenuTeams(isSkirmish, numPlayers, diff);
        if (!isSkirmish)
            return 0;
        SkirmishGenerateQuestionPreview(destBuf, numPlayers);
        return 1;
    }

    if (!isSkirmish)
        return 0;

    /* Roll a fresh random battle seed when entering CO Select from SELECT MAP
     * (callerLr == 0x020C771C && parentLr == 0x020C73C4), and reuse that exact seed
     * when starting the battle (parentLr == 0x020BDAC8) so the Battle Rules property
     * count (Capture Limit slider) and the battle map match 100%!
     * On Save / Continue (0x02018500 / 0x02018B3C), restore seed from mapCfg + 0x14.
     */
    {
        u32 seed = 0;
        if (callerLr == 0x020C771C) {
            if (parentLr == 0x020C73C4) {
                u32 vcount = *(volatile u16 *)0x04000006;
                u32 t0     = *(volatile u16 *)0x04000100;
                u32 frame  = *(volatile u32 *)0x027FFFC0;
                gEntropySalt += 0x9E3779B9;
                seed = gLastBattleSeed ^ (vcount << 16) ^ (t0 << 5) ^ frame ^ gEntropySalt ^ ((u32)mapId * 0x45D9F3B);
                SkirmishStepRng(&seed);
                SkirmishStepRng(&seed);
                gLastBattleSeed = seed;
            } else {
                seed = gLastBattleSeed;
            }
        } else {
            u8 *mapCfg = GetMapCfgPtr();
            if (mapCfg)
                seed = *(u32 *)(mapCfg + 0x14);
            if (seed == 0)
                seed = gLastBattleSeed;
        }

        SkirmishGenerateBattleMap(destBuf, numPlayers, diff, seed);
    }

    return 1;
}
