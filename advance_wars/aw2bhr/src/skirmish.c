#include "global.h"
#include "hardware.h"
#include "proc.h"

/*
 * Advance Wars 2: Procedural Skirmish Map Generator
 *
 * Adds 7 procedural Random & Left-Right Mirrored modes at the top of the
 * Versus Mode "War Room" tab (category == 7), followed by the original
 * War Room maps:
 *   - RAN 1v1   (mapId =  1): 1v1 (Player OS vs 1 CPU BM)
 *   - MIR 1v1   (mapId =  2): 1v1 Left-Right Mirrored (Player OS vs 1 CPU BM)
 *   - RAN 1v1v1 (mapId = 59): 1v1v1 FFA (Player OS vs 2 CPUs BM, GE)
 *   - RAN 1v2   (mapId = 60): 1v2 Team (Player Team A vs 2 Allied CPUs Team B)
 *   - RAN 2v2   (mapId = 83): 2v2 Team (Player OS + CPU GE Team A vs 2 CPUs BM + YC Team B)
 *   - MIR 2v2   (mapId = 84): 2v2 Left-Right Mirrored (Player OS + CPU GE Team A vs 2 CPUs BM + YC Team B)
 *   - RAN 1v3   (mapId = 85): 1v3 Team (Player Team A vs 3 Allied CPUs Team B)
 */

#define SKIRMISH_MAP_RAN_1V1   1
#define SKIRMISH_MAP_MIR_1V1   2
#define SKIRMISH_MAP_RAN_1V1V1 59
#define SKIRMISH_MAP_RAN_1V2   60
#define SKIRMISH_MAP_RAN_2V2   83
#define SKIRMISH_MAP_MIR_2V2   84
#define SKIRMISH_MAP_RAN_1V3   85

#define SKIRMISH_WAR_ROOM_CATEGORY 7
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
extern u8 gUnknown_03003FFD[];
extern u8 gUnknown_03004002[];
extern u8 gUnknown_0200C420[];
extern s16 gUnknown_0300596C;
extern u8 * const gUnknown_0849ECDC;
extern u8 gUnknown_02027F78[];
extern u8 gUnknown_02027F74[];
extern u8 gUnknown_02028030[];
extern const u8 gUnknown_085C77A0[];
extern const char * const gUnknown_08610A38[];
extern const struct ProcCmd gUnknown_08616B74[];
extern s32 gUnknown_03004008;
extern u32 gUnknown_03001FD4;

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

static const char sNameRan1v1[]   = "RAN 1v1";
static const char sNameMir1v1[]   = "MIR 1v1";
static const char sNameRan1v1v1[] = "RAN 1v1v1";
static const char sNameRan1v2[]   = "RAN 1v2";
static const char sNameRan2v2[]   = "RAN 2v2";
static const char sNameMir2v2[]   = "MIR 2v2";
static const char sNameRan1v3[]   = "RAN 1v3";

/* 20x16 '?' bitmap (1 bit per column in each 20-bit row) for Map Select preview */
static const u32 sQuestionMarkRows[16] = {
    0x00000u,
    0x00000u,
    0x01F80u, /*       ######        */
    0x03FC0u, /*      ########       */
    0x030C0u, /*      ##    ##       */
    0x030C0u, /*      ##    ##       */
    0x001C0u, /*            ###      */
    0x00380u, /*           ###       */
    0x00700u, /*          ###        */
    0x00600u, /*          ##         */
    0x00600u, /*          ##         */
    0x00000u, /*                     */
    0x00600u, /*          ##         */
    0x00600u, /*          ##         */
    0x00000u,
    0x00000u,
};

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
    return (gUnknown_03003FC0[1] == 3
         && gUnknown_03003FC0[0x32] == 0
         && gUnknown_0200C420[0x0D] == 1);
}

static int Skirmish_IsSkirmishMapId(u16 mapId)
{
    return (mapId == SKIRMISH_MAP_RAN_1V1
         || mapId == SKIRMISH_MAP_MIR_1V1
         || mapId == SKIRMISH_MAP_RAN_1V1V1
         || mapId == SKIRMISH_MAP_RAN_1V2
         || mapId == SKIRMISH_MAP_RAN_2V2
         || mapId == SKIRMISH_MAP_MIR_2V2
         || mapId == SKIRMISH_MAP_RAN_1V3);
}

static int Skirmish_IsMirrorMapId(u16 mapId)
{
    return (mapId == SKIRMISH_MAP_MIR_1V1
         || mapId == SKIRMISH_MAP_MIR_2V2);
}

/*
 * Returns 1 only when mapId is one of our 7 random/mirror modes AND we are
 * either viewing the War Room tab (category == 7) on the Versus Map Select
 * screen OR in an active/saved random Skirmish battle (savedSeed != 0).
 * This ensures maps 1, 2, 59, 60, 83, 84, 85 behave as normal built-in maps
 * on the 2P, 3P, and 4P tabs.
 */
static int Skirmish_IsActiveRandomMap(u16 mapId)
{
    if (!Skirmish_IsSkirmishMode() || !Skirmish_IsSkirmishMapId(mapId))
        return 0;

    if (Proc_Find(gUnknown_08616B74) != NULL)
        return (gUnknown_0300596C == SKIRMISH_WAR_ROOM_CATEGORY);

    return (*(u32 *)(gUnknown_03003FC0 + 0x10) != 0);
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
    int count;
    int m;

    if (!Skirmish_IsSkirmishMode())
    {
        u8 mode = gUnknown_03003FC0[1];
        if (mode != 2 && mode != 4)
        {
            if ((gUnknown_02028030[0x10 + (category >> 3)] & (1 << (category & 7))) == 0)
                return 0;
        }
        return 2;
    }

    *(u32 *)(gUnknown_03003FC0 + 0x10) = 0;
    Skirmish_EnsureStateInit();
    Skirmish_MixEntropy(category);
    SKIRMISH_STATE->mapLocked = 0;
    SKIRMISH_STATE->needsReroll = 1;

    /* Leave 2P, Pre-Deployed, 3P, 4P, and Design Maps tabs untouched */
    if (category != SKIRMISH_WAR_ROOM_CATEGORY)
    {
        if ((gUnknown_02028030[0x10 + (category >> 3)] & (1 << (category & 7))) == 0)
            return 0;
        return 2;
    }

    /* War Room tab in Versus Mode: 7 Random/Mirror modes at the top */
    gUnknown_02027F78[0] = SKIRMISH_MAP_RAN_1V1;
    gUnknown_02027F78[1] = SKIRMISH_MAP_MIR_1V1;
    gUnknown_02027F78[2] = SKIRMISH_MAP_RAN_1V1V1;
    gUnknown_02027F78[3] = SKIRMISH_MAP_RAN_1V2;
    gUnknown_02027F78[4] = SKIRMISH_MAP_RAN_2V2;
    gUnknown_02027F78[5] = SKIRMISH_MAP_MIR_2V2;
    gUnknown_02027F78[6] = SKIRMISH_MAP_RAN_1V3;
    count = 7;

    /* Followed by the original War Room maps (if War Room category is unlocked) */
    if ((gUnknown_02028030[0x10 + (SKIRMISH_WAR_ROOM_CATEGORY >> 3)]
         & (1 << (SKIRMISH_WAR_ROOM_CATEGORY & 7))) != 0)
    {
        for (m = 0; m <= 0xB3 && count < 0x32; m++)
        {
            u16 mapCat = *(const u16 *)(gUnknown_085C77A0 + (m * 0x5C) + 0x1A);
            u8 armies  = *(const u8 *)(gUnknown_085C77A0 + (m * 0x5C) + 0x18);
            if (mapCat == SKIRMISH_WAR_ROOM_CATEGORY && armies != 0 && armies != 0xFF)
            {
                gUnknown_02027F78[count++] = (u8)m;
            }
        }
    }

    gUnknown_02027F74[0x36] = 0;
    gUnknown_02027F74[0x37] = (u8)(count - 1);

    return 1;
}

u8 Skirmish_IsMapUnlocked(u8 mapId)
{
    /*
     * Return 2 for active random/mirror maps so sub_08087104 hides the
     * facility counts (-1), while all other callers treat non-zero as unlocked.
     */
    if (Skirmish_IsActiveRandomMap(mapId))
        return 2;

    return (gUnknown_02028030[0x12 + (mapId >> 3)] & (1 << (mapId & 7))) ? 1 : 0;
}

const char * Skirmish_GetMapName(u16 mapId)
{
    u16 nameIdx;

    if (Skirmish_IsActiveRandomMap(mapId))
    {
        if (mapId == SKIRMISH_MAP_RAN_1V1)
            return sNameRan1v1;
        if (mapId == SKIRMISH_MAP_MIR_1V1)
            return sNameMir1v1;
        if (mapId == SKIRMISH_MAP_RAN_1V1V1)
            return sNameRan1v1v1;
        if (mapId == SKIRMISH_MAP_RAN_1V2)
            return sNameRan1v2;
        if (mapId == SKIRMISH_MAP_RAN_2V2)
            return sNameRan2v2;
        if (mapId == SKIRMISH_MAP_MIR_2V2)
            return sNameMir2v2;
        if (mapId == SKIRMISH_MAP_RAN_1V3)
            return sNameRan1v3;
    }

    nameIdx = *(const u16 *)(gUnknown_085C77A0 + (mapId * 0x5C) + 0x14);
    return gUnknown_08610A38[nameIdx];
}

static void Skirmish_RandomizeCpuCos(u32 seed)
{
    u32 rng = seed ^ 0xA5A5F00Du;
    u8 unlockedCos[19];
    int numUnlocked = 0;
    int co;
    int army;

    for (co = 0; co <= 18; co++)
    {
        if ((gUnknown_02028030[0x2A + (co >> 3)] & (1 << (co & 7))) != 0)
        {
            unlockedCos[numUnlocked++] = (u8)co;
        }
    }

    /* Fallback to standard COs if save data has none flagged */
    if (numUnlocked == 0)
    {
        static const u8 sDefaultCos[13] = {
            1, 2, 4, 3, 5, 8, 9, 6, 7, 11, 12, 13, 14
        };
        for (co = 0; co < 13; co++)
            unlockedCos[co] = sDefaultCos[co];
        numUnlocked = 13;
    }

    /* Assign random distinct unlocked COs to CPU armies (Armies 2, 3, 4) */
    for (army = 2; army <= 4; army++)
    {
        int tries;
        u8 chosen = unlockedCos[Skirmish_RandRange(&rng, (u32)numUnlocked)];
        for (tries = 0; tries < 8; tries++)
        {
            int dup = 0;
            int prev;
            for (prev = 1; prev < army; prev++)
            {
                if (gUnknown_03003FFD[prev] == chosen)
                    dup = 1;
            }
            if (!dup)
                break;
            chosen = unlockedCos[Skirmish_RandRange(&rng, (u32)numUnlocked)];
        }
        gUnknown_03003FFD[army] = chosen;
    }
}

void Skirmish_OnSelectMap(u8 mapId)
{
    gUnknown_03003FC0[2] = mapId;

    if (!Skirmish_IsSkirmishMode()
        || gUnknown_0300596C != SKIRMISH_WAR_ROOM_CATEGORY
        || !Skirmish_IsSkirmishMapId(mapId))
    {
        *(u32 *)(gUnknown_03003FC0 + 0x10) = 0;
        if (Skirmish_IsSkirmishMode())
        {
            gUnknown_03004002[1] = 0;
            gUnknown_03004002[2] = 1;
            gUnknown_03004002[3] = 2;
            gUnknown_03004002[4] = 3;
        }
        return;
    }

    Skirmish_EnsureStateInit();
    Skirmish_AdvanceHardwareSeed(mapId);
    SKIRMISH_STATE->cachedMapId = mapId;
    SKIRMISH_STATE->needsReroll = 0;
    SKIRMISH_STATE->mapLocked = 1;
    *(u32 *)(gUnknown_03003FC0 + 0x10) = SKIRMISH_STATE->activeSeed;
    gUnknown_03001FD4 ^= SKIRMISH_STATE->activeSeed;

    /* Force 1 Human Player (OS) vs CPU opponents (2 = CPU with assigned CO) */
    gUnknown_0849ECDC[0] = 1;
    gUnknown_03003FF8[1] = 1; /* Army 1 (OS): Human */
    gUnknown_03003FF8[2] = 2; /* Army 2 (BM): CPU */
    gUnknown_03003FF8[3] = 2; /* Army 3 (GE): CPU */
    gUnknown_03003FF8[4] = 2; /* Army 4 (YC): CPU */

    /* Randomize CPU COs from currently unlocked COs on the save file */
    Skirmish_RandomizeCpuCos(SKIRMISH_STATE->activeSeed);

    if (mapId == SKIRMISH_MAP_RAN_2V2 || mapId == SKIRMISH_MAP_MIR_2V2)
    {
        /* 2v2 Team Battle: Player (OS) + Ally CPU (GE) vs Enemy CPUs (BM + YC) */
        gUnknown_03004002[1] = 0; /* Team A */
        gUnknown_03004002[2] = 1; /* Team B */
        gUnknown_03004002[3] = 0; /* Team A (CPU Ally) */
        gUnknown_03004002[4] = 1; /* Team B */
    }
    else if (mapId == SKIRMISH_MAP_RAN_1V2 || mapId == SKIRMISH_MAP_RAN_1V3)
    {
        /* 1v2 / 1v3 Team Battle: Player (Team A = 0) vs Allied CPUs (Team B = 1) */
        gUnknown_03004002[1] = 0;
        gUnknown_03004002[2] = 1;
        gUnknown_03004002[3] = 1;
        gUnknown_03004002[4] = 1;
    }
    else
    {
        /* Free-For-All (1v1 on RAN/MIR 1v1, 1v1v1 on RAN 1v1v1) */
        gUnknown_03004002[1] = 0;
        gUnknown_03004002[2] = 1;
        gUnknown_03004002[3] = 2;
        gUnknown_03004002[4] = 3;
    }
}

static int Skirmish_Abs(int v)
{
    return (v < 0) ? -v : v;
}

static int Skirmish_NearAnyHq(
    int x, int y, const int * hx, const int * hy, int numArmies, int dist)
{
    int i;
    for (i = 0; i < numArmies; i++)
    {
        if (Skirmish_Abs(x - hx[i]) + Skirmish_Abs(y - hy[i]) <= dist)
            return 1;
    }
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
        if (kind[idx] < KIND_RIVER)
            kind[idx] = KIND_ROAD;
    }
    if (hy + dirY >= 1 && hy + dirY < h - 1)
    {
        int idx = (hy + dirY) * w + hx;
        if (kind[idx] < KIND_RIVER)
            kind[idx] = KIND_ROAD;
    }

    /* Place starting Bases (Factories) around HQ */
    {
        int bx[6];
        int by[6];
        bx[0] = hx + dirX * 2;
        by[0] = hy;
        bx[1] = hx;
        by[1] = hy + dirY * 2;
        bx[2] = hx + dirX;
        by[2] = hy + dirY;
        bx[3] = hx - dirX;
        by[3] = hy + dirY;
        bx[4] = hx + dirX * 2;
        by[4] = hy + dirY * 2;
        bx[5] = hx + dirX;
        by[5] = hy - dirY;

        placed = 0;
        for (i = 0; i < 6 && placed < numBases; i++)
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

    /* Place starting Airports (0..2) */
    {
        int axList[3];
        int ayList[3];
        axList[0] = hx + dirX * 3;
        ayList[0] = hy + dirY;
        axList[1] = hx + dirX;
        ayList[1] = hy + dirY * 3;
        axList[2] = hx + dirX * 3;
        ayList[2] = hy;

        placed = 0;
        for (i = 0; i < 3 && placed < numAirports; i++)
        {
            int ax = axList[i];
            int ay = ayList[i];
            if (ax >= 1 && ax < w - 1 && ay >= 1 && ay < h - 1)
            {
                int idx = ay * w + ax;
                if (kind[idx] != KIND_RIVER && kind[idx] != KIND_BRIDGE
                    && kind[idx] < KIND_HQ && !(ax == hx && ay == hy - 1))
                {
                    kind[idx] = KIND_AIRPORT;
                    owner[idx] = army;
                    placed++;
                }
            }
        }
    }

    /* Place starting owned Cities in home zone */
    placed = 0;
    for (i = 0; i < 50 && placed < numCities; i++)
    {
        int dx = (int)Skirmish_RandRange(rng, 7) - 1;
        int dy = (int)Skirmish_RandRange(rng, 6) - 1;
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

static void Skirmish_SanitizeRiversAndBridges(u8 * kind, int w, int h)
{
    int x;
    int y;

    /* Ensure every bridge has valid straight river orientation on both river ends */
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            int idx = y * w + x;
            if (kind[idx] == KIND_BRIDGE)
            {
                int rN = (y == 0) || (kind[(y - 1) * w + x] == KIND_RIVER);
                int rS = (y == h - 1) || (kind[(y + 1) * w + x] == KIND_RIVER);
                int rW = (x == 0) || (kind[y * w + (x - 1)] == KIND_RIVER);
                int rE = (x == w - 1) || (kind[y * w + (x + 1)] == KIND_RIVER);
                int validNS = (rN && rS && !rW && !rE);
                int validWE = (rW && rE && !rN && !rS);

                if (!validNS && !validWE)
                    kind[idx] = KIND_ROAD;
            }
        }
    }
}

static void Skirmish_ConvertToTiles(const u8 * kind, const u8 * owner, int w, int h, u16 * outTiles)
{
    int x;
    int y;

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

/*
 * Generates a 20x16 '?' Mystery preview map into destBuf when browsing
 * the War Room tab on the Map Select screen.
 */
static void Skirmish_GenerateQuestionMarkPreview(u8 * destBuf)
{
    int w = 20;
    int h = 16;
    u16 * outTiles = (u16 *)(destBuf + 2);
    int x;
    int y;

    destBuf[0] = (u8)w;
    destBuf[1] = (u8)h;

    for (y = 0; y < h; y++)
    {
        u32 rowBits = sQuestionMarkRows[y];
        for (x = 0; x < w; x++)
        {
            int isBorder = (x == 0 || x == w - 1 || y == 0 || y == h - 1);
            int isQMark = (rowBits & (1u << (w - 1 - x))) != 0;
            u16 tile = 1; /* Plain (green background) */

            if (isQMark)
                tile = 134; /* Mountain (bold contrast '?' glyph, not counted as a facility) */
            else if (isBorder)
                tile = 32;  /* Wood border frame */

            outTiles[y * w + x] = tile;
        }
    }
}

/*
 * Left-Right Mirrored Map Generator for MIR 1v1 (mapId = 2) and MIR 2v2 (mapId = 84).
 * Uses an odd map width (w = 2 * midX + 1) so the center column (x = midX) lies
 * on the exact axis of symmetry, and mirrors every tile and property across
 * x <-> w - 1 - x.
 */
static void Skirmish_GenerateMirroredMap(u16 mapId, u32 seed, u8 * destBuf)
{
    u32 rng = seed;
    int is2v2 = (mapId == SKIRMISH_MAP_MIR_2V2);
    int w = is2v2
        ? (27 + ((int)Skirmish_RandRange(&rng, 2) << 1))  /* 27 or 29 */
        : (21 + ((int)Skirmish_RandRange(&rng, 3) << 1)); /* 21, 23, or 25 */
    int h = is2v2
        ? (17 + (int)Skirmish_RandRange(&rng, 2))         /* 17 or 18 */
        : (15 + (int)Skirmish_RandRange(&rng, 2));        /* 15 or 16 */
    int midX = w >> 1;
    u8 * kind = destBuf + 1204;
    u8 * owner = destBuf + 1804;
    u16 * outTiles = (u16 *)(destBuf + 2);
    int numBases = 2 + (int)Skirmish_RandRange(&rng, 2);            /* 2..3 starting bases per army */
    int numAirports = (int)Skirmish_RandRange(&rng, 2);             /* 0..1 starting airports per army */
    int numCities = 2 + (int)Skirmish_RandRange(&rng, 3);           /* 2..4 starting cities per army */
    int neutralBasesPerSide = 1 + (int)Skirmish_RandRange(&rng, 2); /* 1..2 neutral bases per side */
    int neutralAirportsPerSide = (int)Skirmish_RandRange(&rng, 2);  /* 0..1 neutral airports per side */
    int neutralCitiesPerSide = (is2v2 ? 6 : 5) + (int)Skirmish_RandRange(&rng, 5);
    int hasCenterRiver = (Skirmish_RandRange(&rng, 100) < 45);
    int hasHorizRiver = (Skirmish_RandRange(&rng, 100) < 45);
    int archetype = (int)Skirmish_RandRange(&rng, 4);
    int midY1 = 2 + (int)Skirmish_RandRange(&rng, 2); /* 2..3 (top crossing) */
    int midY2 = h - 1 - midY1;                        /* h-4..h-3 (bottom crossing) */
    int hx1;
    int hy1;
    int hx3;
    int hy3;
    int x;
    int y;
    int i;

    destBuf[0] = (u8)w;
    destBuf[1] = (u8)h;

    for (i = 0; i < w * h; i++)
    {
        kind[i] = KIND_PLAIN;
        owner[i] = 0;
    }

    if (is2v2)
    {
        /* Team A on Left: Army 1 (OS) Bottom-Left, Army 3 (GE) Top-Left */
        hx1 = 2 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 2 or 4 */
        hy1 = h - 3 - (int)Skirmish_RandRange(&rng, 2);    /* h-4 or h-3 */
        hx3 = hx1;
        hy3 = 2 + (int)Skirmish_RandRange(&rng, 2);        /* 2 or 3 */
    }
    else
    {
        /* 1v1: Army 1 (OS) on Left in either Bottom-Left or Top-Left quadrant */
        hx1 = 2 + (int)Skirmish_RandRange(&rng, 3);
        if (Skirmish_RandRange(&rng, 2) == 0)
            hy1 = h - 3 - (int)Skirmish_RandRange(&rng, 2);
        else
            hy1 = 2 + (int)Skirmish_RandRange(&rng, 2);
        hx3 = -10;
        hy3 = -10;
    }

    /* 1. Optional rivers on Left half + Centerline (mirrored to Right half) */
    if (hasHorizRiver)
    {
        int ry = (h >> 1);
        int lastStepX = -10;
        for (x = 0; x <= midX; x++)
        {
            kind[ry * w + x] = KIND_RIVER;
            if (x >= 6 && x <= midX - 3
                && x - lastStepX >= 3
                && Skirmish_Abs(x - hx1) >= 2
                && Skirmish_Abs(x - hx3) >= 2
                && Skirmish_RandRange(&rng, 100) < 40)
            {
                int step = (Skirmish_RandRange(&rng, 2) == 0) ? -1 : 1;
                int ny = ry + step;
                if (ny >= 6 && ny < h - 6)
                {
                    ry = ny;
                    kind[ry * w + x] = KIND_RIVER;
                    lastStepX = x;
                }
            }
        }
    }
    if (hasCenterRiver)
    {
        for (y = 0; y < h; y++)
            kind[y * w + midX] = KIND_RIVER;
    }

    /* 2. Irregular terrain blobs on Left half + Centerline based on archetype */
    {
        int numBlobs = is2v2 ? 25 : 18;
        int mountainChance = (archetype == 1) ? 60 : (archetype == 2 ? 25 : 42);
        for (i = 0; i < numBlobs; i++)
        {
            int bx = 1 + (int)Skirmish_RandRange(&rng, midX);
            int by = 1 + (int)Skirmish_RandRange(&rng, h - 2);
            u8 cKind = (Skirmish_RandRange(&rng, 100) < (u32)mountainChance) ? KIND_MOUNTAIN : KIND_WOOD;
            int len = 2 + (int)Skirmish_RandRange(&rng, 5);
            int j;

            for (j = 0; j < len; j++)
            {
                if (bx >= 1 && bx <= midX && by >= 1 && by < h - 1)
                {
                    if (kind[by * w + bx] == KIND_PLAIN
                        && Skirmish_Abs(bx - hx1) + Skirmish_Abs(by - hy1) > 3
                        && Skirmish_Abs(bx - hx3) + Skirmish_Abs(by - hy3) > 3)
                    {
                        kind[by * w + bx] = cKind;
                    }
                }
                if (Skirmish_RandRange(&rng, 2) == 0)
                    bx += (Skirmish_RandRange(&rng, 2) == 0) ? -1 : 1;
                else
                    by += (Skirmish_RandRange(&rng, 2) == 0) ? -1 : 1;
            }
        }
    }

    /* 3. Carve organic roads on Left half reaching the centerline (hvFirst = 0) */
    Skirmish_CarveRoad(kind, w, h, hx1, hy1, midX, midY2, 0);
    if (is2v2)
    {
        Skirmish_CarveRoad(kind, w, h, hx3, hy3, midX, midY1, 0);
        Skirmish_CarveRoad(kind, w, h, hx1, hy1, hx3, hy3, 0);
    }
    else
    {
        Skirmish_CarveRoad(kind, w, h, hx1, hy1, midX, midY1, 0);
    }

    /* 4. Place Left-side Home Clusters */
    Skirmish_PlaceHomeCluster(
        kind, owner, w, h,
        hx1, hy1, 1, (hy1 >= (h >> 1)) ? -1 : 1,
        1, numBases, numAirports, numCities, &rng);

    if (is2v2)
    {
        Skirmish_PlaceHomeCluster(
            kind, owner, w, h,
            hx3, hy3, 1, 1,
            3, numBases, numAirports, numCities, &rng);
    }

    /* 5. Place randomized neutral facilities on Left half (0 .. midX - 1) and Center (midX) */
    {
        int placed = 0;
        for (i = 0; i < 40 && placed < neutralBasesPerSide; i++)
        {
            int nx = 2 + (int)Skirmish_RandRange(&rng, midX - 2);
            int ny = 2 + (int)Skirmish_RandRange(&rng, h - 4);
            int idx = ny * w + nx;
            if (kind[idx] < KIND_RIVER
                && Skirmish_Abs(nx - hx1) + Skirmish_Abs(ny - hy1) > 4
                && Skirmish_Abs(nx - hx3) + Skirmish_Abs(ny - hy3) > 4)
            {
                kind[idx] = KIND_BASE;
                owner[idx] = 0;
                placed++;
            }
        }

        placed = 0;
        for (i = 0; i < 40 && placed < neutralAirportsPerSide; i++)
        {
            int nx = 2 + (int)Skirmish_RandRange(&rng, midX - 2);
            int ny = 2 + (int)Skirmish_RandRange(&rng, h - 4);
            int idx = ny * w + nx;
            if (kind[idx] < KIND_RIVER
                && Skirmish_Abs(nx - hx1) + Skirmish_Abs(ny - hy1) > 4
                && Skirmish_Abs(nx - hx3) + Skirmish_Abs(ny - hy3) > 4)
            {
                kind[idx] = KIND_AIRPORT;
                owner[idx] = 0;
                placed++;
            }
        }

        if (Skirmish_RandRange(&rng, 100) < 70)
        {
            for (i = 0; i < 25; i++)
            {
                int sx = midX - (int)Skirmish_RandRange(&rng, 3);
                int sy = 3 + (int)Skirmish_RandRange(&rng, h - 6);
                int idx = sy * w + sx;
                if (kind[idx] < KIND_RIVER)
                {
                    kind[idx] = KIND_SILO;
                    owner[idx] = 0;
                    break;
                }
            }
        }

        placed = 0;
        for (i = 0; i < 120 && placed < neutralCitiesPerSide; i++)
        {
            int nx = 1 + (int)Skirmish_RandRange(&rng, midX - 1);
            int ny = 1 + (int)Skirmish_RandRange(&rng, h - 2);
            int idx = ny * w + nx;
            if (kind[idx] < KIND_RIVER
                && Skirmish_Abs(nx - hx1) + Skirmish_Abs(ny - hy1) > 3
                && Skirmish_Abs(nx - hx3) + Skirmish_Abs(ny - hy3) > 3)
            {
                kind[idx] = KIND_CITY;
                owner[idx] = 0;
                placed++;
            }
        }
    }

    /* 6. Mirror Left half (0 .. midX - 1) to Right half (w - 1 - x) */
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < midX; x++)
        {
            int srcIdx = y * w + x;
            int dstIdx = y * w + (w - 1 - x);
            u8 k = kind[srcIdx];
            u8 own = owner[srcIdx];

            kind[dstIdx] = k;
            if (own == 1)
                owner[dstIdx] = 2; /* Army 1 (OS) <-> Army 2 (BM) */
            else if (own == 3)
                owner[dstIdx] = 4; /* Army 3 (GE) <-> Army 4 (YC) */
            else
                owner[dstIdx] = 0;
        }
    }

    Skirmish_SanitizeRiversAndBridges(kind, w, h);
    Skirmish_ConvertToTiles(kind, owner, w, h, outTiles);
}

/*
 * Fully Random Irregular Map Generator for RAN 1v1, RAN 1v1v1, RAN 1v2, RAN 2v2, RAN 1v3.
 */
static void Skirmish_GenerateRandomMap(u16 mapId, u32 seed, u8 * destBuf)
{
    u32 rng = seed;
    int w;
    int h;
    int numArmies;
    int archetype = (int)Skirmish_RandRange(&rng, 4);
    int playerBases = 2 + (int)Skirmish_RandRange(&rng, 3); /* 2..4 */
    int aiBases = 2 + (int)Skirmish_RandRange(&rng, 2);     /* 2..3 */
    int playerAirports = (int)Skirmish_RandRange(&rng, 3);  /* 0..2 */
    int aiAirports = (int)Skirmish_RandRange(&rng, 2);      /* 0..1 */
    int playerCities = 2 + (int)Skirmish_RandRange(&rng, 4);/* 2..5 */
    int aiCities = 2 + (int)Skirmish_RandRange(&rng, 3);    /* 2..4 */
    int neutralBases = 1 + (int)Skirmish_RandRange(&rng, 4);/* 1..4 */
    int neutralAirports = (int)Skirmish_RandRange(&rng, 3); /* 0..2 */
    int numSilos = (int)Skirmish_RandRange(&rng, 3);        /* 0..2 */
    int neutralCities;
    int rxBase;
    int ryBase;
    int hubX;
    int hubY;
    u8 * kind = destBuf + 1204;
    u8 * owner = destBuf + 1804;
    u16 * outTiles = (u16 *)(destBuf + 2);
    int hx[4];
    int hy[4];
    int dirX[4];
    int dirY[4];
    int cornerOrder[4];
    int x;
    int y;
    int i;

    if (mapId == SKIRMISH_MAP_RAN_1V1)
    {
        w = 20 + ((int)Skirmish_RandRange(&rng, 3) << 1); /* 20..24 */
        h = 14 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 14..16 */
        numArmies = 2;
    }
    else if (mapId == SKIRMISH_MAP_RAN_1V1V1 || mapId == SKIRMISH_MAP_RAN_1V2)
    {
        w = 24 + ((int)Skirmish_RandRange(&rng, 3) << 1); /* 24..28 */
        h = 16 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 16..18 */
        numArmies = 3;
    }
    else
    {
        w = 26 + ((int)Skirmish_RandRange(&rng, 3) << 1); /* 26..30 */
        h = 18 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 18..20 */
        numArmies = 4;
    }

    neutralCities = ((w * h) >> 5) + 2 + (int)Skirmish_RandRange(&rng, 10);

    destBuf[0] = (u8)w;
    destBuf[1] = (u8)h;

    for (i = 0; i < w * h; i++)
    {
        kind[i] = KIND_PLAIN;
        owner[i] = 0;
    }

    /* 1. Assign randomized quadrants/positions for each army's HQ */
    cornerOrder[0] = 0;
    cornerOrder[1] = 1;
    cornerOrder[2] = 2;
    cornerOrder[3] = 3;
    for (i = 3; i > 0; i--)
    {
        int j = (int)Skirmish_RandRange(&rng, (u32)(i + 1));
        int tmp = cornerOrder[i];
        cornerOrder[i] = cornerOrder[j];
        cornerOrder[j] = tmp;
    }
    /* On 1v1, ensure opponents are in opposite quadrants */
    if (numArmies == 2 && (cornerOrder[0] ^ cornerOrder[1]) != 1)
    {
        cornerOrder[1] = cornerOrder[0] ^ 1;
    }

    {
        /* Use even/aligned offsets so road legs never land 1 tile apart */
        int oxL = 2 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 2 or 4 */
        int oxR = 2 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 2 or 4 */
        int oyT = 2 + (int)Skirmish_RandRange(&rng, 2);        /* 2 or 3 */
        int oyB = 2 + (int)Skirmish_RandRange(&rng, 2);        /* 2 or 3 */
        if (h <= 14)
        {
            oyT = 2;
            oyB = 2;
        }

        for (i = 0; i < 4; i++)
        {
            int c = cornerOrder[i];
            if (c == 0) /* Bottom-Left */
            {
                hx[i] = oxL;
                hy[i] = h - 1 - oyB;
                dirX[i] = 1;
                dirY[i] = -1;
            }
            else if (c == 1) /* Top-Right */
            {
                hx[i] = w - 1 - oxR;
                hy[i] = oyT;
                dirX[i] = -1;
                dirY[i] = 1;
            }
            else if (c == 2) /* Top-Left */
            {
                hx[i] = oxL;
                hy[i] = oyT;
                dirX[i] = 1;
                dirY[i] = 1;
            }
            else /* Bottom-Right */
            {
                hx[i] = w - 1 - oxR;
                hy[i] = h - 1 - oyB;
                dirX[i] = -1;
                dirY[i] = -1;
            }
        }
    }

    /* Central river axes and separated road hub waypoint */
    rxBase = (w >> 1) + (int)Skirmish_RandRange(&rng, 3) - 1;
    ryBase = (h >> 1);
    hubX = rxBase + ((Skirmish_RandRange(&rng, 2) == 0) ? -2 : 2);
    hubY = ryBase + ((Skirmish_RandRange(&rng, 2) == 0) ? -2 : 2);

    /* 2. Carve irregular meandering rivers (0, 1, or 2 rivers) */
    {
        int riverMode = (int)Skirmish_RandRange(&rng, 4); /* 0=V, 1=H, 2=Both, 3=Dry */
        if (riverMode == 0 || riverMode == 2)
        {
            int rx = rxBase;
            int lastStepY = -10;
            for (y = 0; y < h; y++)
            {
                kind[y * w + rx] = KIND_RIVER;
                if (y >= 5 && y < h - 5
                    && y - lastStepY >= 3
                    && Skirmish_Abs(y - ryBase) >= 4
                    && Skirmish_Abs(y - hubY) >= 2
                    && Skirmish_RandRange(&rng, 100) < 48)
                {
                    int step = (Skirmish_RandRange(&rng, 2) == 0) ? -1 : 1;
                    int nx = rx + step;
                    if (nx >= 6 && nx < w - 6
                        && Skirmish_Abs(nx - hubX) >= 2
                        && !Skirmish_NearAnyHq(nx, y, hx, hy, numArmies, 3))
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
            int ry = ryBase;
            int lastStepX = -10;
            for (x = 0; x < w; x++)
            {
                kind[ry * w + x] = KIND_RIVER;
                if (x >= 6 && x < w - 6
                    && x - lastStepX >= 3
                    && Skirmish_Abs(x - hubX) >= 2
                    && Skirmish_RandRange(&rng, 100) < 45)
                {
                    int step = (Skirmish_RandRange(&rng, 2) == 0) ? -1 : 1;
                    int ny = ry + step;
                    if (ny >= 5 && ny < h - 5
                        && Skirmish_Abs(ny - ryBase) <= 1
                        && Skirmish_Abs(ny - hubY) >= 2
                        && kind[ny * w + (x - 1)] != KIND_RIVER
                        && kind[ny * w + x] != KIND_RIVER
                        && kind[ny * w + (x + 1)] != KIND_RIVER
                        && kind[ry * w + (x + 1)] != KIND_RIVER
                        && !Skirmish_NearAnyHq(x, ny, hx, hy, numArmies, 3))
                    {
                        ry = ny;
                        kind[ry * w + x] = KIND_RIVER;
                        lastStepX = x;
                    }
                }
            }
        }
    }

    /* 3. Scatter organic, irregular Mountain Ridges and Wood Groves */
    {
        int numClusters = (archetype == 3) ? ((w * h) >> 4) : (((w * h) * 6) >> 6);
        int mtnWeight = (archetype == 1) ? 62 : (archetype == 2 ? 24 : 42);

        for (i = 0; i < numClusters; i++)
        {
            int mx = 1 + (int)Skirmish_RandRange(&rng, w - 2);
            int my = 1 + (int)Skirmish_RandRange(&rng, h - 2);
            u8 cKind = (Skirmish_RandRange(&rng, 100) < (u32)mtnWeight) ? KIND_MOUNTAIN : KIND_WOOD;
            int len = 3 + (int)Skirmish_RandRange(&rng, 6);
            int j;

            for (j = 0; j < len; j++)
            {
                if (mx >= 1 && mx < w - 1 && my >= 1 && my < h - 1)
                {
                    if (kind[my * w + mx] == KIND_PLAIN
                        && !Skirmish_NearAnyHq(mx, my, hx, hy, numArmies, 3))
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

        /* Individual scatter for organic texture */
        for (y = 1; y < h - 1; y++)
        {
            for (x = 1; x < w - 1; x++)
            {
                int idx = y * w + x;
                if (kind[idx] == KIND_PLAIN && !Skirmish_NearAnyHq(x, y, hx, hy, numArmies, 3))
                {
                    u32 roll = Skirmish_RandRange(&rng, 100);
                    if (roll < 11)
                        kind[idx] = KIND_WOOD;
                    else if (roll < 17)
                        kind[idx] = KIND_MOUNTAIN;
                }
            }
        }
    }

    /* 4. Carve organic road network through central hub waypoint */
    for (i = 0; i < numArmies; i++)
    {
        int next = (i + 1 < numArmies) ? (i + 1) : 0;
        Skirmish_CarveRoad(
            kind, w, h, hx[i], hy[i], hx[next], hy[next],
            (int)Skirmish_RandRange(&rng, 2));
        if (i == 0 || Skirmish_RandRange(&rng, 2) == 0)
        {
            Skirmish_CarveRoad(
                kind, w, h, hx[i], hy[i], hubX, hubY,
                (int)Skirmish_RandRange(&rng, 2));
        }
    }

    /* 5. Place HQs and starting owned Bases, Airports, and Cities */
    Skirmish_PlaceHomeCluster(
        kind, owner, w, h,
        hx[0], hy[0], dirX[0], dirY[0],
        1, playerBases, playerAirports, playerCities, &rng);

    for (i = 1; i < numArmies; i++)
    {
        int b = aiBases + (int)Skirmish_RandRange(&rng, 2) - 1;
        int c = aiCities + (int)Skirmish_RandRange(&rng, 2);
        if (b < 2)
            b = 2;
        Skirmish_PlaceHomeCluster(
            kind, owner, w, h,
            hx[i], hy[i], dirX[i], dirY[i],
            (u8)(i + 1), b, aiAirports, c, &rng);
    }

    /* 6. Scatter randomized Neutral Bases, Airports, Missile Silos, and Cities */
    {
        int placed = 0;
        for (i = 0; i < 60 && placed < neutralBases; i++)
        {
            int nx = 2 + (int)Skirmish_RandRange(&rng, w - 4);
            int ny = 2 + (int)Skirmish_RandRange(&rng, h - 4);
            int idx = ny * w + nx;
            if (kind[idx] < KIND_RIVER && !Skirmish_NearAnyHq(nx, ny, hx, hy, numArmies, 5))
            {
                kind[idx] = KIND_BASE;
                owner[idx] = 0;
                placed++;
            }
        }

        placed = 0;
        for (i = 0; i < 60 && placed < neutralAirports; i++)
        {
            int nx = 2 + (int)Skirmish_RandRange(&rng, w - 4);
            int ny = 2 + (int)Skirmish_RandRange(&rng, h - 4);
            int idx = ny * w + nx;
            if (kind[idx] < KIND_RIVER && !Skirmish_NearAnyHq(nx, ny, hx, hy, numArmies, 5))
            {
                kind[idx] = KIND_AIRPORT;
                owner[idx] = 0;
                placed++;
            }
        }

        placed = 0;
        for (i = 0; i < 40 && placed < numSilos; i++)
        {
            int sx = (w >> 2) + (int)Skirmish_RandRange(&rng, w >> 1);
            int sy = (h >> 2) + (int)Skirmish_RandRange(&rng, h >> 1);
            int idx = sy * w + sx;
            if (kind[idx] < KIND_RIVER && !Skirmish_NearAnyHq(sx, sy, hx, hy, numArmies, 4))
            {
                kind[idx] = KIND_SILO;
                owner[idx] = 0;
                placed++;
            }
        }

        placed = 0;
        for (i = 0; i < 160 && placed < neutralCities; i++)
        {
            int nx = 1 + (int)Skirmish_RandRange(&rng, w - 2);
            int ny = 1 + (int)Skirmish_RandRange(&rng, h - 2);
            int idx = ny * w + nx;
            if (kind[idx] < KIND_RIVER && !Skirmish_NearAnyHq(nx, ny, hx, hy, numArmies, 4))
            {
                kind[idx] = KIND_CITY;
                owner[idx] = 0;
                placed++;
            }
        }
    }

    Skirmish_SanitizeRiversAndBridges(kind, w, h);
    Skirmish_ConvertToTiles(kind, owner, w, h, outTiles);
}

const void * Skirmish_GetOrGenerateMapLz(u16 mapId, u32 variant, u8 * destBuf)
{
    if (Skirmish_IsActiveRandomMap(mapId))
    {
        u32 savedSeed = *(u32 *)(gUnknown_03003FC0 + 0x10);

        Skirmish_EnsureStateInit();

        /*
         * While browsing the Map Select screen (before Skirmish_OnSelectMap
         * locks in activeSeed), render a '?' mystery preview instead of
         * revealing the generated terrain.
         */
        if (savedSeed == 0 || Proc_Find(gUnknown_08616B74) != NULL)
        {
            Skirmish_GenerateQuestionMarkPreview(destBuf);
            return NULL;
        }

        SKIRMISH_STATE->activeSeed = savedSeed;
        SKIRMISH_STATE->cachedMapId = (u8)mapId;
        SKIRMISH_STATE->mapLocked = 1;
        SKIRMISH_STATE->needsReroll = 0;

        if (Skirmish_IsMirrorMapId(mapId))
            Skirmish_GenerateMirroredMap(mapId, savedSeed, destBuf);
        else
            Skirmish_GenerateRandomMap(mapId, savedSeed, destBuf);

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
