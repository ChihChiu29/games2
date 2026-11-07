#include "global.h"
#include "hardware.h"

/*
 * Advance Wars 2: Procedural Skirmish Map Generator
 *
 * Replaces the Versus Mode map list with 4 procedural random map difficulties:
 *   - RANDOM EASY   (mapId =  1): 1v1 (Player OS vs 1 AI BM), 20x14
 *   - RANDOM NORMAL (mapId = 59): 1v1v1 FFA (Player OS vs 2 AIs BM, GE), 24x16
 *   - RANDOM HARD   (mapId = 60): 1v2 Team AI (Player Team A vs 2 Allied AIs Team B), 26x18
 *   - RANDOM INSANE (mapId = 83): 1v3 Team AI (Player Team A vs 3 Allied AIs Team B), 30x20
 */

#define SKIRMISH_MAP_EASY   1
#define SKIRMISH_MAP_NORMAL 59
#define SKIRMISH_MAP_HARD   60
#define SKIRMISH_MAP_INSANE 83

#define SKIRMISH_MAGIC 0x534B524Du /* 'SKRM' */

struct SkirmishState
{
    u32 magic;
    u32 seed;
    u32 activeSeed;
    u8 cachedMapId;
    u8 mapLocked;
    u8 needsReroll;
    u8 pad;
};

#define SKIRMISH_STATE ((volatile struct SkirmishState *)0x0203FF00)

enum
{
    KIND_PLAIN = 0,
    KIND_WOOD = 1,
    KIND_MOUNTAIN = 2,
    KIND_RIVER = 3,
    KIND_ROAD = 4,
    KIND_BRIDGE = 5,
    KIND_HQ = 6,
    KIND_BASE = 7,
    KIND_CITY = 8,
    KIND_AIRPORT = 9,
    KIND_SILO = 10,
};

extern u8 gUnknown_03003FC0[];
extern u8 gUnknown_03003FF8[];
extern u8 gUnknown_03004002[];
extern u8 * const gUnknown_0849ECDC;
extern u8 gUnknown_02027F78[];
extern u8 gUnknown_02027F74[];
extern u8 gUnknown_02028030[];
extern const u8 gUnknown_085C77A0[];
extern const char * const gUnknown_08610A38[];
extern s32 gUnknown_03004008;

u8 Skirmish_PopulateMapList(u8 category);
u8 Skirmish_IsMapUnlocked(u8 mapId);
const char * Skirmish_GetMapName(u16 mapId);
void Skirmish_OnSelectMap(u8 mapId);
const void * Skirmish_GetOrGenerateMapLz(u16 mapId, u32 variant, u8 * destBuf);

static const u16 sRoadTileTable[16] = {
    97, 97, 64, 66, 97, 97, 65, 224,
    64, 98, 64, 193, 96, 192, 225, 256,
};

static const u16 sRiverTileTable[16] = {
    24, 25, 24, 89, 25, 25, 88, 58,
    24, 123, 24, 93, 120, 26, 92, 27,
};

static const char sNameEasy[]   = "RANDOM EASY";
static const char sNameNormal[] = "RANDOM NORMAL";
static const char sNameHard[]   = "RANDOM HARD";
static const char sNameInsane[] = "RANDOM INSANE";

static u32 Skirmish_XorShift32(u32 * state)
{
    u32 x = *state;
    if (x == 0)
        x = 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* Uniform 0 .. max-1 without division/modulo libgcc calls */
static u32 Skirmish_RandRange(u32 * state, u32 max)
{
    u32 r = Skirmish_XorShift32(state);
    return ((r & 0xFFFFu) * max) >> 16;
}

static int Skirmish_IsSkirmishMode(void)
{
    return (gUnknown_03003FC0[1] == 3 && gUnknown_03003FC0[0x32] == 0);
}

static int Skirmish_IsSkirmishMapId(u16 mapId)
{
    return (mapId == SKIRMISH_MAP_EASY
         || mapId == SKIRMISH_MAP_NORMAL
         || mapId == SKIRMISH_MAP_HARD
         || mapId == SKIRMISH_MAP_INSANE);
}

static void Skirmish_EnsureStateInit(void)
{
    if (SKIRMISH_STATE->magic != SKIRMISH_MAGIC)
    {
        u32 savedSeed = *(u32 *)(gUnknown_03003FC0 + 0x10);
        u32 hw = savedSeed;
        if (hw == 0)
        {
            hw = (*(volatile u16 *)0x04000006)
               ^ ((u32)(*(volatile u16 *)0x04000100) << 8)
               ^ ((u32)gUnknown_03004008 * 0x85EBCA6Bu)
               ^ 0xC2B2AE35u;
            if (hw == 0)
                hw = 0x6C078965u;
        }
        SKIRMISH_STATE->magic = SKIRMISH_MAGIC;
        SKIRMISH_STATE->seed = hw;
        SKIRMISH_STATE->activeSeed = hw;
        SKIRMISH_STATE->cachedMapId = (savedSeed != 0) ? gUnknown_03003FC0[2] : 0xFF;
        SKIRMISH_STATE->mapLocked = (savedSeed != 0) ? 1 : 0;
        SKIRMISH_STATE->needsReroll = (savedSeed != 0) ? 0 : 1;
    }
}

static u32 Skirmish_MixEntropy(u32 salt)
{
    u32 s = SKIRMISH_STATE->seed;
    s ^= (*(volatile u16 *)0x04000006);
    s ^= ((u32)(*(volatile u16 *)0x04000100) << 11);
    s ^= ((u32)gUnknown_03004008 * 0x27D4EB2Du);
    s ^= (salt * 0x165667B1u) + 0x9E3779B9u;
    Skirmish_XorShift32(&s);
    Skirmish_XorShift32(&s);
    SKIRMISH_STATE->seed = s;
    return s;
}

static void Skirmish_AdvanceHardwareSeed(u32 salt)
{
    SKIRMISH_STATE->activeSeed = Skirmish_MixEntropy(salt);
}

u8 Skirmish_PopulateMapList(u8 category)
{
    *(u32 *)(gUnknown_03003FC0 + 0x10) = 0;
    Skirmish_EnsureStateInit();
    Skirmish_MixEntropy(category);

    SKIRMISH_STATE->mapLocked = 0;
    SKIRMISH_STATE->needsReroll = 1;

    gUnknown_02027F78[0] = SKIRMISH_MAP_EASY;
    gUnknown_02027F78[1] = SKIRMISH_MAP_NORMAL;
    gUnknown_02027F78[2] = SKIRMISH_MAP_HARD;
    gUnknown_02027F78[3] = SKIRMISH_MAP_INSANE;

    gUnknown_02027F74[0x36] = 0;
    gUnknown_02027F74[0x37] = 3;

    return 1;
}

u8 Skirmish_IsMapUnlocked(u8 mapId)
{
    if (Skirmish_IsSkirmishMode() && Skirmish_IsSkirmishMapId(mapId))
        return 1;

    return gUnknown_02028030[0x12 + (mapId >> 3)] & (1 << (mapId & 7));
}

const char * Skirmish_GetMapName(u16 mapId)
{
    u16 nameIdx;

    if (Skirmish_IsSkirmishMode())
    {
        if (mapId == SKIRMISH_MAP_EASY)
            return sNameEasy;
        if (mapId == SKIRMISH_MAP_NORMAL)
            return sNameNormal;
        if (mapId == SKIRMISH_MAP_HARD)
            return sNameHard;
        if (mapId == SKIRMISH_MAP_INSANE)
            return sNameInsane;
    }

    nameIdx = *(const u16 *)(gUnknown_085C77A0 + (mapId * 0x5C) + 0x14);
    return gUnknown_08610A38[nameIdx];
}

void Skirmish_OnSelectMap(u8 mapId)
{
    gUnknown_03003FC0[2] = mapId;

    if (!Skirmish_IsSkirmishMode() || !Skirmish_IsSkirmishMapId(mapId))
        return;

    Skirmish_EnsureStateInit();
    if (!SKIRMISH_STATE->mapLocked
        && (SKIRMISH_STATE->needsReroll || SKIRMISH_STATE->cachedMapId != mapId))
    {
        Skirmish_AdvanceHardwareSeed(mapId);
        SKIRMISH_STATE->cachedMapId = mapId;
        SKIRMISH_STATE->needsReroll = 0;
    }
    SKIRMISH_STATE->mapLocked = 1;
    *(u32 *)(gUnknown_03003FC0 + 0x10) = SKIRMISH_STATE->activeSeed;

    /* Force 1 Human Player (OS) vs AI opponents */
    gUnknown_0849ECDC[0] = 1;
    gUnknown_03003FF8[1] = 1; /* Army 1 (OS): Human */
    gUnknown_03003FF8[2] = 0; /* Army 2 (BM): AI */
    gUnknown_03003FF8[3] = 0; /* Army 3 (GE): AI */
    gUnknown_03003FF8[4] = 0; /* Army 4 (YC): AI */

    if (mapId == SKIRMISH_MAP_HARD || mapId == SKIRMISH_MAP_INSANE)
    {
        /* Team Battle: Player (Team A = 0) vs Allied AIs (Team B = 1) */
        gUnknown_03003FC0[0x2D] = 0;
        gUnknown_03003FC0[0x2F] = 0;
        gUnknown_03004002[1] = 0;
        gUnknown_03004002[2] = 1;
        gUnknown_03004002[3] = 1;
        gUnknown_03004002[4] = 1;
    }
    else
    {
        /* Free-For-All (1v1 on Easy, 1v1v1 on Normal) */
        gUnknown_03003FC0[0x2D] = 1;
        gUnknown_03003FC0[0x2F] = 0;
        gUnknown_03004002[1] = 0;
        gUnknown_03004002[2] = 1;
        gUnknown_03004002[3] = 2;
        gUnknown_03004002[4] = 3;
    }
}

static int Skirmish_IsInCornerZone(int x, int y, int w, int h, int margin)
{
    if (x < margin && y < margin)
        return 1;
    if (x >= w - margin && y < margin)
        return 1;
    if (x < margin && y >= h - margin)
        return 1;
    if (x >= w - margin && y >= h - margin)
        return 1;
    return 0;
}

static void Skirmish_CarveRoad(
    u8 * kind, int w, int h, int x0, int y0, int x1, int y1, int hvFirst)
{
    int x = x0;
    int y = y0;
    (void)h;

    while (x != x1 || y != y1)
    {
        int idx = y * w + x;
        if (kind[idx] == KIND_RIVER)
            kind[idx] = KIND_BRIDGE;
        else if (kind[idx] != KIND_BRIDGE && kind[idx] < KIND_HQ)
            kind[idx] = KIND_ROAD;

        if (hvFirst)
        {
            if (x < x1)
                x++;
            else if (x > x1)
                x--;
            else if (y < y1)
                y++;
            else if (y > y1)
                y--;
        }
        else
        {
            if (y < y1)
                y++;
            else if (y > y1)
                y--;
            else if (x < x1)
                x++;
            else if (x > x1)
                x--;
        }
    }

    {
        int idx = y * w + x;
        if (kind[idx] == KIND_RIVER)
            kind[idx] = KIND_BRIDGE;
        else if (kind[idx] != KIND_BRIDGE && kind[idx] < KIND_HQ)
            kind[idx] = KIND_ROAD;
    }
}

static void Skirmish_PlaceHomeCluster(
    u8 * kind, u8 * owner, int w, int h,
    int hx, int hy, int dirX, int dirY,
    u8 army, int numBases, int numAirports, int numCities, u32 * rng)
{
    int i;
    int placed;

    /* Place HQ */
    kind[hy * w + hx] = KIND_HQ;
    owner[hy * w + hx] = army;

    /* Keep tile directly North of HQ flat/open so HQ top-cap looks clean */
    if (hy > 0)
    {
        int nIdx = (hy - 1) * w + hx;
        if (kind[nIdx] == KIND_WOOD || kind[nIdx] == KIND_MOUNTAIN)
        {
            kind[nIdx] = KIND_PLAIN;
            owner[nIdx] = 0;
        }
    }

    /* Ensure immediate cardinal neighbors of HQ are passable */
    if (hx + dirX >= 1 && hx + dirX < w - 1)
    {
        int idx = hy * w + (hx + dirX);
        if (kind[idx] == KIND_RIVER)
            kind[idx] = KIND_BRIDGE;
        else if (kind[idx] != KIND_BRIDGE && kind[idx] < KIND_HQ)
            kind[idx] = KIND_ROAD;
    }
    if (hy + dirY >= 1 && hy + dirY < h - 1)
    {
        int idx = (hy + dirY) * w + hx;
        if (kind[idx] == KIND_RIVER)
            kind[idx] = KIND_BRIDGE;
        else if (kind[idx] != KIND_BRIDGE && kind[idx] < KIND_HQ)
            kind[idx] = KIND_ROAD;
    }

    /* Place starting Bases (Factories) close to HQ */
    {
        int bx[4];
        int by[4];
        bx[0] = hx + dirX * 2;
        by[0] = hy;
        bx[1] = hx;
        by[1] = hy + dirY * 2;
        bx[2] = hx + dirX;
        by[2] = hy + dirY;
        bx[3] = hx - dirX;
        by[3] = hy + dirY;

        placed = 0;
        for (i = 0; i < 4 && placed < numBases; i++)
        {
            int x = bx[i];
            int y = by[i];
            if (x >= 1 && x < w - 1 && y >= 1 && y < h - 1)
            {
                int idx = y * w + x;
                if (kind[idx] != KIND_RIVER && kind[idx] != KIND_BRIDGE
                    && kind[idx] < KIND_HQ && !(x == hx && y == hy - 1))
                {
                    kind[idx] = KIND_BASE;
                    owner[idx] = army;
                    placed++;
                }
            }
        }
    }

    /* Place starting Airport if requested */
    if (numAirports > 0)
    {
        int ax = hx + dirX * 3;
        int ay = hy + dirY;
        if (ax >= 1 && ax < w - 1 && ay >= 1 && ay < h - 1)
        {
            int idx = ay * w + ax;
            if (kind[idx] != KIND_RIVER && kind[idx] != KIND_BRIDGE && kind[idx] < KIND_HQ)
            {
                kind[idx] = KIND_AIRPORT;
                owner[idx] = army;
            }
        }
    }

    /* Place starting owned Cities in home zone */
    placed = 0;
    for (i = 0; i < 40 && placed < numCities; i++)
    {
        int dx = (int)Skirmish_RandRange(rng, 6) - 1;
        int dy = (int)Skirmish_RandRange(rng, 5) - 1;
        int x = hx + dx * dirX;
        int y = hy + dy * dirY;

        if (x >= 1 && x < w - 1 && y >= 1 && y < h - 1)
        {
            int idx = y * w + x;
            if ((kind[idx] == KIND_PLAIN || kind[idx] == KIND_WOOD || kind[idx] == KIND_MOUNTAIN)
                && !(x == hx && y == hy - 1))
            {
                kind[idx] = KIND_CITY;
                owner[idx] = army;
                placed++;
            }
        }
    }
}

static int Skirmish_IsTallKind(u8 k)
{
    return (k == KIND_WOOD || k == KIND_MOUNTAIN || k >= KIND_HQ);
}

static int Skirmish_IsRoadLink(u8 k)
{
    return (k == KIND_ROAD || k == KIND_BRIDGE || k >= KIND_HQ);
}

static void Skirmish_GenerateMap(u16 mapId, u32 seed, u8 * destBuf)
{
    u32 rng = seed;
    int w;
    int h;
    int numArmies;
    int playerBases;
    int aiBases;
    int playerAirports;
    int aiAirports;
    int playerCities;
    int aiCities;
    int neutralCityPairs;
    u8 * kind = destBuf + 1204;
    u8 * owner = destBuf + 1804;
    u16 * outTiles = (u16 *)(destBuf + 2);
    int x;
    int y;
    int i;
    int hx[4];
    int hy[4];
    int dirX[4];
    int dirY[4];

    if (mapId == SKIRMISH_MAP_EASY)
    {
        w = 20;
        h = 14;
        numArmies = 2;
        playerBases = 3;
        aiBases = 2;
        playerAirports = 1;
        aiAirports = 0;
        playerCities = 4;
        aiCities = 2;
        neutralCityPairs = 7;
    }
    else if (mapId == SKIRMISH_MAP_NORMAL)
    {
        w = 24;
        h = 16;
        numArmies = 3;
        playerBases = 2;
        aiBases = 2;
        playerAirports = 1;
        aiAirports = 1;
        playerCities = 3;
        aiCities = 3;
        neutralCityPairs = 9;
    }
    else if (mapId == SKIRMISH_MAP_HARD)
    {
        w = 26;
        h = 18;
        numArmies = 3;
        playerBases = 3;
        aiBases = 2;
        playerAirports = 1;
        aiAirports = 1;
        playerCities = 4;
        aiCities = 3;
        neutralCityPairs = 10;
    }
    else
    {
        w = 30;
        h = 20;
        numArmies = 4;
        playerBases = 3;
        aiBases = 3;
        playerAirports = 1;
        aiAirports = 1;
        playerCities = 4;
        aiCities = 3;
        neutralCityPairs = 12;
    }

    destBuf[0] = (u8)w;
    destBuf[1] = (u8)h;

    /* 1. Clear grids to Plains */
    for (i = 0; i < w * h; i++)
    {
        kind[i] = KIND_PLAIN;
        owner[i] = 0;
    }

    /* 2. Determine symmetric HQ corner coordinates */
    {
        int leftX = 3 + (int)Skirmish_RandRange(&rng, 2);
        int rightX = w - 1 - leftX;
        int topY = (h <= 14) ? 2 : (2 + (int)Skirmish_RandRange(&rng, 2));
        int botY = h - 1 - topY;
        int cx = w >> 1;
        int cy = h >> 1;
        int riverMode = (int)Skirmish_RandRange(&rng, 3);

        /* Army 1 (Player, OS): Bottom-Left */
        hx[0] = leftX;
        hy[0] = botY;
        dirX[0] = 1;
        dirY[0] = -1;

        /* Army 2 (AI 1, BM): Top-Right */
        hx[1] = rightX;
        hy[1] = topY;
        dirX[1] = -1;
        dirY[1] = 1;

        /* Army 3 (AI 2, GE): Top-Left */
        hx[2] = leftX;
        hy[2] = topY;
        dirX[2] = 1;
        dirY[2] = 1;

        /* Army 4 (AI 3, YC): Bottom-Right */
        hx[3] = rightX;
        hy[3] = botY;
        dirX[3] = -1;
        dirY[3] = -1;

        /* On 3-army maps, randomly place AI 2 in either Top-Left or Bottom-Right */
        if (numArmies == 3 && Skirmish_RandRange(&rng, 2) == 1)
        {
            hx[2] = hx[3];
            hy[2] = hy[3];
            dirX[2] = dirX[3];
            dirY[2] = dirY[3];
        }

        /* 3. Carve 1 or 2 natural rivers with straight bridge-crossing zones */
        if (riverMode == 0 || riverMode == 2)
        {
            int rx = cx;
            int lastStepY = -10;
            for (y = 0; y < h; y++)
            {
                kind[y * w + rx] = KIND_RIVER;
                if (y >= topY + 2 && y <= botY - 2
                    && (y < cy - 2 || y > cy + 2)
                    && y - lastStepY >= 2
                    && Skirmish_RandRange(&rng, 100) < 45)
                {
                    int step = (Skirmish_RandRange(&rng, 2) == 0) ? -1 : 1;
                    int nx = rx + step;
                    if (nx >= cx - 2 && nx <= cx + 2)
                    {
                        rx = nx;
                        kind[y * w + rx] = KIND_RIVER;
                        lastStepY = y;
                    }
                }
            }
        }
        if (riverMode == 1 || riverMode == 2)
        {
            int ry = cy;
            int lastStepX = -10;
            for (x = 0; x < w; x++)
            {
                kind[ry * w + x] = KIND_RIVER;
                if (x >= leftX + 2 && x <= rightX - 2
                    && (x < cx - 3 || x > cx + 3)
                    && x - lastStepX >= 2
                    && Skirmish_RandRange(&rng, 100) < 40)
                {
                    int step = (Skirmish_RandRange(&rng, 2) == 0) ? -1 : 1;
                    int ny = ry + step;
                    if (ny >= cy - 1 && ny <= cy + 1)
                    {
                        ry = ny;
                        kind[ry * w + x] = KIND_RIVER;
                        lastStepX = x;
                    }
                }
            }
        }

        /* 4. Scatter Mountain Ridges and Wood Groves on Plain tiles */
        {
            int numClusters = (w * h) >> 4;
            for (i = 0; i < numClusters; i++)
            {
                int mx = (int)Skirmish_RandRange(&rng, w - 2) + 1;
                int my = (int)Skirmish_RandRange(&rng, h - 2) + 1;
                u8 cKind = (Skirmish_RandRange(&rng, 100) < 42) ? KIND_MOUNTAIN : KIND_WOOD;
                int len = (int)Skirmish_RandRange(&rng, 4) + 2;
                int j;

                for (j = 0; j < len; j++)
                {
                    if (mx >= 1 && mx < w - 1 && my >= 1 && my < h - 1)
                    {
                        if (kind[my * w + mx] == KIND_PLAIN
                            && !Skirmish_IsInCornerZone(mx, my, w, h, 5))
                        {
                            kind[my * w + mx] = cKind;
                        }
                    }
                    if (Skirmish_RandRange(&rng, 2) == 0)
                        mx += (Skirmish_RandRange(&rng, 2) == 0) ? -1 : 1;
                    else
                        my += (Skirmish_RandRange(&rng, 2) == 0) ? -1 : 1;
                }
            }

            /* Light individual scatter */
            for (y = 1; y < h - 1; y++)
            {
                for (x = 1; x < w - 1; x++)
                {
                    int idx = y * w + x;
                    if (kind[idx] == KIND_PLAIN && !Skirmish_IsInCornerZone(x, y, w, h, 5))
                    {
                        u32 roll = Skirmish_RandRange(&rng, 100);
                        if (roll < 10)
                            kind[idx] = KIND_WOOD;
                        else if (roll < 15)
                            kind[idx] = KIND_MOUNTAIN;
                    }
                }
            }
        }

        /* 5. Carve Road Network across straight river crossing zones */
        Skirmish_CarveRoad(kind, w, h, leftX, topY, rightX, topY, 1);
        Skirmish_CarveRoad(kind, w, h, leftX, botY, rightX, botY, 1);
        Skirmish_CarveRoad(kind, w, h, leftX, topY, leftX, botY, 0);
        Skirmish_CarveRoad(kind, w, h, rightX, topY, rightX, botY, 0);

        if (riverMode == 0)
        {
            /* Central East-West highway across vertical river */
            Skirmish_CarveRoad(kind, w, h, leftX, cy, rightX, cy, 1);
        }
        else if (riverMode == 1)
        {
            /* Central North-South highway across horizontal river */
            Skirmish_CarveRoad(kind, w, h, cx, topY, cx, botY, 0);
        }
    }

    /* 6. Place HQs and starting owned Bases, Airports, and Cities */
    Skirmish_PlaceHomeCluster(
        kind, owner, w, h,
        hx[0], hy[0], dirX[0], dirY[0],
        1, playerBases, playerAirports, playerCities, &rng);

    for (i = 1; i < numArmies; i++)
    {
        Skirmish_PlaceHomeCluster(
            kind, owner, w, h,
            hx[i], hy[i], dirX[i], dirY[i],
            (u8)(i + 1), aiBases, aiAirports, aiCities, &rng);
    }

    /* 7. Place Contested Neutral Properties (Missile Silo, Neutral Bases/Airports, Cities) */
    {
        int cx = w >> 1;
        int cy = h >> 1;

        /* Central Missile Silo */
        {
            static const s8 sSiloDx[9] = { 0, -1, 1, 0, 0, -1, 1, -1, 1 };
            static const s8 sSiloDy[9] = { 0, 0, 0, -1, 1, -1, 1, 1, -1 };
            for (i = 0; i < 9; i++)
            {
                int sx = cx + sSiloDx[i];
                int sy = cy + sSiloDy[i];
                int idx = sy * w + sx;
                if (kind[idx] == KIND_PLAIN || kind[idx] == KIND_WOOD || kind[idx] == KIND_MOUNTAIN)
                {
                    kind[idx] = KIND_SILO;
                    owner[idx] = 0;
                    break;
                }
            }
        }

        /* Symmetric Neutral Expansion Bases near mid-edges */
        for (i = 0; i < 32; i++)
        {
            int mx1 = 2 + (int)Skirmish_RandRange(&rng, 5);
            int my1 = (h >> 1) + (int)Skirmish_RandRange(&rng, 5) - 2;
            int mx2 = w - 1 - mx1;
            int my2 = h - 1 - my1;

            if (kind[my1 * w + mx1] < KIND_RIVER && kind[my2 * w + mx2] < KIND_RIVER)
            {
                kind[my1 * w + mx1] = KIND_BASE;
                owner[my1 * w + mx1] = 0;
                kind[my2 * w + mx2] = KIND_BASE;
                owner[my2 * w + mx2] = 0;
                break;
            }
        }

        if (w >= 24)
        {
            for (i = 0; i < 32; i++)
            {
                int mx1 = (w >> 1) + (int)Skirmish_RandRange(&rng, 7) - 3;
                int my1 = 2 + (int)Skirmish_RandRange(&rng, 4);
                int mx2 = w - 1 - mx1;
                int my2 = h - 1 - my1;

                if (kind[my1 * w + mx1] < KIND_RIVER && kind[my2 * w + mx2] < KIND_RIVER)
                {
                    kind[my1 * w + mx1] = KIND_AIRPORT;
                    owner[my1 * w + mx1] = 0;
                    kind[my2 * w + mx2] = KIND_AIRPORT;
                    owner[my2 * w + mx2] = 0;
                    break;
                }
            }
        }

        /* Symmetric Neutral Cities across the map */
        {
            int pairsPlaced = 0;
            for (i = 0; i < 120 && pairsPlaced < neutralCityPairs; i++)
            {
                int nx = (int)Skirmish_RandRange(&rng, w - 2) + 1;
                int ny = (int)Skirmish_RandRange(&rng, h - 2) + 1;
                int sx = w - 1 - nx;
                int sy = h - 1 - ny;
                int idx1 = ny * w + nx;
                int idx2 = sy * w + sx;

                if (idx1 != idx2
                    && !Skirmish_IsInCornerZone(nx, ny, w, h, 5)
                    && !Skirmish_IsInCornerZone(sx, sy, w, h, 5)
                    && (kind[idx1] == KIND_PLAIN || kind[idx1] == KIND_WOOD || kind[idx1] == KIND_MOUNTAIN)
                    && (kind[idx2] == KIND_PLAIN || kind[idx2] == KIND_WOOD || kind[idx2] == KIND_MOUNTAIN))
                {
                    kind[idx1] = KIND_CITY;
                    owner[idx1] = 0;
                    kind[idx2] = KIND_CITY;
                    owner[idx2] = 0;
                    pairsPlaced++;
                }
            }
        }
    }

    /* 8. Sanitize Bridges so every bridge has valid straight river/road orientation */
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            int idx = y * w + x;
            if (kind[idx] == KIND_BRIDGE)
            {
                int rN = (y > 0) && (kind[(y - 1) * w + x] == KIND_RIVER);
                int rS = (y < h - 1) && (kind[(y + 1) * w + x] == KIND_RIVER);
                int rW = (x > 0) && (kind[y * w + (x - 1)] == KIND_RIVER);
                int rE = (x < w - 1) && (kind[y * w + (x + 1)] == KIND_RIVER);

                /* If river neighbors form a corner or T-junction on a bridge, convert to road */
                if ((rN || rS) && (rW || rE))
                    kind[idx] = KIND_ROAD;
                else if (!rN && !rS && !rW && !rE)
                    kind[idx] = KIND_ROAD;
            }
        }
    }

    /* 9. Auto-Tile Conversion to AW2 Visual Tile IDs (u16) */
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            int idx = y * w + x;
            u8 k = kind[idx];
            u8 own = owner[idx];
            int westTall = (x > 0) && Skirmish_IsTallKind(kind[idx - 1]);
            u16 tile = 1;

            if (k == KIND_PLAIN)
            {
                tile = westTall ? 33 : 1;
            }
            else if (k == KIND_WOOD)
            {
                int nWood = (y > 0) && (kind[(y - 1) * w + x] == KIND_WOOD);
                int sWood = (y < h - 1) && (kind[(y + 1) * w + x] == KIND_WOOD);
                if (nWood && sWood)
                    tile = 34;
                else if (nWood)
                    tile = 35;
                else if (sWood)
                    tile = 2;
                else
                    tile = 32;
            }
            else if (k == KIND_MOUNTAIN)
            {
                tile = ((x + y) & 1) ? 135 : 134;
            }
            else if (k == KIND_ROAD)
            {
                int m = 0;
                if (y > 0 && Skirmish_IsRoadLink(kind[(y - 1) * w + x]))
                    m |= 8;
                if (x < w - 1 && Skirmish_IsRoadLink(kind[y * w + (x + 1)]))
                    m |= 4;
                if (y < h - 1 && Skirmish_IsRoadLink(kind[(y + 1) * w + x]))
                    m |= 2;
                if (x > 0 && Skirmish_IsRoadLink(kind[y * w + (x - 1)]))
                    m |= 1;
                tile = sRoadTileTable[m];
                if (tile == 64 && westTall)
                    tile = 128;
            }
            else if (k == KIND_RIVER)
            {
                int m = 0;
                if (y == 0 || kind[(y - 1) * w + x] == KIND_RIVER || kind[(y - 1) * w + x] == KIND_BRIDGE)
                    m |= 8;
                if (x == w - 1 || kind[y * w + (x + 1)] == KIND_RIVER || kind[y * w + (x + 1)] == KIND_BRIDGE)
                    m |= 4;
                if (y == h - 1 || kind[(y + 1) * w + x] == KIND_RIVER || kind[(y + 1) * w + x] == KIND_BRIDGE)
                    m |= 2;
                if (x == 0 || kind[y * w + (x - 1)] == KIND_RIVER || kind[y * w + (x - 1)] == KIND_BRIDGE)
                    m |= 1;
                tile = sRiverTileTable[m];
            }
            else if (k == KIND_BRIDGE)
            {
                int riverNS = (y > 0 && kind[(y - 1) * w + x] == KIND_RIVER)
                           || (y < h - 1 && kind[(y + 1) * w + x] == KIND_RIVER);
                if (riverNS)
                    tile = westTall ? 19 : 20;
                else
                    tile = 22;
            }
            else if (k == KIND_HQ)
            {
                tile = (u16)(453 + (own - 1) * 5);
            }
            else if (k == KIND_BASE)
            {
                tile = (own == 0) ? 449 : (u16)(454 + (own - 1) * 5);
            }
            else if (k == KIND_CITY)
            {
                tile = (own == 0) ? 450 : (u16)(455 + (own - 1) * 5);
            }
            else if (k == KIND_AIRPORT)
            {
                tile = (own == 0) ? 451 : (u16)(456 + (own - 1) * 5);
            }
            else if (k == KIND_SILO)
            {
                tile = 384;
            }

            outTiles[idx] = tile;
        }
    }
}

const void * Skirmish_GetOrGenerateMapLz(u16 mapId, u32 variant, u8 * destBuf)
{
    if (Skirmish_IsSkirmishMode() && Skirmish_IsSkirmishMapId(mapId))
    {
        u32 savedSeed;

        Skirmish_EnsureStateInit();

        savedSeed = *(u32 *)(gUnknown_03003FC0 + 0x10);
        if (savedSeed != 0)
        {
            /* Locked in via Skirmish_OnSelectMap or restored from SRAM via Continue */
            SKIRMISH_STATE->activeSeed = savedSeed;
            SKIRMISH_STATE->cachedMapId = (u8)mapId;
            SKIRMISH_STATE->mapLocked = 1;
            SKIRMISH_STATE->needsReroll = 0;
        }

        if (!SKIRMISH_STATE->mapLocked)
        {
            if (SKIRMISH_STATE->needsReroll || SKIRMISH_STATE->cachedMapId != (u8)mapId)
            {
                Skirmish_AdvanceHardwareSeed(mapId);
                SKIRMISH_STATE->cachedMapId = (u8)mapId;
                SKIRMISH_STATE->needsReroll = 0;
            }
        }

        Skirmish_GenerateMap(mapId, SKIRMISH_STATE->activeSeed, destBuf);
        return NULL;
    }

    {
        const u8 * entry = gUnknown_085C77A0 + (mapId * 0x5C) + 0x2C;
        const void * lz = *(const void * const *)(entry + (variant << 2));
        if (lz == NULL)
            lz = *(const void * const *)entry;
        return lz;
    }
}
