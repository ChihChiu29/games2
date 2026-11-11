#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

/* ARM9 Hook Trampolines (called from ARM mode at 0x020C6874, 0x020C6A80, 0x0202E114) */
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
    "\n"
    ".global HookIsCategoryUnlocked\n"
    ".type HookIsCategoryUnlocked, %function\n"
    "HookIsCategoryUnlocked:\n"
    "    cmp     r0, #9\n"
    "    bne     0x02105174\n"
    "    push    {r1, lr}\n"
    "    blx     SkirmishIsCategoryUnlocked\n"
    "    pop     {r1, lr}\n"
    "    bx      lr\n"
    ".pool\n"
    ".thumb\n"
    ".align 2\n"
);

#define MAX_W 30
#define MAX_H 20
#define MAX_TILES (MAX_W * MAX_H)

/* Dedicated Random Tab (tabId == 7, cat == 9) map IDs: 188..198 */
#define SKIRMISH_MAP_BASE   188
#define SKIRMISH_MODE_COUNT 11

#define TTEAM(a, b, c, d) ((u8)((a) | ((b) << 2) | ((c) << 4) | ((d) << 6)))

struct SkirmishModeConfig {
    u8 numArmies;
    u8 mirror;
    u8 teamMask;
    const char *name;  /* Short label for 3-column SELECT MAP grid */
    const char *title; /* Full title for top preview banner & in-battle info */
};

static const struct SkirmishModeConfig sSkirmishModes[SKIRMISH_MODE_COUNT] = {
    /* 0: 1v1 */     { 2, 0, TTEAM(0, 1, 2, 3), "1v1",     "Random 1v1"   },
    /* 1: MIR 1v1 */ { 2, 1, TTEAM(0, 1, 2, 3), "MIR 1v1", "Mirror 1v1"   },
    /* 2: 1v1v1 */   { 3, 0, TTEAM(0, 1, 2, 3), "1v1v1",   "Random 1v1v1" },
    /* 3: 1v2 */     { 3, 0, TTEAM(0, 1, 1, 3), "1v2",     "Outnumbered"  },
    /* 4: 2v1 */     { 3, 0, TTEAM(0, 0, 1, 3), "2v1",     "Boss Raid"    },
    /* 5: 1v1v1v1 */ { 4, 0, TTEAM(0, 1, 2, 3), "1v1v1v1", "Random FFA"   },
    /* 6: MIR FFA */ { 4, 1, TTEAM(0, 1, 2, 3), "MIR FFA", "Mirror FFA"   },
    /* 7: 2v2 */     { 4, 0, TTEAM(0, 0, 1, 1), "2v2",     "Team Battle"  },
    /* 8: MIR 2v2 */ { 4, 1, TTEAM(0, 0, 1, 1), "MIR 2v2", "Mirror Teams" },
    /* 9: 1v1v2 */   { 4, 0, TTEAM(0, 1, 2, 2), "1v1v2",   "Three Teams"  },
    /* 10: 1v3 */    { 4, 0, TTEAM(0, 1, 1, 1), "1v3",     "Survival"     },
};

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
#define TILE_WOOD          134
#define TILE_WOOD_SHADOW   135
#define TILE_ROAD_CROSS    256
#define TILE_SEAM_EW       354
#define TILE_SEAM_NS       355
#define TILE_REEF          360
#define TILE_SILO          384

#define TILE_COM_NEUTRAL   441
#define TILE_BASE_NEUTRAL  449
#define TILE_CITY_NEUTRAL  450
#define TILE_AIRP_NEUTRAL  451
#define TILE_PORT_NEUTRAL  452

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

/* Default Versus CO IDs (Andy, Max, Sami, Olaf, Eagle, Drake, Kanbei, Sonja) */
static const u8 sDefaultVersusCos[8] = { 2, 3, 4, 6, 9, 10, 7, 8 };

/* Persistent state inside OV9 5 */
static u32 gLastBattleSeed = 0xA55A3C69;
static u32 gEntropySalt    = 0x13579BDF;

static const struct SkirmishModeConfig *GetSkirmishModeConfig(u16 mapId)
{
    int idx = (int)mapId - SKIRMISH_MAP_BASE;
    if (idx < 0 || idx >= SKIRMISH_MODE_COUNT)
        return 0;
    return &sSkirmishModes[idx];
}

static __attribute__((noinline)) u32 SkirmishStepRng(u32 *rng)
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

static __attribute__((noinline)) int SkirmishRandRange(u32 *rng, int bound)
{
    if (bound <= 1)
        return 0;
    return (int)((((u32)(SkirmishStepRng(rng) & 0xFFFF)) * (u32)bound) >> 16);
}

static __attribute__((noinline)) u8 *GetSessionPtr(void)
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

/* Randomize Primary + Tag CO combinations (session[0x99..0xA0]) across all 4 armies
 * using unlocked CO IDs (1..27) from 0x022D65A4 so every army gets a distinct CO pair.
 */
static void SkirmishRandomizeSessionCos(u8 *session)
{
    u8 pool[28];
    int count = 0;
    int i;
    u32 rng;

    *(volatile u32 *)0x022D65A4 |= (*(volatile u32 *)0x022D65A8) | 0x000007FEu;

    for (i = 1; i <= 27; i++) {
        if ((*(volatile u8 *)(0x022D65A4 + (i >> 3))) & (1 << (i & 7)))
            pool[count++] = (u8)i;
    }
    if (count < 8) {
        for (i = 0; i < 8; i++)
            pool[i] = sDefaultVersusCos[i];
        count = 8;
    }

    rng = gLastBattleSeed ^ gEntropySalt ^
          ((u32)(*(volatile u16 *)0x04000006) << 16) ^
          ((u32)(*(volatile u16 *)0x04000100) << 5) ^
          (*(volatile u32 *)0x027FFFC0);
    SkirmishStepRng(&rng);

    for (i = count - 1; i > 0; i--) {
        int j = SkirmishRandRange(&rng, i + 1);
        u8 tmp = pool[i];
        pool[i] = pool[j];
        pool[j] = tmp;
    }

    for (i = 0; i < 8; i++)
        session[0x99 + i] = pool[i];
}

/* Called by HookIsCategoryUnlocked at 0x0202E114 when cat == 9 (tabId == 7) */
int SkirmishIsCategoryUnlocked(void)
{
    u8 *session = GetSessionPtr();
    *(volatile u8 *)(0x022D6582 + 23) |= 0xF0;
    *(volatile u8 *)(0x022D6582 + 24) |= 0x7F;
    if (session && session[0x8A] != 0)
        return 0;
    return 1;
}

/* Called by GetMapName hook at 0x020C6874 */
const char *SkirmishGetMapName(u16 mapId, u32 callerLr, u32 parentLr)
{
    const struct SkirmishModeConfig *cfg = GetSkirmishModeConfig(mapId);

    if (callerLr == 0x020C7828) {
        u8 *session = GetSessionPtr();
        u8 *mapCfg  = GetMapCfgPtr();
        int i;
        if (mapCfg && cfg) {
            for (i = 0; i < 4; i++)
                mapCfg[0x14 + i] = (u8)(gLastBattleSeed >> (i * 8));
        }
        if (session && parentLr == 0x020C73C4) {
            if (cfg) {
                session[0x93] = 1;
                for (i = 0; i < 4; i++) {
                    if (i > 0)
                        session[0x93 + i] = 2;
                    session[0xA2 + i] = (cfg->teamMask >> (i * 2)) & 3;
                }
                SkirmishRandomizeSessionCos(session);
            } else if (session[0x58] == 2) {
                for (i = 0; i < 4; i++)
                    session[0xA2 + i] = (u8)i;
                if (session[0x99] == session[0x9B]) {
                    for (i = 0; i < 8; i++)
                        session[0x99 + i] = sDefaultVersusCos[i];
                }
            }
        }
    }

    if (!cfg)
        return 0;

    return (callerLr == 0x020662D8) ? cfg->name : cfg->title;
}

/* Generate the "Surprise Me!" Giant '?' Question Mark minimap preview (17x13) */
static void SkirmishGenerateQuestionPreview(u8 *destBuf, int numPlayers)
{
    const int w = 17;
    const int h = 13;
    u16 *tiles = (u16 *)(destBuf + 2);
    int x, y;

    static const u8 sQuestionRows[9] = {
        0x0E, 0x1B, 0x03, 0x06, 0x04, 0x04, 0x00, 0x04, 0x04
    };

    destBuf[0] = (u8)w;
    destBuf[1] = (u8)h;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            u16 tid = TILE_PLAIN;
            if (y == 0)
                tid = (x == 0 || x == w - 1) ? 42 : 10;
            else if (y == h - 1)
                tid = (x == 0 || x == w - 1) ? 42 : 74;
            else if (x == 0)
                tid = 40;
            else if (x == w - 1)
                tid = 44;
            tiles[y * w + x] = tid;
        }
    }

    for (y = 0; y < 9; y++) {
        u8 rowBits = sQuestionRows[y];
        for (x = 0; x < 5; x++) {
            if (rowBits & (1 << (4 - x))) {
                int idx = (2 + y) * w + (6 + x);
                tiles[idx] = TILE_ROAD_CROSS;
                if (tiles[idx - 1] == TILE_PLAIN)
                    tiles[idx - 1] = TILE_MTN;
                tiles[idx + w] = TILE_MTN;
            }
        }
    }

    tiles[2 * w + 2] = TILE_ARMY_HQ(1);
    tiles[2 * w + 3] = TILE_ARMY_BASE(1);
    tiles[1 * w + 2] = TILE_ARMY_CITY(1);
    tiles[2 * w + 1] = TILE_AIRP_NEUTRAL;
    tiles[3 * w + 1] = TILE_PORT_NEUTRAL;
    tiles[1 * w + 1] = TILE_COM_NEUTRAL;
    tiles[11 * w + 1] = TILE_SILO;

    tiles[10 * w + 14] = TILE_ARMY_HQ(2);
    tiles[10 * w + 13] = TILE_ARMY_BASE(2);
    tiles[11 * w + 14] = TILE_ARMY_CITY(2);

    if (numPlayers >= 3) {
        tiles[2 * w + 14] = TILE_ARMY_HQ(3);
        tiles[2 * w + 13] = TILE_ARMY_BASE(3);
    }
    if (numPlayers >= 4) {
        tiles[10 * w + 2] = TILE_ARMY_HQ(4);
        tiles[10 * w + 3] = TILE_ARMY_BASE(4);
    }
}

static int AbsInt(int v)
{
    return v < 0 ? -v : v;
}

static __attribute__((noinline)) int IsWaterTerrain(u8 t)
{
    return (t == T_SEA || t == T_SHOAL || t == T_REEF || t == T_RIVER || t == T_BRIDGE);
}

static __attribute__((noinline)) int NearAnyHq(int x, int y, const int *hx, const int *hy, int n, int dist)
{
    int i;
    for (i = 0; i < n; i++) {
        if (AbsInt(x - hx[i]) + AbsInt(y - hy[i]) <= dist)
            return 1;
    }
    return 0;
}

static __attribute__((noinline)) int IsBuildable(u8 *tgrid, u16 *tiles, int idx)
{
    return (tiles[idx] == 0 && (tgrid[idx] == T_PLAIN || tgrid[idx] == T_WOOD));
}

/* Road step: bridges Rivers, opens Seams in Pipes, leaves Sea/Shoal/Reef intact
 * so naval basins are never walled off by multi-tile bridges!
 */
static __attribute__((noinline)) void ApplyRoadStep(u8 *tgrid, u16 *tiles, int w, int h, int x, int y)
{
    if (x > 0 && x < w - 1 && y > 0 && y < h - 1 && tiles[y * w + x] == 0) {
        u8 cur = tgrid[y * w + x];
        if (cur == T_RIVER || cur == T_BRIDGE)
            tgrid[y * w + x] = T_BRIDGE;
        else if (cur == T_PIPE || cur == T_SEAM)
            tgrid[y * w + x] = T_SEAM;
        else if (cur != T_SEA && cur != T_SHOAL && cur != T_REEF)
            tgrid[y * w + x] = T_ROAD;
    }
}

static __attribute__((noinline)) void CarveRoadHV(u8 *tgrid, u16 *tiles, int w, int h,
                                                   int x0, int y0, int x1, int y1, int hvFirst)
{
    int x = x0, y = y0;
    while (x != x1 || y != y1) {
        ApplyRoadStep(tgrid, tiles, w, h, x, y);
        if (hvFirst) {
            if (x < x1) x++; else if (x > x1) x--; else if (y < y1) y++; else y--;
        } else {
            if (y < y1) y++; else if (y > y1) y--; else if (x < x1) x++; else x--;
        }
    }
    ApplyRoadStep(tgrid, tiles, w, h, x, y);
}

/* Carve a water basin (T_SEA core with T_SHOAL shallow beach rim on orthogonal edges) */
static __attribute__((noinline)) void CarveWaterBasin(
    u8 *tgrid, int w, int h, int cx, int cy, int rx, int ry,
    const int *hx, const int *hy, int numPlayers)
{
    int x, y;
    for (y = cy - ry; y <= cy + ry; y++) {
        for (x = cx - rx; x <= cx + rx; x++) {
            if (x >= 0 && x < w && y >= 0 && y < h && !NearAnyHq(x, y, hx, hy, numPlayers, 4)) {
                int dx = AbsInt(x - cx), dy = AbsInt(y - cy);
                if (dx == rx && dy == ry)
                    continue;
                if ((dx == rx || dy == ry) && x > 0 && x < w - 1 && y > 0 && y < h - 1)
                    tgrid[y * w + x] = T_SHOAL;
                else
                    tgrid[y * w + x] = T_SEA;
            }
        }
    }
}

/* Place a coastal Port on land and ensure it opens directly into a deep T_SEA tile */
static __attribute__((noinline)) int TryPlaceCoastalPort(
    u8 *tgrid, u16 *tiles, int w, int h,
    int x0, int y0, int radius, u16 portTile,
    const int *hx, const int *hy, int numPlayers)
{
    static const s8 sDirs[4][2] = { {0, -1}, {0, 1}, {-1, 0}, {1, 0} };
    int x, y, d;
    for (y = y0 - radius; y <= y0 + radius; y++) {
        for (x = x0 - radius; x <= x0 + radius; x++) {
            if (x >= 1 && x < w - 1 && y >= 1 && y < h - 1) {
                int idx = y * w + x;
                if (tiles[idx] == 0 && (tgrid[idx] == T_PLAIN || tgrid[idx] == T_WOOD)
                    && !NearAnyHq(x, y, hx, hy, numPlayers, 1)) {
                    for (d = 0; d < 4; d++) {
                        int nx = x + sDirs[d][0], ny = y + sDirs[d][1];
                        u8 nt = tgrid[ny * w + nx];
                        if (nt == T_SEA || nt == T_SHOAL) {
                            tgrid[ny * w + nx] = T_SEA; /* Guarantee deep-water harbor mouth */
                            tgrid[idx] = T_PLAIN;
                            tiles[idx] = portTile;
                            return 1;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

/* Place an army's HQ, starting Bases, Airports, Cities, and optional Port */
static __attribute__((noinline)) void PlaceHomeCluster(
    u8 *tgrid, u16 *tiles, int w, int h,
    int hx, int hy, int dirX, int dirY,
    int army, int numBases, int numAirports, int numCities, int wantPort,
    const int *allHx, const int *allHy, int numPlayers, u32 *rng)
{
    int i, placed, pass;
    static const s8 sOffsets[12][2] = {
        /* 0..6: Bases */
        { 2, 0 }, { 0, 2 }, { 1, 1 }, { 2, 1 }, {-1, 1 }, { 1,-1 }, { 2, 2 },
        /* 7..11: Airports */
        { 3, 1 }, { 1, 3 }, { 3, 0 }, { 0, 3 }, {-1, 2 }
    };

    tiles[hy * w + hx] = TILE_ARMY_HQ(army);
    tgrid[hy * w + hx] = T_PLAIN;

    if (hy > 0 && tiles[(hy - 1) * w + hx] == 0 && tgrid[(hy - 1) * w + hx] != T_ROAD)
        tgrid[(hy - 1) * w + hx] = T_PLAIN;
    if (hx + dirX >= 1 && hx + dirX < w - 1 && tiles[hy * w + (hx + dirX)] == 0)
        tgrid[hy * w + (hx + dirX)] = T_ROAD;
    if (hy + dirY >= 1 && hy + dirY < h - 1 && tiles[(hy + dirY) * w + hx] == 0)
        tgrid[(hy + dirY) * w + hx] = T_PLAIN;

    for (pass = 0; pass < 2; pass++) {
        int start = pass ? 7 : 0;
        int end   = pass ? 12 : 7;
        int limit = pass ? numAirports : numBases;
        u16 prop  = pass ? TILE_ARMY_AIRP(army) : TILE_ARMY_BASE(army);
        placed = 0;
        for (i = start; i < end && placed < limit; i++) {
            int bx = hx + sOffsets[i][0] * dirX;
            int by = hy + sOffsets[i][1] * dirY;
            if (bx >= 1 && bx < w - 1 && by >= 1 && by < h - 1 && !(bx == hx && by == hy - 1)) {
                int idx = by * w + bx;
                if (tiles[idx] == 0 && (tgrid[idx] == T_PLAIN || tgrid[idx] == T_WOOD || tgrid[idx] == T_MTN)) {
                    tgrid[idx] = T_PLAIN;
                    tiles[idx] = prop;
                    placed++;
                }
            }
        }
    }

    if (wantPort)
        TryPlaceCoastalPort(tgrid, tiles, w, h, hx, hy, 7, TILE_ARMY_PORT(army), allHx, allHy, numPlayers);

    placed = 0;
    for (i = 0; i < 40 && placed < numCities; i++) {
        int cx = hx + (SkirmishRandRange(rng, 6) - 1) * dirX;
        int cy = hy + (SkirmishRandRange(rng, 5) - 1) * dirY;
        if (cx >= 1 && cx < w - 1 && cy >= 1 && cy < h - 1
            && !(cx == hx && cy == hy - 1) && !(cx == hx && cy == hy + dirY)) {
            int idx = cy * w + cx;
            if (IsBuildable(tgrid, tiles, idx)) {
                tgrid[idx] = T_PLAIN;
                tiles[idx] = TILE_ARMY_CITY(army);
                placed++;
            }
        }
    }
}

static __attribute__((noinline)) void ScatterNeutralProp(
    u8 *tgrid, u16 *tiles, int w, int h, int maxX,
    int target, int tries, int margin, int minHqDist, u16 propTile,
    const int *hx, const int *hy, int numPlayers, u32 *rng)
{
    int i, placed = 0;
    int spanX = maxX - margin + 1;
    int spanY = h - (margin << 1);
    if (spanX < 1) spanX = 1;
    if (spanY < 1) spanY = 1;
    for (i = 0; i < tries && placed < target; i++) {
        int nx = margin + SkirmishRandRange(rng, spanX);
        int ny = margin + SkirmishRandRange(rng, spanY);
        int idx = ny * w + nx;
        if (IsBuildable(tgrid, tiles, idx) && !NearAnyHq(nx, ny, hx, hy, numPlayers, minHqDist)) {
            tiles[idx] = propTile;
            tgrid[idx] = T_PLAIN;
            placed++;
        }
    }
}

/* Map a left-side tile ID to its right-side mirror counterpart (Army 1<->2, 3<->4) */
static u16 MirrorFacilityTile(u16 tid, int numPlayers)
{
    if (tid >= 453 && tid <= 472) {
        int d = tid - 453;
        int army = 1;
        while (d >= 5) {
            d -= 5;
            army++;
        }
        if (army == 1) army = 2;
        else if (army == 2) army = 1;
        else if (numPlayers >= 3) {
            if (army == 3) army = 4;
            else if (army == 4) army = 3;
        }
        return (u16)(453 + (army - 1) * 5 + d);
    }
    return tid;
}

static void SkirmishGenerateBattleMap(u8 *destBuf, int numPlayers, int isMirror, u32 seed)
{
    static const u8 sBaseW[6] = { 18, 20, 18, 26, 26, 24 };
    static const u8 sBaseH[6] = { 12, 14, 18, 12, 18, 14 };
    u32 rng = seed;
    u16 *tiles = (u16 *)(destBuf + 2);
    u8 *tgrid = destBuf + 1400;
    int sizeClass = SkirmishRandRange(&rng, 6);
    int archetype = SkirmishRandRange(&rng, 8);
    int orient    = isMirror ? 0 : SkirmishRandRange(&rng, 2);
    int w, h, x, y, i;
    int cx, cy, hubX, hubY;
    int hx[4], hy[4], dX[4], dY[4];
    int cornerOrder[4];
    int baseCount = 2 + SkirmishRandRange(&rng, 2);
    int airpCount = SkirmishRandRange(&rng, 2);
    int cityCount = 2 + SkirmishRandRange(&rng, 3);
    int neutralBases = 2 + SkirmishRandRange(&rng, 3) + ((sizeClass == 4) ? 2 : 0);
    int neutralAirps = SkirmishRandRange(&rng, 3);
    int numSilos     = 1 + SkirmishRandRange(&rng, 2);
    int numComTowers = 1 + SkirmishRandRange(&rng, 2);
    int neutralPorts = 0;
    int neutralCities;
    int wantHomePort = 0;

    /* 1. Choose diverse map dimensions & aspect ratios */
    w = sBaseW[sizeClass] + (SkirmishRandRange(&rng, 3) << 1);
    h = sBaseH[sizeClass] + (SkirmishRandRange(&rng, 2) << 1);

    if (numPlayers >= 3 && w < 22) w += 2;
    if (numPlayers >= 4 && w < 24) w += 2;
    if (h < 14 && (archetype >= 1 && archetype <= 6)) h = 14;
    if (w < 22 && (archetype == 1 || archetype == 2 || archetype == 4)) w = 22;

    if (isMirror) {
        w |= 1;
        h |= 1;
        if (w < 21) w = 21;
        if (h < 15) h = 15;
    }
    if (w > MAX_W) w = isMirror ? 29 : MAX_W;
    if (h > MAX_H) h = isMirror ? 19 : MAX_H;

    cx = w >> 1;
    cy = h >> 1;
    destBuf[0] = (u8)w;
    destBuf[1] = (u8)h;

    for (i = 0; i < w * h; i++) {
        tiles[i] = 0;
        tgrid[i] = T_PLAIN;
    }

    if (archetype >= 1 && archetype <= 4) {
        if (airpCount < 1) airpCount = 1;
        wantHomePort = 1;
        neutralPorts = 2;
    } else if (archetype == 0 || archetype == 5) {
        neutralPorts = 1 + SkirmishRandRange(&rng, 2);
    }

    neutralCities = ((w * h) >> 5) + 3 + SkirmishRandRange(&rng, 8);

    /* 2. Assign HQ Spawn Formations (Corners, 3P Triangle/Wedge, or 2P Mid-Edge) */
    for (i = 0; i < 4; i++) cornerOrder[i] = i;
    for (i = 3; i > 0; i--) {
        int j = SkirmishRandRange(&rng, i + 1);
        int tmp = cornerOrder[i]; cornerOrder[i] = cornerOrder[j]; cornerOrder[j] = tmp;
    }
    if (numPlayers <= 2 && (cornerOrder[0] ^ cornerOrder[1]) != 1)
        cornerOrder[1] = cornerOrder[0] ^ 1;

    {
        int oxL = (w <= 22) ? 2 : (2 + SkirmishRandRange(&rng, 3));
        int oxR = isMirror ? oxL : ((w <= 22) ? 2 : (2 + SkirmishRandRange(&rng, 3)));
        int oyT = (h <= 15) ? 2 : (2 + SkirmishRandRange(&rng, 2));
        int oyB = isMirror ? oyT : ((h <= 15) ? 2 : (2 + SkirmishRandRange(&rng, 2)));

        if (isMirror) {
            cornerOrder[0] = 0; cornerOrder[1] = 3; cornerOrder[2] = 2; cornerOrder[3] = 1;
        }
        for (i = 0; i < 4; i++) {
            int c = cornerOrder[i];
            int isR = (c == 1 || c == 3);
            int isT = (c == 1 || c == 2);
            hx[i] = isR ? (w - 1 - oxR) : oxL;
            hy[i] = isT ? oyT : (h - 1 - oyB);
            dX[i] = isR ? -1 : 1;
            dY[i] = isT ? 1 : -1;
        }

        if (!isMirror) {
            /* 3P Triangle Formation (55% chance on 3-player modes like 1v1v1) */
            if (numPlayers == 3 && SkirmishRandRange(&rng, 100) < 55) {
                int apex = SkirmishRandRange(&rng, 3);
                int i1 = (apex == 2) ? 0 : (apex + 1);
                int i2 = (apex == 0) ? 2 : (apex - 1);
                int dir = SkirmishRandRange(&rng, 4);
                int jit = (SkirmishRandRange(&rng, 2) == 0) ? -1 : 1;
                if (dir < 2) {
                    int apRight = (dir == 0);
                    hx[apex] = apRight ? (w - 1 - oxR) : oxL;
                    hy[apex] = cy + jit;
                    dX[apex] = apRight ? -1 : 1;
                    dY[apex] = -jit;
                    hx[i1] = apRight ? oxL : (w - 1 - oxR); hy[i1] = oyT;         dX[i1] = -dX[apex]; dY[i1] =  1;
                    hx[i2] = hx[i1];                        hy[i2] = h - 1 - oyB; dX[i2] = -dX[apex]; dY[i2] = -1;
                } else {
                    int apBot = (dir == 2);
                    hx[apex] = cx + jit * 2;
                    hy[apex] = apBot ? (h - 1 - oyB) : oyT;
                    dX[apex] = -jit;
                    dY[apex] = apBot ? -1 : 1;
                    hx[i1] = oxL;         hy[i1] = apBot ? oyT : (h - 1 - oyB); dX[i1] =  1; dY[i1] = -dY[apex];
                    hx[i2] = w - 1 - oxR; hy[i2] = hy[i1];                      dX[i2] = -1; dY[i2] = -dY[apex];
                }
            } else if (numPlayers == 2 && SkirmishRandRange(&rng, 100) < 35) {
                int off = (SkirmishRandRange(&rng, 2) == 0) ? -2 : 2;
                if (SkirmishRandRange(&rng, 2) == 0) {
                    hx[0] = oxL;         hy[0] = cy + off; dX[0] =  1; dY[0] = (off > 0) ? -1 : 1;
                    hx[1] = w - 1 - oxR; hy[1] = cy - off; dX[1] = -1; dY[1] = -dY[0];
                } else {
                    hx[0] = cx + off; hy[0] = h - 1 - oyB; dX[0] = (off > 0) ? -1 : 1; dY[0] = -1;
                    hx[1] = cx - off; hy[1] = oyT;         dX[1] = -dX[0];             dY[1] =  1;
                }
            }
        }
    }

    hubX = isMirror ? cx : (cx + SkirmishRandRange(&rng, 5) - 2);
    hubY = isMirror ? cy : (cy + SkirmishRandRange(&rng, 3) - 1);

    /* 3. Carve Macro-Archetype Geography (0..7) with Creative Water Topologies */
    if (archetype == 0 || archetype == 2 || archetype == 4 || archetype == 7) {
        for (i = 0; i < 4; i++) {
            int crX = (i & 1) ? (w - 1) : 0;
            int crY = (i & 2) ? (h - 1) : 0;
            if (!NearAnyHq(crX, crY, hx, hy, numPlayers, 7))
                CarveWaterBasin(tgrid, w, h, crX, crY, 3, 3, hx, hy, numPlayers);
        }
    }

    if (archetype == 0) {
        /* Archetype 0: Meandering River & Shoal Estuary Coves */
        int len = orient ? w : h;
        int cPos = orient ? cy : cx;
        int rPos = isMirror ? cPos : (cPos + SkirmishRandRange(&rng, 3) - 1);
        int lastStep = -10;
        for (i = 0; i < len; i++) {
            int rx = orient ? i : rPos;
            int ry = orient ? rPos : i;
            if (!NearAnyHq(rx, ry, hx, hy, numPlayers, 3))
                tgrid[ry * w + rx] = T_RIVER;
            if (!isMirror && i >= 4 && i < len - 4 && i - lastStep >= 3 && SkirmishRandRange(&rng, 100) < 55) {
                int nr = rPos + ((SkirmishRandRange(&rng, 2) == 0) ? -1 : 1);
                int nx = orient ? i : nr;
                int ny = orient ? nr : i;
                if (AbsInt(nr - cPos) <= 2 && !NearAnyHq(nx, ny, hx, hy, numPlayers, 3)) {
                    rPos = nr;
                    tgrid[ny * w + nx] = T_RIVER;
                    lastStep = i;
                }
            }
        }
        {
            int endPos = (SkirmishRandRange(&rng, 2) == 0) ? 1 : (len - 2);
            CarveWaterBasin(tgrid, w, h, orient ? endPos : rPos, orient ? rPos : endPos,
                            2, 2, hx, hy, numPlayers);
        }
    } else if (archetype == 1 || archetype == 2) {
        /* Archetype 1: Central Atoll Lagoon | Archetype 2: Sea Strait & Island Chain */
        int rx = (archetype == 1) ? ((w >= 26) ? 5 : 4) : (orient ? (cx - 2) : 2);
        int ry = (archetype == 1) ? ((h >= 16) ? 4 : 3) : (orient ? 2 : (cy - 2));
        hubX = cx; hubY = cy;
        CarveWaterBasin(tgrid, w, h, cx, cy, rx, ry, hx, hy, numPlayers);
        for (y = cy - 1; y <= cy + 1; y++)
            for (x = cx - 1; x <= cx + 1; x++)
                tgrid[y * w + x] = T_PLAIN;
        tgrid[(orient ? cy : (cy - 2)) * w + (orient ? (cx - 2) : cx)] = T_REEF;
        tgrid[(orient ? cy : (cy + 2)) * w + (orient ? (cx + 2) : cx)] = T_REEF;
    } else if (archetype == 3) {
        /* Archetype 3: Twin Lakes & Central Isthmus (3-Lane Amphibious Corridor) */
        int lx1 = orient ? 5 : cx,       ly1 = orient ? cy : 4;
        int lx2 = orient ? (w - 6) : cx, ly2 = orient ? cy : (h - 5);
        int rx  = orient ? 2 : 3,        ry  = orient ? 2 : 2;
        hubX = cx; hubY = cy;
        CarveWaterBasin(tgrid, w, h, lx1, ly1, rx, ry, hx, hy, numPlayers);
        CarveWaterBasin(tgrid, w, h, lx2, ly2, rx, ry, hx, hy, numPlayers);
        tgrid[ly1 * w + lx1] = T_REEF;
        tgrid[ly2 * w + lx2] = T_REEF;
    } else if (archetype == 4) {
        /* Archetype 4: Coastal Sea Bay, Peninsula & Harbor */
        int topBay = (SkirmishRandRange(&rng, 2) == 0);
        int bayCy = topBay ? 2 : (h - 3);
        int penY  = topBay ? 3 : (h - 4);
        hubX = cx; hubY = cy;
        CarveWaterBasin(tgrid, w, h, cx, bayCy, 5, 2, hx, hy, numPlayers);
        for (y = (topBay ? 1 : (h - 5)); y <= (topBay ? 4 : (h - 2)); y++)
            for (x = cx - 1; x <= cx + 1; x++)
                if (y >= 1 && y < h - 1)
                    tgrid[y * w + x] = T_PLAIN;
        tgrid[bayCy * w + (cx - 3)] = T_REEF;
        tgrid[bayCy * w + (cx + 3)] = T_REEF;
        tiles[penY * w + cx] = TILE_COM_NEUTRAL;
        CarveRoadHV(tgrid, tiles, w, h, cx, cy, cx, penY, 0);
    } else if (archetype == 5) {
        /* Archetype 5: Wetland Marsh & Shoal Bayou (4..6 Shallow Shoal-rimmed Ponds + Wood Groves) */
        int numPonds = 4 + SkirmishRandRange(&rng, 3);
        for (i = 0; i < numPonds; i++) {
            int px = 4 + SkirmishRandRange(&rng, isMirror ? (cx - 3) : (w - 8));
            int py = 3 + SkirmishRandRange(&rng, h - 6);
            CarveWaterBasin(tgrid, w, h, px, py, 2, 2, hx, hy, numPlayers);
        }
    } else {
        /* Archetypes 6 & 7: Volcanic Mountain Crater (6) or Black Hole Pipe Citadel (7) + Flanking Oasis Lakes */
        int isPipe = (archetype == 7);
        int crRx = 3, crRy = 2;
        hubX = cx; hubY = cy;
        for (y = cy - crRy; y <= cy + crRy; y++) {
            for (x = cx - crRx; x <= cx + crRx; x++) {
                int dx = AbsInt(x - cx), dy = AbsInt(y - cy);
                if (dx == crRx || dy == crRy) {
                    if (dx == 0 || dy == 0)
                        tgrid[y * w + x] = isPipe ? T_SEAM : T_ROAD;
                    else if (!NearAnyHq(x, y, hx, hy, numPlayers, 3))
                        tgrid[y * w + x] = isPipe ? T_PIPE : T_MTN;
                } else {
                    tgrid[y * w + x] = T_PLAIN;
                }
            }
        }
        CarveWaterBasin(tgrid, w, h, cx - 6, cy, 2, 2, hx, hy, numPlayers);
        if (!isMirror)
            CarveWaterBasin(tgrid, w, h, cx + 6, cy, 2, 2, hx, hy, numPlayers);
    }

    /* 4. Scatter Organic Multi-Step Terrain Blobs (Mountains & Woods) */
    {
        int maxX = isMirror ? cx : (w - 2);
        int numBlobs = isMirror ? ((w * h) >> 4) : (((w * h) * 3) >> 5);
        int mtnChance = (archetype == 6) ? 55 : ((archetype == 5) ? 25 : 42);
        for (i = 0; i < numBlobs; i++) {
            int bx = 1 + SkirmishRandRange(&rng, maxX);
            int by = 1 + SkirmishRandRange(&rng, h - 2);
            u8 bType = (SkirmishRandRange(&rng, 100) < mtnChance) ? T_MTN : T_WOOD;
            int len = 2 + SkirmishRandRange(&rng, 4);
            int step;
            for (step = 0; step < len; step++) {
                if (bx >= 1 && bx <= maxX && by >= 1 && by < h - 1) {
                    int idx = by * w + bx;
                    if (tgrid[idx] == T_PLAIN && tiles[idx] == 0
                        && !(AbsInt(bx - hubX) <= 2 && AbsInt(by - hubY) <= 2)
                        && !NearAnyHq(bx, by, hx, hy, numPlayers, 3)) {
                        tgrid[idx] = bType;
                    }
                }
                if (SkirmishRandRange(&rng, 2) == 0)
                    bx += (SkirmishRandRange(&rng, 2) == 0) ? -1 : 1;
                else
                    by += (SkirmishRandRange(&rng, 2) == 0) ? -1 : 1;
            }
        }
    }

    /* 5. Carve Road Network through Central Hub & Flanks */
    for (i = 0; i < numPlayers; i++) {
        int next = (i + 1 < numPlayers) ? (i + 1) : 0;
        int x0 = hx[i] + dX[i], y0 = hy[i];
        int x1 = hx[next] + dX[next], y1 = hy[next];
        int hvF = isMirror ? 1 : SkirmishRandRange(&rng, 2);
        CarveRoadHV(tgrid, tiles, w, h, x0, y0, x1, y1, hvF);
        CarveRoadHV(tgrid, tiles, w, h, x0, y0, hubX, hubY, 1 - hvF);
    }

    /* 6. Place HQs & Home Clusters */
    for (i = 0; i < numPlayers; i++) {
        if (isMirror && (i == 1 || i == 3))
            continue;
        PlaceHomeCluster(tgrid, tiles, w, h, hx[i], hy[i], dX[i], dY[i],
                         i + 1, baseCount, airpCount, cityCount, wantHomePort, hx, hy, numPlayers, &rng);
    }

    /* 7. Place Central Set-Piece Objectives (Silo, Com Tower, Neutral Base, Naval Ports) */
    if (!NearAnyHq(hubX, hubY, hx, hy, numPlayers, 4)) {
        if (IsBuildable(tgrid, tiles, hubY * w + hubX)) {
            tiles[hubY * w + hubX] = TILE_SILO;
            tgrid[hubY * w + hubX] = T_PLAIN;
        }
        if (hubY > 1 && IsBuildable(tgrid, tiles, (hubY - 1) * w + hubX)) {
            tiles[(hubY - 1) * w + hubX] = TILE_BASE_NEUTRAL;
            tgrid[(hubY - 1) * w + hubX] = T_PLAIN;
        }
        if (hubX > 1 && IsBuildable(tgrid, tiles, hubY * w + (hubX - 1))) {
            tiles[hubY * w + (hubX - 1)] = TILE_COM_NEUTRAL;
            tgrid[hubY * w + (hubX - 1)] = T_PLAIN;
        }
    }

    for (i = 0; i < neutralPorts; i++) {
        int px = 2 + SkirmishRandRange(&rng, isMirror ? (cx - 2) : (w - 4));
        int py = 2 + SkirmishRandRange(&rng, h - 4);
        TryPlaceCoastalPort(tgrid, tiles, w, h, px, py, 8, TILE_PORT_NEUTRAL, hx, hy, numPlayers);
    }

    /* 8. Scatter Neutral Bases, Airports, Com Towers, Silos, and Cities */
    {
        int maxX = isMirror ? (cx - 1) : (w - 2);
        ScatterNeutralProp(tgrid, tiles, w, h, maxX, isMirror ? ((neutralBases + 1) >> 1) : neutralBases,
                           60, 2, 4, TILE_BASE_NEUTRAL, hx, hy, numPlayers, &rng);
        ScatterNeutralProp(tgrid, tiles, w, h, maxX, isMirror ? ((neutralAirps + 1) >> 1) : neutralAirps,
                           50, 2, 4, TILE_AIRP_NEUTRAL, hx, hy, numPlayers, &rng);
        ScatterNeutralProp(tgrid, tiles, w, h, maxX, numComTowers,
                           50, 2, 4, TILE_COM_NEUTRAL, hx, hy, numPlayers, &rng);
        ScatterNeutralProp(tgrid, tiles, w, h, maxX, numSilos,
                           40, 2, 4, TILE_SILO, hx, hy, numPlayers, &rng);
        ScatterNeutralProp(tgrid, tiles, w, h, maxX, isMirror ? ((neutralCities + 1) >> 1) : neutralCities,
                           160, 1, 3, TILE_CITY_NEUTRAL, hx, hy, numPlayers, &rng);
    }

    /* 9. If Mirror mode, reflect Left half (0 .. cx - 1) onto Right half (w - 1 - x) */
    if (isMirror) {
        for (y = 0; y < h; y++) {
            for (x = 0; x < cx; x++) {
                int src = y * w + x;
                int dst = y * w + (w - 1 - x);
                tgrid[dst] = tgrid[src];
                tiles[dst] = MirrorFacilityTile(tiles[src], numPlayers);
            }
        }
    }

    /* 10. Sanitize Properties, Rivers, Bridges, and Shoals */
    for (i = 0; i < w * h; i++) {
        if (tiles[i] != 0)
            tgrid[i] = T_PLAIN;
    }

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int idx = y * w + x;
            u8 t = tgrid[idx];
            u8 tN = (y > 0)     ? tgrid[idx - w] : t;
            u8 tS = (y < h - 1) ? tgrid[idx + w] : t;
            u8 tW = (x > 0)     ? tgrid[idx - 1] : t;
            u8 tE = (x < w - 1) ? tgrid[idx + 1] : t;
            if (tiles[idx] != 0 && y > 1 && tN == T_MTN && tS >= T_MTN) {
                tgrid[idx - w] = T_PLAIN;
            } else if (t == T_RIVER) {
                if (tN >= T_SEA || tS >= T_SEA || tW >= T_SEA || tE >= T_SEA)
                    tgrid[idx] = T_PLAIN;
            } else if (t == T_SHOAL) {
                int hasSea  = (tN == T_SEA || tS == T_SEA || tW == T_SEA || tE == T_SEA);
                int hasLand = (!IsWaterTerrain(tN) || !IsWaterTerrain(tS) || !IsWaterTerrain(tW) || !IsWaterTerrain(tE));
                if (!hasLand)
                    tgrid[idx] = T_SEA;
                else if (!hasSea)
                    tgrid[idx] = T_PLAIN;
            }
        }
    }

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int idx = y * w + x;
            if (tgrid[idx] == T_BRIDGE) {
                u8 tN = (y > 0)     ? tgrid[idx - w] : T_RIVER;
                u8 tS = (y < h - 1) ? tgrid[idx + w] : T_RIVER;
                u8 tW = (x > 0)     ? tgrid[idx - 1] : T_RIVER;
                u8 tE = (x < w - 1) ? tgrid[idx + 1] : T_RIVER;
                int vNS = (tN == T_RIVER && tS == T_RIVER && !IsWaterTerrain(tW) && !IsWaterTerrain(tE));
                int vWE = (tW == T_RIVER && tE == T_RIVER && !IsWaterTerrain(tN) && !IsWaterTerrain(tS));
                if (!vNS && !vWE)
                    tgrid[idx] = T_ROAD;
            }
        }
    }

    /* 11. Resolve Final AWDS Visual Tile IDs with Full Auto-Tiling & Shadows */
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int idx = y * w + x;
            u8 t = tgrid[idx];
            u8 tN, tS, tW, tE;
            int wMask, lMask;

            if (tiles[idx] != 0)
                continue;

            tN = (y > 0)     ? tgrid[idx - w] : t;
            tS = (y < h - 1) ? tgrid[idx + w] : t;
            tW = (x > 0)     ? tgrid[idx - 1] : t;
            tE = (x < w - 1) ? tgrid[idx + 1] : t;
            wMask = (IsWaterTerrain(tN) ? 1 : 0) | (IsWaterTerrain(tS) ? 2 : 0) |
                    (IsWaterTerrain(tW) ? 4 : 0) | (IsWaterTerrain(tE) ? 8 : 0);
            lMask = (~wMask) & 15;

            if (t == T_WOOD) {
                tiles[idx] = TILE_WOOD;
            } else if (t == T_MTN) {
                tiles[idx] = TILE_MTN;
            } else if (t == T_REEF && wMask == 15 && x > 0 && x < w - 1 && y > 0 && y < h - 1) {
                tiles[idx] = TILE_REEF;
            } else if (t == T_BRIDGE) {
                int nsWater = ((wMask & 1) && tN != T_BRIDGE) || ((wMask & 2) && tS != T_BRIDGE);
                tiles[idx] = nsWater ? TILE_BRIDGE_EW : TILE_BRIDGE_NS;
            } else if (t == T_SEAM) {
                int nsPipe = (y > 0 && tN >= T_PIPE) || (y < h - 1 && tS >= T_PIPE);
                tiles[idx] = nsPipe ? TILE_SEAM_NS : TILE_SEAM_EW;
            } else if (t == T_PIPE) {
                int m = ((y > 0 && tN >= T_PIPE) ? 1 : 0) |
                        ((y < h - 1 && tS >= T_PIPE) ? 2 : 0) |
                        ((x > 0 && tW >= T_PIPE) ? 4 : 0) |
                        ((x < w - 1 && tE >= T_PIPE) ? 8 : 0);
                tiles[idx] = sPipeTileTable[m];
            } else if (t == T_ROAD) {
                int m = ((y > 0 && (tN == T_ROAD || tN == T_BRIDGE || tN == T_SEAM)) ? 1 : 0) |
                        ((y < h - 1 && (tS == T_ROAD || tS == T_BRIDGE || tS == T_SEAM)) ? 2 : 0) |
                        ((x > 0 && (tW == T_ROAD || tW == T_BRIDGE || tW == T_SEAM)) ? 4 : 0) |
                        ((x < w - 1 && (tE == T_ROAD || tE == T_BRIDGE || tE == T_SEAM)) ? 8 : 0);
                tiles[idx] = sRoadTileTable[m];
            } else if (t == T_RIVER) {
                tiles[idx] = sRiverTileTable[wMask];
            } else if (t == T_SHOAL) {
                u16 st = sShoalTileTable[lMask];
                if (lMask == 4 || lMask == 8) {
                    int nShoal = (y > 0     && tN == T_SHOAL);
                    int sShoal = (y < h - 1 && tS == T_SHOAL);
                    if (!nShoal && sShoal)
                        st = (lMask == 4) ? 176 : 175;
                    else if (nShoal && !sShoal)
                        st = (lMask == 4) ? 208 : 207;
                    else if (!nShoal && !sShoal)
                        st = (lMask == 4) ? 147 : 146;
                }
                tiles[idx] = st;
            } else if (t == T_SEA || t == T_REEF) {
                if (lMask != 0) {
                    tiles[idx] = sSeaTileTable[lMask];
                } else {
                    u16 st = TILE_SEA;
                    if (x < w - 1 && y < h - 1 && !IsWaterTerrain(tgrid[idx + w + 1]))
                        st = 9;
                    else if (x > 0 && y < h - 1 && !IsWaterTerrain(tgrid[idx + w - 1]))
                        st = 11;
                    else if (x < w - 1 && y > 0 && !IsWaterTerrain(tgrid[idx - w + 1]))
                        st = 72;
                    else if (x > 0 && y > 0 && !IsWaterTerrain(tgrid[idx - w - 1]))
                        st = 76;
                    tiles[idx] = st;
                }
            } else {
                tiles[idx] = TILE_PLAIN;
            }
        }
    }

    /* 12. Apply West-of-Tall-Tile Shadows to Plains, Woods, and Roads */
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
    const struct SkirmishModeConfig *cfg = GetSkirmishModeConfig(mapId);

    if (callerLr >= 0x0202CA00 && callerLr < 0x0202D700) {
        if (!cfg)
            return 0;
        SkirmishGenerateQuestionPreview(destBuf, cfg->numArmies);
        return 1;
    }

    if (!cfg)
        return 0;

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
                int i;
                for (i = 0; i < 4; i++)
                    seed |= ((u32)mapCfg[0x14 + i]) << (i * 8);
            }
            if (seed == 0)
                seed = gLastBattleSeed;
            else
                gLastBattleSeed = seed;
        }

        SkirmishGenerateBattleMap(destBuf, cfg->numArmies, cfg->mirror, seed);
    }

    return 1;
}
