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
#define TILE_MTN           32
#define TILE_SEA           42
#define TILE_ROAD_NS_SHAD  128
#define TILE_WOOD          134
#define TILE_WOOD_SHADOW   135
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
    25, 120,  88, 216,  25, 283, 280,  27
};

static const u16 sPipeTileTable[16] = {
    322, 259, 258, 323, 289, 353, 321, 323,
    288, 352, 320, 323, 322, 322, 322, 322
};

/* Sea & Shoal tables indexed by 4-bit cardinal LAND mask (N=1, S=2, W=4, E=8) */
static const u16 sSeaTileTable[16] = {
    42,  74,  10, 108,  44,  75,  43, 106,
    40,  73,  41, 139,  12, 104, 137,   8
};

static const u16 sShoalTileTable[16] = {
    16,  80,  16,  80,  50, 274, 242,  83,
    46, 275, 243,  84,  46, 117,  85, 117
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
     * this map's default teams (COSelectInit at 0x0206ED48 then copies session + 0xA2..0xA5
     * into the CO Select menu state at 0x022CD3B8..0x022CD3BB). We guard the non-Skirmish
     * reset with session[0x58] == 2 (Versus Mode) so Campaign/Trial preset teams are
     * untouched, and we do NOT overwrite session + 0xA2..0xA5 when starting the battle
     * (parentLr = 0x020BDAC8), preserving manual team customizations from CO Select!
     */
    if (callerLr == 0x020C7828) {
        u8 *session = GetSessionPtr();
        u8 *mapCfg  = GetMapCfgPtr();
        if (mapCfg && isSkirmish) {
            mapCfg[0x14] = (u8)(gLastBattleSeed & 0xFF);
            mapCfg[0x15] = (u8)((gLastBattleSeed >> 8) & 0xFF);
            mapCfg[0x16] = (u8)((gLastBattleSeed >> 16) & 0xFF);
            mapCfg[0x17] = (u8)((gLastBattleSeed >> 24) & 0xFF);
        }
        if (session && parentLr == 0x020C73C4 && (isSkirmish || session[0x58] == 2)) {
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
            if (y == 0)
                tid = (x == 0) ? 42 : ((x == w - 1) ? 42 : 10);
            else if (y == h - 1)
                tid = (x == 0) ? 42 : ((x == w - 1) ? 42 : 74);
            else if (x == 0)
                tid = 40;
            else if (x == w - 1)
                tid = 44;
            tiles[y * w + x] = tid;
        }
    }
    tiles[1 * w + 0] = 73;
    tiles[0 * w + 1] = 41;
    tiles[1 * w + (w - 1)] = 75;
    tiles[0 * w + (w - 2)] = 43;
    tiles[(h - 2) * w + 0] = 41;
    tiles[(h - 1) * w + 1] = 73;
    tiles[(h - 2) * w + (w - 1)] = 43;
    tiles[(h - 1) * w + (w - 2)] = 75;

    /* Draw dark mountain drop-shadow behind the '?' so it pops on the minimap */
    for (y = 0; y < 9; y++) {
        u8 rowBits = sQuestionRows[y];
        for (x = 0; x < 5; x++) {
            if (rowBits & (1 << (4 - x))) {
                int gx = 6 + x;
                int gy = 2 + y;
                if (gx - 1 >= 1)
                    tiles[gy * w + (gx - 1)] = TILE_MTN;
                if (gy + 1 < h - 1)
                    tiles[(gy + 1) * w + gx] = TILE_MTN;
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

    /* Place active Army HQs, Bases, Cities, Airports, Com Towers & Silos in the corners
     * so GetMapArmyPresent enables the exact 2, 3, or 4 CO slots AND the SELECT MAP
     * 6-icon property summary box displays the active objective types!
     */
    tiles[2 * w + 2] = TILE_ARMY_HQ(1);
    tiles[2 * w + 3] = TILE_ARMY_BASE(1);
    tiles[3 * w + 2] = TILE_ARMY_BASE(1);
    tiles[1 * w + 2] = TILE_ARMY_CITY(1);
    tiles[2 * w + 1] = TILE_AIRP_NEUTRAL;
    tiles[1 * w + 1] = TILE_COM_NEUTRAL;
    tiles[11 * w + 1] = TILE_SILO;

    tiles[10 * w + 14] = TILE_ARMY_HQ(2);
    tiles[10 * w + 13] = TILE_ARMY_BASE(2);
    tiles[9 * w + 14]  = TILE_ARMY_BASE(2);
    tiles[11 * w + 14] = TILE_ARMY_CITY(2);
    tiles[10 * w + 15] = TILE_AIRP_NEUTRAL;
    tiles[11 * w + 15] = TILE_COM_NEUTRAL;
    tiles[1 * w + 15]  = TILE_SILO;

    if (numPlayers >= 3) {
        tiles[2 * w + 14] = TILE_ARMY_HQ(3);
        tiles[2 * w + 13] = TILE_ARMY_BASE(3);
        tiles[3 * w + 14] = TILE_ARMY_BASE(3);
        tiles[1 * w + 14] = TILE_ARMY_CITY(3);
        tiles[2 * w + 15] = TILE_AIRP_NEUTRAL;
    }
    if (numPlayers >= 4) {
        tiles[10 * w + 2] = TILE_ARMY_HQ(4);
        tiles[10 * w + 3] = TILE_ARMY_BASE(4);
        tiles[9 * w + 2]  = TILE_ARMY_BASE(4);
        tiles[11 * w + 2] = TILE_ARMY_CITY(4);
        tiles[10 * w + 1] = TILE_AIRP_NEUTRAL;
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

static int IsVehicleBlockedTerrain(u8 t, u16 tid)
{
    if (t == T_MTN || t == T_SEA || t == T_RIVER || t == T_PIPE || t == T_REEF)
        return 1;
    if (tid == TILE_BASE_NEUTRAL || tid == TILE_PORT_NEUTRAL ||
        tid == TILE_ARMY_PORT(1) || tid == TILE_ARMY_PORT(2) ||
        tid == TILE_ARMY_PORT(3) || tid == TILE_ARMY_PORT(4))
        return 1;
    return 0;
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

/* Ensure Mountains never form 8-connected impassable chains with other Mountains, Rivers,
 * Pipes, Ports, or Sea that could wall off a Neutral Base/City or block vehicle corridors.
 */
static int CanPlaceMountainAt(u8 *tgrid, u16 *tiles, int w, int h, int x, int y, int isMirror, int numPlayers)
{
    int pts[4][2];
    int count = 1;
    int i, j, dx, dy;

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
        for (j = i + 1; j < count; j++) {
            if (AbsInt(pts[i][0] - pts[j][0]) <= 1 && AbsInt(pts[i][1] - pts[j][1]) <= 1)
                return 0;
        }
    }

    for (i = 0; i < count; i++) {
        int px = pts[i][0];
        int py = pts[i][1];
        for (dy = -1; dy <= 1; dy++) {
            for (dx = -1; dx <= 1; dx++) {
                int nx = px + dx;
                int ny = py + dy;
                if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                    if (IsVehicleBlockedTerrain(tgrid[ny * w + nx], tiles[ny * w + nx]))
                        return 0;
                }
            }
        }
    }
    return 1;
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
    int i, j, dy, dx;

    if (x < 1 || x > w - 2 || y < 1 || y > h - 2)
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
        for (i = 0; i < count; i++) {
            for (j = i + 1; j < count; j++) {
                if (AbsInt(pts[i][0] - pts[j][0]) <= minBuildingDist &&
                    AbsInt(pts[i][1] - pts[j][1]) <= minBuildingDist)
                    return 0;
            }
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
        w = 25;
        h = 17;
    } else if (diff == DIFF_NORMAL) {
        w = (numPlayers <= 2) ? 25 : 27;
        h = 17;
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
                if ((x == midX - 3 || x == midX + 3) && y > 3 && y < h - 4) {
                    tgrid[y * w + x] = T_SHOAL;
                } else {
                    tgrid[y * w + x] = T_SEA;
                }
            }
        }
        /* Scatter symmetric Reefs in the Open Sea Basin (separated horizontally & vertically) */
        SetTerrainSym(tgrid, w, h, midX - 1, midY - 2, T_REEF, 1, 4);
    } else if (biome == BIOME_PIPE) {
        /* Pipe Fortress: Continuous 1-tile-thick Black Hole Pipe wall along x = midX
         * (plus flush E-W pipe arms along y = midY in 3P/4P), with zero open gaps!
         */
        for (y = 1; y < h - 1; y++) {
            tgrid[y * w + midX] = T_PIPE;
        }
        if (numPlayers >= 3) {
            for (x = 1; x < w - 1; x++) {
                if (x == midX)
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
        /* Clean single-track highway system around the Open Sea Basin:
         * - North Coastal Highway at y = 2 (x = 4 .. w - 5, right outside the (4, 3) & (3, 2) Bases!)
         * - South Coastal Highway at y = h - 3 (x = 4 .. w - 5, right outside the (4, h - 4) & (3, h - 3) Bases!)
         * - West Trunk at x = 3 (y = 3 .. h - 4) & East Trunk at x = w - 4 (y = 3 .. h - 4)
         * - Mid-Basin Port Spurs at y = midY (x = 3 .. midX - 5 and midX + 5 .. w - 4)
         */
        CarveRoadSegment(tgrid, tiles, w, h, 3,        2,     w - 4,    2);
        CarveRoadSegment(tgrid, tiles, w, h, 3,        h - 3, w - 4,    h - 3);
        CarveRoadSegment(tgrid, tiles, w, h, 3,        2,     3,        h - 3);
        CarveRoadSegment(tgrid, tiles, w, h, w - 4,    2,     w - 4,    h - 3);
        CarveRoadSegment(tgrid, tiles, w, h, 3,        midY,  midX - 5, midY);
        CarveRoadSegment(tgrid, tiles, w, h, midX + 5, midY,  w - 4,    midY);
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
            /* In 3P/4P River Heartland, carve two inner N-S highway trunks at x = midX - 3
             * and x = midX + 3 (from y = 3 to y = h - 4) bridging the E-W river on both sides
             * of the central crossroads while staying well-separated from y = 3 and y = h - 4!
             */
            CarveRoadSegment(tgrid, tiles, w, h, midX - 3, 3, midX - 3, h - 4);
            CarveRoadSegment(tgrid, tiles, w, h, midX + 3, 3, midX + 3, h - 4);
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
         * (Bonus properties placed within the outer HQ corner at (hx-dx, hy-dy) and (hx-dx, hy+dy)
         * so they never block any highway or inner approach!)
         */
        if (a == 1 && diff == DIFF_EASY) {
            /* Player 1 advantage on Easy: extra Base + starting Airport + 2 Cities */
            tiles[(hy - dy) * w + hx] = TILE_ARMY_BASE(1);
            tgrid[(hy - dy) * w + hx] = T_PLAIN;
            tiles[hy * w + (hx - dx)] = TILE_ARMY_AIRP(1);
            tgrid[hy * w + (hx - dx)] = T_PLAIN;
            tiles[(hy - dy) * w + (hx - dx)] = TILE_ARMY_CITY(1);
            tgrid[(hy - dy) * w + (hx - dx)] = T_PLAIN;
            tiles[(hy + dy) * w + (hx - dx)] = TILE_ARMY_CITY(1);
            tgrid[(hy + dy) * w + (hx - dx)] = T_PLAIN;
        } else if (a >= 2 && (diff == DIFF_HARD || diff == DIFF_INSANE)) {
            /* AI advantage on Hard/Insane: extra Base + starting Airport + City */
            tiles[(hy - dy) * w + hx] = TILE_ARMY_BASE(a);
            tgrid[(hy - dy) * w + hx] = T_PLAIN;
            tiles[hy * w + (hx - dx)] = TILE_ARMY_AIRP(a);
            tgrid[hy * w + (hx - dx)] = T_PLAIN;
            if (diff == DIFF_INSANE) {
                tiles[(hy + dy) * w + (hx - dx)] = TILE_ARMY_CITY(a);
                tgrid[(hy + dy) * w + (hx - dx)] = T_PLAIN;
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

    /* 8. Place Naval Ports if in Naval Strait biome
     * Ports sit on the West & East coastal land edge (midX - 4 and midX + 4) with
     * deep-water Harbor Mouth sea channels (107 and 138) at (midX - 3 and midX + 3).
     */
    if (biome == BIOME_NAVAL) {
        int pyTop = 4;
        int pyBot = h - 5;
        tiles[pyTop * w + (midX - 4)] = TILE_ARMY_PORT(1);
        tgrid[pyTop * w + (midX - 3)] = T_SEA;

        tiles[pyBot * w + (midX + 4)] = TILE_ARMY_PORT(2);
        tgrid[pyBot * w + (midX + 3)] = T_SEA;

        tiles[pyTop * w + (midX + 4)] = (numPlayers >= 3) ? TILE_ARMY_PORT(3) : TILE_PORT_NEUTRAL;
        tgrid[pyTop * w + (midX + 3)] = T_SEA;

        tiles[pyBot * w + (midX - 4)] = (numPlayers >= 4) ? TILE_ARMY_PORT(4) : TILE_PORT_NEUTRAL;
        tgrid[pyBot * w + (midX - 3)] = T_SEA;

        /* Contested Neutral Ports at mid-basin */
        tiles[midY * w + (midX - 4)] = TILE_PORT_NEUTRAL;
        tgrid[midY * w + (midX - 3)] = T_SEA;

        tiles[midY * w + (midX + 4)] = TILE_PORT_NEUTRAL;
        tgrid[midY * w + (midX + 3)] = T_SEA;
    }

    /* 9. Place Centerpiece Objective: Central/Coastal Missile Silos (384) & Contested Com Towers (441)
     * Placed along the outer North/South coastal highways (y = 1 / h - 2) in Naval Basin or
     * at (midX - 2, midY - 2) and (midX - 4, 2) in River/Pipe so they are well-separated
     * from all Ports, HQ clusters, roads, rivers, and pipe walls!
     */
    TryPlaceProperty(tgrid, tiles, w, h,
                     (biome == BIOME_NAVAL) ? (midX - 1) : (midX - 2),
                     (biome == BIOME_NAVAL) ? 1 : (midY - 2),
                     TILE_SILO, 1, numPlayers, 1);

    /* Place symmetric Contested Neutral Com Towers on the flanks (without overwriting roads) */
    TryPlaceProperty(tgrid, tiles, w, h,
                     midX - 4,
                     (biome == BIOME_NAVAL) ? 1 : 2,
                     TILE_COM_NEUTRAL, 1, numPlayers, 1);

    /* 10. Place Neutral Bases and Cities across the map (stratified by quadrant for fairness) */
    if (isMirror) {
        int targetNeutralBases = (numPlayers >= 3) ? 1 : 2;
        int targetCities       = (numPlayers >= 3) ? 3 : 6;
        int placed = 0;
        int tries;

        for (tries = 0; tries < 100 && placed < targetNeutralBases; tries++) {
            int minDist = (tries < 40) ? 2 : 1;
            int rx = 2 + SkirmishRandRange(&rng, midX - 3);
            int ry = ((((tries < 60) ? placed : tries) & 1) ? (midY + 2) : 2) + SkirmishRandRange(&rng, midY - 3);
            if (TryPlaceProperty(tgrid, tiles, w, h, rx, ry, TILE_BASE_NEUTRAL, 1, numPlayers, minDist)) {
                placed++;
            }
        }

        placed = 0;
        for (tries = 0; tries < 200 && placed < targetCities; tries++) {
            int rx = 1 + SkirmishRandRange(&rng, midX - 1);
            int y0 = (numPlayers == 2 && (((tries < 120) ? placed : tries) & 1)) ? (midY + 1) : 1;
            int ry = y0 + SkirmishRandRange(&rng, midY - 1);
            if (TryPlaceProperty(tgrid, tiles, w, h, rx, ry, TILE_CITY_NEUTRAL, 1, numPlayers, 1)) {
                placed++;
            }
        }
    } else {
        /* Non-mirror modes: place 1 Neutral Base and (3 + (diff >> 1)) Neutral Cities
         * in each of the 4 quadrants so every quadrant has identical economy!
         */
        int q;
        int quadCities = 3 + (diff >> 1);

        for (q = 0; q < 4; q++) {
            int x0 = (q & 1) ? (midX + 1) : 1;
            int xSpan = (midX - 1 > 1) ? (midX - 1) : 1;
            int y0 = (q & 2) ? (midY + 1) : 1;
            int ySpan = (midY - 1 > 1) ? (midY - 1) : 1;
            int placed = 0;
            int tries;

            for (tries = 0; tries < 100; tries++) {
                int minDist = (tries < 40) ? 2 : 1;
                int rx = x0 + 1 + SkirmishRandRange(&rng, xSpan - 2);
                int ry = y0 + 1 + SkirmishRandRange(&rng, ySpan - 2);
                if (TryPlaceProperty(tgrid, tiles, w, h, rx, ry, TILE_BASE_NEUTRAL, 0, numPlayers, minDist))
                    break;
            }
            for (tries = 0; tries < 100 && placed < quadCities; tries++) {
                int rx = x0 + SkirmishRandRange(&rng, xSpan);
                int ry = y0 + SkirmishRandRange(&rng, ySpan);
                if (TryPlaceProperty(tgrid, tiles, w, h, rx, ry, TILE_CITY_NEUTRAL, 0, numPlayers, 1))
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

            /* Keep immediate 2-tile access around HQ, central courtyard, and Naval beachheads clear */
            if (AbsInt(x - midX) <= 2 && AbsInt(y - midY) <= 2)
                continue;
            if (biome == BIOME_NAVAL && AbsInt(x - midX) == 4 && y >= 3 && y <= h - 4)
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
                if (CanPlaceMountainAt(tgrid, tiles, w, h, x, y, isMirror, numPlayers)) {
                    SetTerrainSym(tgrid, w, h, x, y, T_MTN, isMirror, numPlayers);
                } else {
                    SetTerrainSym(tgrid, w, h, x, y, T_WOOD, isMirror, numPlayers);
                }
            }
        }
    }

    /* Make sure all placed properties sit on T_PLAIN for coastline/road auto-tiling */
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            if (tiles[y * w + x] != 0) {
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
                tiles[y * w + x] = TILE_MTN;
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
                if (x == midX) {
                    if (y > 0     && (tgrid[(y - 1) * w + x] == T_PIPE || tgrid[(y - 1) * w + x] == T_SEAM)) m |= 1;
                    if (y < h - 1 && (tgrid[(y + 1) * w + x] == T_PIPE || tgrid[(y + 1) * w + x] == T_SEAM)) m |= 2;
                } else {
                    if (x > 0     && (x - 1) != midX && (tgrid[y * w + (x - 1)] == T_PIPE || tgrid[y * w + (x - 1)] == T_SEAM)) m |= 4;
                    if (x < w - 1 && (x + 1) != midX && (tgrid[y * w + (x + 1)] == T_PIPE || tgrid[y * w + (x + 1)] == T_SEAM)) m |= 8;
                }
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
                tiles[y * w + x] = (m == 12 && x < midX) ? 57 : sRiverTileTable[m];
            } else if (t == T_SHOAL) {
                int m = 0;
                u16 st;
                if (y > 0     && !IsWaterTerrain(tgrid[(y - 1) * w + x])) m |= 1;
                if (y < h - 1 && !IsWaterTerrain(tgrid[(y + 1) * w + x])) m |= 2;
                if (x > 0     && !IsWaterTerrain(tgrid[y * w + (x - 1)])) m |= 4;
                if (x < w - 1 && !IsWaterTerrain(tgrid[y * w + (x + 1)])) m |= 8;
                st = sShoalTileTable[m];
                if (m == 4 || m == 8) {
                    int nShoal = (tgrid[(y - 1) * w + x] == T_SHOAL);
                    int sShoal = (tgrid[(y + 1) * w + x] == T_SHOAL);
                    if (!nShoal && sShoal)
                        st = (m == 4) ? 176 : 175;
                    else if (nShoal && !sShoal)
                        st = (m == 4) ? 208 : 207;
                    else if (!nShoal && !sShoal)
                        st = (m == 4) ? 147 : 146;
                }
                tiles[y * w + x] = st;
            } else if (t == T_SEA) {
                /* Check for River Mouth / Estuary tiles along the Sea border */
                if (y == 0 && tgrid[1 * w + x] == T_RIVER) {
                    tiles[y * w + x] = 105;
                } else if (y == h - 1 && tgrid[(h - 2) * w + x] == T_RIVER) {
                    tiles[y * w + x] = 284;
                } else if (x == 0 && tgrid[y * w + 1] == T_RIVER) {
                    tiles[y * w + x] = 285;
                } else if (x == w - 1 && tgrid[y * w + (w - 2)] == T_RIVER) {
                    tiles[y * w + x] = 253;
                } else {
                    int m = 0;
                    if (y > 0     && !IsWaterTerrain(tgrid[(y - 1) * w + x])) m |= 1;
                    if (y < h - 1 && !IsWaterTerrain(tgrid[(y + 1) * w + x])) m |= 2;
                    if (x > 0     && !IsWaterTerrain(tgrid[y * w + (x - 1)])) m |= 4;
                    if (x < w - 1 && !IsWaterTerrain(tgrid[y * w + (x + 1)])) m |= 8;
                    if (m != 0) {
                        u16 st = sSeaTileTable[m];
                        if (m == 4 && x == midX - 3)
                            st = 107;
                        else if (m == 8 && x == midX + 3)
                            st = 138;
                        tiles[y * w + x] = st;
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
                }
            } else {
                tiles[y * w + x] = TILE_PLAIN;
            }
        }
    }

    /* 13. Apply West-of-Tall-Tile Shadows to Plains, Woods, and Roads */
    for (y = 0; y < h; y++) {
        for (x = 1; x < w; x++) {
            u16 west = tiles[y * w + (x - 1)];
            int westTall = (west == TILE_MTN || west == TILE_WOOD || west == TILE_WOOD_SHADOW ||
                            west == TILE_SILO || west >= 441);
            if (westTall) {
                u16 cur = tiles[y * w + x];
                if (cur == TILE_PLAIN)
                    tiles[y * w + x] = TILE_PLAIN_SHADOW;
                else if (cur == TILE_WOOD)
                    tiles[y * w + x] = TILE_WOOD_SHADOW;
                else if (tgrid[y * w + x] == T_ROAD)
                    tiles[y * w + x] = (u16)(cur + ((cur <= 98) ? 64 : ((cur == 256) ? 1 : 2)));
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
     * return the "Surprise Me!" Giant '?' minimap preview!
     */
    if (callerLr >= 0x0202CA00 && callerLr < 0x0202D700) {
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
            if (mapCfg) {
                seed = (u32)mapCfg[0x14] |
                       ((u32)mapCfg[0x15] << 8) |
                       ((u32)mapCfg[0x16] << 16) |
                       ((u32)mapCfg[0x17] << 24);
            }
            if (seed == 0)
                seed = gLastBattleSeed;
            else
                gLastBattleSeed = seed;
        }

        SkirmishGenerateBattleMap(destBuf, numPlayers, diff, seed);
    }

    return 1;
}
