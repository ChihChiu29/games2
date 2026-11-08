#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

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
    24,  24,  24,  24,  25, 123,  91,  24,
    25, 120,  88,  24,  25,  26,  58,  90
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

/* Pre-populate Team Select menu defaults (0x022CD0F0 + 0x2C8 = 0x022CD3B8..0x022CD3BB)
 * when browsing/selecting a map on SELECT MAP.
 */
static void SkirmishApplyMenuTeams(int isSkirmish, int numPlayers, int diff)
{
    volatile u8 *menuTeams = (volatile u8 *)0x022CD3B8;
    u8 teams[4];
    int i;

    teams[0] = 0;
    teams[1] = 1;
    teams[2] = 2;
    teams[3] = 3;

    if (isSkirmish) {
        if (numPlayers == 4 && diff == DIFF_EASY) {
            /* 4P Easy: 2v2 Allied (OS + BM on Team A vs GE + YC on Team B) */
            teams[0] = 0;
            teams[1] = 0;
            teams[2] = 1;
            teams[3] = 1;
        } else if (numPlayers >= 3 && (diff == DIFF_HARD || diff == DIFF_INSANE)) {
            /* 3P/4P Hard & Insane: 1v2 / 1v3 AI Coalition (OS on Team A vs all AIs on Team B) */
            teams[0] = 0;
            teams[1] = 1;
            teams[2] = 1;
            teams[3] = 1;
        }
    }

    for (i = 0; i < 4; i++) {
        menuTeams[i] = teams[i];
    }
}

/* Called by GetMapName hook at 0x020C6874 */
const char *SkirmishGetMapName(u16 mapId, u32 callerLr)
{
    int numPlayers, diff;
    if (!SkirmishDecodeMapId(mapId, &numPlayers, &diff))
        return 0;

    /* When called from InitBattleMap (0x020C7824 -> lr = 0x020C7828),
     * mapCfg + 0x28..0x2B has just been cleared to 0. Persist gLastBattleSeed there,
     * and apply the difficulty's default team preset into session + 0xA2..0xA5
     * (which 0x020C41DC copies into army[1..4].team right after InitBattleMap)!
     */
    if (callerLr == 0x020C7828) {
        u8 *session = GetSessionPtr();
        u8 *mapCfg  = GetMapCfgPtr();
        if (mapCfg) {
            *(u32 *)(mapCfg + 0x28) = gLastBattleSeed;
        }
        if (session &&
            session[0xA2] == 0 && session[0xA3] == 1 &&
            session[0xA4] == 2 && session[0xA5] == 3) {
            if (numPlayers == 4 && diff == DIFF_EASY) {
                /* 4P Easy: 2v2 Allied (OS + BM on Team A vs GE + YC on Team B) */
                session[0xA2] = 0;
                session[0xA3] = 0;
                session[0xA4] = 1;
                session[0xA5] = 1;
            } else if (numPlayers >= 3 && (diff == DIFF_HARD || diff == DIFF_INSANE)) {
                /* 3P/4P Hard & Insane: 1v2 / 1v3 AI Coalition (OS vs allied AIs) */
                session[0xA2] = 0;
                session[0xA3] = 1;
                session[0xA4] = 1;
                session[0xA5] = 1;
            }
        }
    }

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
    return (t == T_SEA || t == T_SHOAL || t == T_REEF || t == T_BRIDGE);
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

/* Carve a road between (x0, y0) and (x1, y1), turning River/Sea into Bridge and Pipe into Seam */
static void CarveRoadPath(u8 *tgrid, u16 *tiles, int w, int h, int x0, int y0, int x1, int y1, u32 *rng, int isMirror, int numPlayers)
{
    int x = x0;
    int y = y0;
    int steps = 0;
    while ((x != x1 || y != y1) && steps < 64) {
        int moveX = 0;
        steps++;
        if (x == x1) {
            moveX = 0;
        } else if (y == y1) {
            moveX = 1;
        } else if (isMirror) {
            /* Deterministic L/stair step in mirror mode so paths are 100% symmetric */
            moveX = ((AbsInt(x - x1) >= AbsInt(y - y1)) ? 1 : 0);
        } else {
            moveX = (SkirmishRandRange(rng, 100) < 55) ? 1 : 0;
        }

        if (moveX) {
            x += (x < x1) ? 1 : -1;
        } else {
            y += (y < y1) ? 1 : -1;
        }

        if (x <= 0 || x >= w - 1 || y <= 0 || y >= h - 1)
            continue;

        /* Do not overwrite already-placed properties */
        if (tiles[y * w + x] != 0)
            continue;

        {
            u8 cur = tgrid[y * w + x];
            u8 nextT = T_ROAD;
            if (cur == T_RIVER || cur == T_SEA || cur == T_SHOAL || cur == T_REEF || cur == T_BRIDGE) {
                nextT = T_BRIDGE;
            } else if (cur == T_PIPE || cur == T_SEAM) {
                nextT = T_SEAM;
            }
            SetTerrainSym(tgrid, w, h, x, y, nextT, isMirror, numPlayers);
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
        u8 ter = tgrid[py * w + px];
        if (ter == T_RIVER || ter == T_BRIDGE || ter == T_SEA || ter == T_REEF || ter == T_SHOAL || ter == T_PIPE || ter == T_SEAM)
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
        /* If placing an army-owned property in mirror mode, assign to each respective corner army! */
        if (isMirror && propTile >= 453 && propTile <= 472) {
            int offsetInArmy = propTile - 453;
            int armyIdx = i + 1;
            while (offsetInArmy >= 5)
                offsetInArmy -= 5;
            if (armyIdx <= numPlayers) {
                tid = (u16)(453 + (armyIdx - 1) * 5 + offsetInArmy);
            } else {
                tid = (u16)(449 + offsetInArmy - 1); /* Neutral for unused 4th corner in 3P */
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
    /* Use destBuf + 1400..2000 as temporary 1-byte logical terrain grid */
    u8 *tgrid = destBuf + 1400;
    int hqX[5], hqY[5];
    int x, y, i, a;
    int midX, midY;

    /* 1. Choose Map Dimensions */
    if (diff == DIFF_EASY) {
        w = (numPlayers <= 2) ? 20 : 24;
        h = (numPlayers <= 2) ? 16 : 18;
    } else if (diff == DIFF_NORMAL) {
        w = (numPlayers <= 2) ? 22 : 26;
        h = (numPlayers <= 2) ? 16 : 18;
    } else if (diff == DIFF_MIRROR) {
        w = (numPlayers <= 2) ? 24 : 26;
        h = (numPlayers <= 2) ? 16 : 18;
    } else if (diff == DIFF_HARD) {
        w = (numPlayers <= 2) ? 26 : 28;
        h = (numPlayers <= 2) ? 18 : 20;
    } else {
        w = 30;
        h = 20;
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

    /* 3. Determine HQ Corner Coordinates
     * Corner 1 (OS): Top-Left
     * Corner 2 (BM): Bottom-Right
     * Corner 3 (GE): Top-Right
     * Corner 4 (YC): Bottom-Left
     */
    {
        int jx1 = isMirror ? 0 : SkirmishRandRange(&rng, 2);
        int jy1 = isMirror ? 0 : SkirmishRandRange(&rng, 2);
        int jx2 = isMirror ? jx1 : SkirmishRandRange(&rng, 2);
        int jy2 = isMirror ? jy1 : SkirmishRandRange(&rng, 2);

        hqX[1] = 3 + jx1;
        hqY[1] = 3 + jy1;

        hqX[2] = w - 4 - jx2;
        hqY[2] = h - 4 - jy2;

        hqX[3] = w - 4 - (isMirror ? jx1 : SkirmishRandRange(&rng, 2));
        hqY[3] = 3 + (isMirror ? jy1 : SkirmishRandRange(&rng, 2));

        hqX[4] = 3 + (isMirror ? jx1 : SkirmishRandRange(&rng, 2));
        hqY[4] = h - 4 - (isMirror ? jy1 : SkirmishRandRange(&rng, 2));
    }

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
        /* Naval Strait: Central N-S sea channel with shoal beaches, reefs, and sea bridge */
        for (y = 1; y < h - 1; y++) {
            int shift = isMirror ? 0 : (SkirmishRandRange(&rng, 3) - 1);
            int cx = midX + (shift / 2);
            SetTerrainSym(tgrid, w, h, cx - 1, y, T_SEA, isMirror, numPlayers);
            SetTerrainSym(tgrid, w, h, cx,     y, T_SEA, isMirror, numPlayers);
            if (!isMirror) {
                SetTerrainSym(tgrid, w, h, cx + 1, y, T_SEA, isMirror, numPlayers);
            }
            /* Shoal beaches on both sides of the channel */
            if (y >= 2 && y <= h - 3) {
                SetTerrainSym(tgrid, w, h, cx - 2, y, T_SHOAL, isMirror, numPlayers);
                if (!isMirror)
                    SetTerrainSym(tgrid, w, h, cx + 2, y, T_SHOAL, isMirror, numPlayers);
            }
        }
        /* Scatter a few Reefs in the sea channel */
        for (y = 3; y < h - 3; y += 3) {
            if (y != midY && y != midY - 1) {
                SetTerrainSym(tgrid, w, h, midX - 1, y, T_REEF, isMirror, numPlayers);
            }
        }
    } else if (biome == BIOME_PIPE) {
        /* Pipe Fortress: Black Hole Pipe walls along the central axis with Pipe Seams */
        for (y = 2; y < h - 2; y++) {
            if (y == midY || y == midY - 1)
                continue; /* Central gap for Silo objective */
            if (y == 3 || y == h - 4) {
                SetTerrainSym(tgrid, w, h, midX - 1, y, T_SEAM, isMirror, numPlayers);
                if (!isMirror)
                    SetTerrainSym(tgrid, w, h, midX, y, T_PIPE, isMirror, numPlayers);
            } else {
                SetTerrainSym(tgrid, w, h, midX - 1, y, T_PIPE, isMirror, numPlayers);
            }
        }
        if (numPlayers >= 3) {
            for (x = 3; x < w - 3; x++) {
                if (AbsInt(x - midX) <= 2)
                    continue;
                if (x == 5 || x == w - 6) {
                    SetTerrainSym(tgrid, w, h, x, midY - 1, T_SEAM, isMirror, numPlayers);
                } else if ((x & 1) == 0) {
                    SetTerrainSym(tgrid, w, h, x, midY - 1, T_PIPE, isMirror, numPlayers);
                }
            }
        }
    } else {
        /* River Heartland: Winding N-S river (plus E-W tributary on 3P/4P) */
        int rx = midX - 1;
        for (y = 1; y < h - 1; y++) {
            SetTerrainSym(tgrid, w, h, rx, y, T_RIVER, isMirror, numPlayers);
            if (!isMirror && y < h - 3 && SkirmishRandRange(&rng, 100) < 35) {
                int nx = rx + (SkirmishRandRange(&rng, 2) ? 1 : -1);
                if (nx >= midX - 3 && nx <= midX + 2) {
                    SetTerrainSym(tgrid, w, h, nx, y, T_RIVER, isMirror, numPlayers);
                    rx = nx;
                }
            }
        }
        if (numPlayers >= 3) {
            for (x = 1; x < w - 1; x++) {
                if (AbsInt(x - midX) > 2) {
                    SetTerrainSym(tgrid, w, h, x, midY - 1, T_RIVER, isMirror, numPlayers);
                }
            }
        }
    }

    /* 6. Carve Primary Road Network (connecting corners through center and along flanks) */
    CarveRoadPath(tgrid, tiles, w, h, hqX[1], hqY[1], midX - 1, midY - 1, &rng, isMirror, numPlayers);
    CarveRoadPath(tgrid, tiles, w, h, hqX[1], hqY[1], hqX[3],   hqY[3],   &rng, isMirror, numPlayers);
    CarveRoadPath(tgrid, tiles, w, h, hqX[1], hqY[1], hqX[4],   hqY[4],   &rng, isMirror, numPlayers);
    if (!isMirror) {
        CarveRoadPath(tgrid, tiles, w, h, hqX[2], hqY[2], midX,   midY,   &rng, 0, numPlayers);
        CarveRoadPath(tgrid, tiles, w, h, hqX[2], hqY[2], hqX[3], hqY[3], &rng, 0, numPlayers);
        CarveRoadPath(tgrid, tiles, w, h, hqX[2], hqY[2], hqX[4], hqY[4], &rng, 0, numPlayers);
    }

    /* Ensure clean straight bridges across the central channel/river */
    for (y = 1; y < h - 1; y++) {
        for (x = 1; x < w - 1; x++) {
            if (tgrid[y * w + x] == T_BRIDGE) {
                /* Keep surrounding E-W tiles as road/bridge so bridge never dead-ends into water */
                if (x > 1 && tgrid[y * w + (x - 1)] == T_PLAIN)
                    tgrid[y * w + (x - 1)] = T_ROAD;
                if (x < w - 2 && tgrid[y * w + (x + 1)] == T_PLAIN)
                    tgrid[y * w + (x + 1)] = T_ROAD;
            }
        }
    }

    /* 7. Place HQs & Starting Bases / Airports / Ports */
    for (a = 1; a <= numPlayers; a++) {
        int hx = hqX[a];
        int hy = hqY[a];
        int dx = (hx < midX) ? 1 : -1;
        int dy = (hy < midY) ? 1 : -1;

        /* Clear 3x3 around HQ to Plains/Roads */
        for (y = hy - 1; y <= hy + 1; y++) {
            for (x = hx - 1; x <= hx + 1; x++) {
                if (tgrid[y * w + x] != T_ROAD)
                    tgrid[y * w + x] = T_PLAIN;
            }
        }

        tiles[hy * w + hx] = TILE_ARMY_HQ(a);
        tgrid[hy * w + hx] = T_PLAIN;

        /* 2 core starting Bases adjacent to HQ */
        tiles[hy * w + (hx + dx)] = TILE_ARMY_BASE(a);
        tgrid[hy * w + (hx + dx)] = T_PLAIN;

        tiles[(hy + dy) * w + hx] = TILE_ARMY_BASE(a);
        tgrid[(hy + dy) * w + hx] = T_PLAIN;

        /* Starting Airport or extra Base depending on difficulty */
        if (a == 1 && diff == DIFF_EASY) {
            /* Player 1 advantage on Easy: extra Base + starting Airport + 2 Cities */
            tiles[(hy - dy) * w + hx] = TILE_ARMY_BASE(1);
            tgrid[(hy - dy) * w + hx] = T_PLAIN;
            tiles[hy * w + (hx - dx)] = TILE_ARMY_AIRP(1);
            tgrid[hy * w + (hx - dx)] = T_PLAIN;
            tiles[(hy + dy) * w + (hx + dx * 2)] = TILE_ARMY_CITY(1);
            tgrid[(hy + dy) * w + (hx + dx * 2)] = T_PLAIN;
            tiles[(hy + dy * 2) * w + (hx + dx)] = TILE_ARMY_CITY(1);
            tgrid[(hy + dy * 2) * w + (hx + dx)] = T_PLAIN;
        } else if (a >= 2 && (diff == DIFF_HARD || diff == DIFF_INSANE)) {
            /* AI advantage on Hard/Insane: extra Base + starting Airport + Cities */
            tiles[(hy - dy) * w + hx] = TILE_ARMY_BASE(a);
            tgrid[(hy - dy) * w + hx] = T_PLAIN;
            tiles[hy * w + (hx - dx)] = TILE_ARMY_AIRP(a);
            tgrid[hy * w + (hx - dx)] = T_PLAIN;
            if (diff == DIFF_INSANE) {
                tiles[(hy + dy) * w + (hx + dx * 2)] = TILE_ARMY_CITY(a);
                tgrid[(hy + dy) * w + (hx + dx * 2)] = T_PLAIN;
            }
        } else {
            /* Balanced starting City + neutral expansion Airport nearby */
            tiles[(hy - dy) * w + hx] = TILE_ARMY_CITY(a);
            tgrid[(hy - dy) * w + hx] = T_PLAIN;
            tiles[hy * w + (hx - dx)] = TILE_AIRP_NEUTRAL;
            tgrid[hy * w + (hx - dx)] = T_PLAIN;
        }

        /* On Insane, give AI armies a pre-owned Com Tower near their HQ! */
        if (a >= 2 && diff == DIFF_INSANE) {
            tiles[(hy + dy * 2) * w + (hx + dx)] = TILE_COM_OWNED(a);
            tgrid[(hy + dy * 2) * w + (hx + dx)] = T_PLAIN;
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
        tiles[(hy - 1) * w + hx] = TILE_CITY_NEUTRAL;
        tgrid[(hy - 1) * w + hx] = T_PLAIN;
    }

    /* 8. Place Naval Ports if in Naval Strait biome (or coastal ports) */
    if (biome == BIOME_NAVAL) {
        int pyTop = 2;
        int pyBot = h - 3;
        /* Place Ports on the coast of the central sea channel */
        for (x = midX - 3; x <= midX + 3; x++) {
            if (tgrid[pyTop * w + x] == T_SHOAL) {
                tiles[pyTop * w + x] = (numPlayers >= 1) ? TILE_ARMY_PORT(1) : TILE_PORT_NEUTRAL;
                tgrid[pyTop * w + x] = T_SEA;
                break;
            }
        }
        for (x = midX + 3; x >= midX - 3; x--) {
            if (tgrid[pyBot * w + x] == T_SHOAL) {
                tiles[pyBot * w + x] = TILE_ARMY_PORT(2);
                tgrid[pyBot * w + x] = T_SEA;
                break;
            }
        }
        /* Contested Neutral Ports near mid-channel */
        if (tgrid[(midY - 2) * w + (midX - 2)] == T_SHOAL) {
            tiles[(midY - 2) * w + (midX - 2)] = TILE_PORT_NEUTRAL;
            tgrid[(midY - 2) * w + (midX - 2)] = T_SEA;
        }
        if (tgrid[(midY + 2) * w + (w - 1 - (midX - 2))] == T_SHOAL) {
            tiles[(midY + 2) * w + (w - 1 - (midX - 2))] = TILE_PORT_NEUTRAL;
            tgrid[(midY + 2) * w + (w - 1 - (midX - 2))] = T_SEA;
        }
    }

    /* 9. Place Centerpiece Objective: Central Missile Silo (384) & Contested Com Towers (441) */
    {
        int sx = (biome == BIOME_NAVAL) ? (midX - 4) : (midX - 1);
        int sy = midY - 1;
        if (tgrid[sy * w + sx] == T_PLAIN || tgrid[sy * w + sx] == T_ROAD) {
            tiles[sy * w + sx] = TILE_SILO;
            tgrid[sy * w + sx] = T_PLAIN;
            if (isMirror || biome == BIOME_NAVAL) {
                tiles[(h - 1 - sy) * w + (w - 1 - sx)] = TILE_SILO;
                tgrid[(h - 1 - sy) * w + (w - 1 - sx)] = T_PLAIN;
            }
        }
        /* Place symmetric Contested Neutral Com Towers on the flanks */
        TryPlaceProperty(tgrid, tiles, w, h, midX - 4, 3, TILE_COM_NEUTRAL, 1, numPlayers, 1);
        if (!isMirror) {
            TryPlaceProperty(tgrid, tiles, w, h, w - 1 - (midX - 4), h - 4, TILE_COM_NEUTRAL, 0, numPlayers, 1);
        }
    }

    /* 10. Place Neutral Bases, Airports, and Cities across the map */
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
            if (isMirror && x >= midX)
                continue;
            if (isMirror && numPlayers >= 3 && y >= midY)
                continue;
            if (tgrid[y * w + x] != T_PLAIN || tiles[y * w + x] != 0)
                continue;

            /* Keep immediate 1-tile access around HQ clear of mountains */
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

    /* Make sure mirrored properties are not overwritten by terrain */
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            if (tiles[y * w + x] != 0 && tiles[y * w + x] != TILE_PORT_NEUTRAL &&
                tiles[y * w + x] != TILE_ARMY_PORT(1) && tiles[y * w + x] != TILE_ARMY_PORT(2) &&
                tiles[y * w + x] != TILE_ARMY_PORT(3) && tiles[y * w + x] != TILE_ARMY_PORT(4)) {
                tgrid[y * w + x] = T_PLAIN;
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
                int ewRoad = 0;
                if (x > 0 && (tgrid[y * w + (x - 1)] == T_ROAD || tgrid[y * w + (x - 1)] == T_BRIDGE || !IsWaterTerrain(tgrid[y * w + (x - 1)])))
                    ewRoad = 1;
                if (y > 0 && (tgrid[(y - 1) * w + x] == T_RIVER || tgrid[(y - 1) * w + x] == T_SEA))
                    ewRoad = 1;
                tiles[y * w + x] = ewRoad ? TILE_BRIDGE_EW : TILE_BRIDGE_NS;
            } else if (t == T_SEAM) {
                int nsPipe = 0;
                if (y > 0 && (tgrid[(y - 1) * w + x] == T_PIPE || tgrid[(y - 1) * w + x] == T_SEAM))
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
                if (y <= 1     || tgrid[(y - 1) * w + x] == T_RIVER || IsWaterTerrain(tgrid[(y - 1) * w + x])) m |= 1;
                if (y >= h - 2 || tgrid[(y + 1) * w + x] == T_RIVER || IsWaterTerrain(tgrid[(y + 1) * w + x])) m |= 2;
                if (x <= 1     || tgrid[y * w + (x - 1)] == T_RIVER || IsWaterTerrain(tgrid[y * w + (x - 1)])) m |= 4;
                if (x >= w - 2 || tgrid[y * w + (x + 1)] == T_RIVER || IsWaterTerrain(tgrid[y * w + (x + 1)])) m |= 8;
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

    /* 14. Clean up temporary terrain scratch buffer */
    for (i = 0; i < w * h; i++) {
        tgrid[i] = 0;
    }
}

/* Called by DecompressMap hook at 0x020C6A80 */
int SkirmishDecompressMap(const void *mapLz, u8 *destBuf, u16 mapId, u32 callerLr)
{
    int numPlayers = 2;
    int diff = 0;
    int isSkirmish = SkirmishDecodeMapId(mapId, &numPlayers, &diff);

    (void)mapLz;

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

    /* New Battle Start (InitBattleMap at 0x020C7718 -> lr = 0x020C771C)
     * vs Save / Continue (0x02018500 / 0x02018B3C)
     */
    {
        u32 seed = 0;
        if (callerLr == 0x020C771C) {
            u32 vcount = *(volatile u16 *)0x04000006;
            u32 t0     = *(volatile u16 *)0x04000100;
            u32 frame  = *(volatile u32 *)0x027FFFC0;
            gEntropySalt += 0x9E3779B9;
            seed = gLastBattleSeed ^ (vcount << 16) ^ (t0 << 5) ^ frame ^ gEntropySalt ^ ((u32)mapId * 0x45D9F3B);
            SkirmishStepRng(&seed);
            SkirmishStepRng(&seed);
            gLastBattleSeed = seed;
        } else {
            u8 *mapCfg = GetMapCfgPtr();
            if (mapCfg)
                seed = *(u32 *)(mapCfg + 0x28);
            if (seed == 0)
                seed = gLastBattleSeed;
        }

        SkirmishGenerateBattleMap(destBuf, numPlayers, diff, seed);
    }

    return 1;
}
