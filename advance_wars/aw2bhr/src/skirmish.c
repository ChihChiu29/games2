#include "global.h"
#include "hardware.h"
#include "proc.h"

/*
 * Advance Wars 2: Procedural Skirmish Map Generator
 *
 * Adds a dedicated "Random" tab (category == 9) in Versus Mode with 11
 * procedural Random & Left-Right Mirrored setups:
 *   - 1v1     (mapId =  1): 1v1 Random Map (Player OS vs 1 CPU BM)
 *   - MIR 1v1 (mapId =  2): 1v1 Left-Right Mirrored (Player OS vs 1 CPU BM)
 *   - 1v1v1   (mapId = 59): 3-Way Free-For-All (Player OS vs CPUs BM, GE)
 *   - 1v2     (mapId = 60): 1v2 Outnumbered (Player Team A vs 2 Allied CPUs Team B)
 *   - 2v1     (mapId = 61): 2v1 Boss Raid (Player + Ally CPU Team A vs Boss CPU Team B)
 *   - 1v1v1v1 (mapId = 83): 4-Way Free-For-All (Player OS vs CPUs BM, GE, YC)
 *   - MIR FFA (mapId = 84): 4-Way Left-Right Mirrored Free-For-All
 *   - 2v2     (mapId = 85): 2v2 Team Battle (Player OS + CPU GE Team A vs CPUs BM + YC Team B)
 *   - MIR 2v2 (mapId = 86): 2v2 Left-Right Mirrored Team Battle
 *   - 1v1v2   (mapId = 87): 3-Team Battle (Player Team A vs Solo CPU Team B vs 2 CPUs Team C)
 *   - 1v3     (mapId = 88): 1v3 Survival (Player Team A vs 3 Allied CPUs Team B)
 */

#define SKIRMISH_MAP_1V1     1
#define SKIRMISH_MAP_MIR_1V1 2
#define SKIRMISH_MAP_1V1V1   59
#define SKIRMISH_MAP_1V2     60
#define SKIRMISH_MAP_2V1     61
#define SKIRMISH_MAP_1V1V1V1 83
#define SKIRMISH_MAP_MIR_FFA 84
#define SKIRMISH_MAP_2V2     85
#define SKIRMISH_MAP_MIR_2V2 86
#define SKIRMISH_MAP_1V1V2   87
#define SKIRMISH_MAP_1V3     88

#define SKIRMISH_MODE_COUNT      11
#define SKIRMISH_RANDOM_CATEGORY 9
#define SKIRMISH_MAGIC           0x534B524Du /* 'SKRM' */

struct SkirmishState
{
    u32 magic;
    u32 seed;
    u32 activeSeed;
    u8 cachedMapId;
    u8 mapLocked;
    u8 needsReroll;
    u8 modeApplied;
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
    KIND_SEA = 6,
    KIND_SEA_BRIDGE = 7,
    KIND_REEF = 8,
    KIND_HQ = 9,
    KIND_BASE = 10,
    KIND_CITY = 11,
    KIND_AIRPORT = 12,
    KIND_SILO = 13,
};

extern u8 gUnknown_03003FC0[];
extern u8 gUnknown_03003FF8[];
extern u8 gUnknown_03003FFD[];
extern u8 gUnknown_03004002[];
extern u8 gUnknown_0200C420[];
extern s16 gUnknown_0300596C;
extern u8 gUnknown_02027F78[];
extern u8 gUnknown_02027F74[];
extern u8 gUnknown_02028030[];
extern u8 gUnknown_020288A0[];
extern const u8 gUnknown_085C77A0[];
extern const char * const gUnknown_08610A38[];
extern const struct ProcCmd gUnknown_08616C54[];
extern const s16 gUnknown_08485DC4[];
extern s32 gUnknown_03004008;
extern u32 gUnknown_03001FD4;
extern u8 sub_08043CA0(void);

u8 Skirmish_PopulateMapList(u8 category);
void Skirmish_SetMapPlayed(u32 mapId, u32 val);
u8 Skirmish_IsMapUnlocked(u8 mapId);
u8 Skirmish_IsMapPlayed(u32 mapId);
const char * Skirmish_GetMapName(u16 mapId);
void Skirmish_OnSelectMap(u8 mapId);
const void * Skirmish_GetOrGenerateMapLz(u16 mapId, u32 variant, u8 * destBuf);

static const u16 sRoadTileTable[16] = {
    97, 97, 64, 66, 97, 97, 65, 224,
    64, 98, 64, 193, 96, 192, 225, 256,
};

/* AW2 West-Tall shadowed road tile counterparts from gUnknown_0848591C */
static const u16 sRoadShadowTileTable[16] = {
    161, 161, 128, 130, 161, 161, 129, 226,
    128, 162, 128, 195, 160, 194, 227, 257,
};

static const u16 sRiverTileTable[16] = {
    24, 57, 24, 89, 57, 57, 88, 280,
    24, 123, 24, 157, 120, 282, 218, 27,
};

struct SkirmishModeConfig
{
    u8 mapId;
    u8 numArmies;
    u8 isMirror;
    u8 teams[4];
    char name[8];
    char title[13];
};

static const struct SkirmishModeConfig sSkirmishModes[SKIRMISH_MODE_COUNT] = {
    { SKIRMISH_MAP_1V1,     2, 0, { 0, 1, 2, 3 }, "1v1",     "Random 1v1"   },
    { SKIRMISH_MAP_MIR_1V1, 2, 1, { 0, 1, 2, 3 }, "MIR 1v1", "Mirror 1v1"   },
    { SKIRMISH_MAP_1V1V1,   3, 0, { 0, 1, 2, 3 }, "1v1v1",   "Random 1v1v1" },
    { SKIRMISH_MAP_1V2,     3, 0, { 0, 1, 1, 1 }, "1v2",     "Outnumbered"  },
    { SKIRMISH_MAP_2V1,     3, 0, { 0, 0, 1, 1 }, "2v1",     "Boss Raid"    },
    { SKIRMISH_MAP_1V1V1V1, 4, 0, { 0, 1, 2, 3 }, "1v1v1v1", "Random FFA"   },
    { SKIRMISH_MAP_MIR_FFA, 4, 1, { 0, 1, 2, 3 }, "MIR FFA", "Mirror FFA"   },
    { SKIRMISH_MAP_2V2,     4, 0, { 0, 1, 0, 1 }, "2v2",     "Team Battle"  },
    { SKIRMISH_MAP_MIR_2V2, 4, 1, { 0, 1, 0, 1 }, "MIR 2v2", "Mirror Teams" },
    { SKIRMISH_MAP_1V1V2,   4, 0, { 0, 1, 2, 2 }, "1v1v2",   "Three Teams"  },
    { SKIRMISH_MAP_1V3,     4, 0, { 0, 1, 1, 1 }, "1v3",     "Survival"     },
};

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

static int Skirmish_IsLocalVersus(void)
{
    return (gUnknown_03003FC0[1] == 3
         && gUnknown_03003FC0[0x32] == 0);
}

static int Skirmish_IsSkirmishMode(void)
{
    return (Skirmish_IsLocalVersus()
         && gUnknown_0200C420[0x0D] == 1);
}

static const struct SkirmishModeConfig * Skirmish_GetModeConfig(u16 mapId)
{
    int i;
    for (i = 0; i < SKIRMISH_MODE_COUNT; i++)
    {
        if (sSkirmishModes[i].mapId == mapId)
            return &sSkirmishModes[i];
    }
    return NULL;
}

static int Skirmish_IsMirrorMapId(u16 mapId)
{
    const struct SkirmishModeConfig * cfg = Skirmish_GetModeConfig(mapId);
    return (cfg != NULL && cfg->isMirror != 0);
}

/*
 * Returns 1 only when mapId is one of our Random tab modes AND we are
 * either viewing the Random tab (category == 9) on the Versus Map Select
 * screen OR in an active/saved random Skirmish battle for this exact mapId.
 * This ensures all built-in tabs (2..8) behave 100% as normal built-in maps.
 */
static int Skirmish_IsActiveRandomMap(u16 mapId)
{
    if (!Skirmish_IsSkirmishMode() || Skirmish_GetModeConfig(mapId) == NULL)
        return 0;

    if (Proc_Find(gUnknown_08616C54) != NULL)
        return (gUnknown_0300596C == SKIRMISH_RANDOM_CATEGORY);

    return (*(u32 *)(gUnknown_03003FC0 + 0x10) != 0
         && gUnknown_03003FC0[2] == mapId);
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
        SKIRMISH_STATE->modeApplied = (savedSeed != 0) ? 1 : 0;
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

static void Skirmish_RestoreDefaultVersusSettings(void)
{
    static const u8 sVanillaDefaultCos[4] = { 1, 2, 4, 11 };
    int army;

    if (!Skirmish_IsLocalVersus())
    {
        if (SKIRMISH_STATE->magic == SKIRMISH_MAGIC)
            SKIRMISH_STATE->modeApplied = 0;
        return;
    }

    if (SKIRMISH_STATE->magic == SKIRMISH_MAGIC
        && SKIRMISH_STATE->modeApplied != 0)
    {
        SKIRMISH_STATE->modeApplied = 0;
        gUnknown_03003FF8[1] = 1;
        gUnknown_03003FF8[2] = 1;
        gUnknown_03003FF8[3] = 1;
        gUnknown_03003FF8[4] = 1;
        gUnknown_03003FFD[2] = 0;
        gUnknown_03003FFD[3] = 0;
        gUnknown_03003FFD[4] = 0;
        gUnknown_03004002[1] = 0;
        gUnknown_03004002[2] = 1;
        gUnknown_03004002[3] = 2;
        gUnknown_03004002[4] = 3;
    }

    /*
     * Populate any zeroed CO slots (from fresh boot, after a 2P/3P battle where
     * sub_0803C1D4 zeroes inactive slots, or after clearing Random mode CPU COs)
     * with distinct defaults from {1, 2, 4, 11} while preserving Player 1's CO.
     */
    for (army = 1; army <= 4; army++)
    {
        if (gUnknown_03003FFD[army] == 0)
        {
            int k;
            u8 pick = sVanillaDefaultCos[army - 1];
            for (k = 0; k < 4; k++)
            {
                u8 cand = sVanillaDefaultCos[(army - 1 + k) & 3];
                int dup = 0;
                int other;
                for (other = 1; other <= 4; other++)
                {
                    if (other != army && gUnknown_03003FFD[other] == cand)
                        dup = 1;
                }
                if (!dup)
                {
                    pick = cand;
                    break;
                }
            }
            gUnknown_03003FFD[army] = pick;
        }
    }
}

u8 Skirmish_PopulateMapList(u8 category)
{
    int i;

    if (!Skirmish_IsSkirmishMode())
    {
        u8 mode = gUnknown_03003FC0[1];
        if (Skirmish_IsLocalVersus())
            *(u32 *)(gUnknown_03003FC0 + 0x10) = 0;
        Skirmish_RestoreDefaultVersusSettings();
        if (category == SKIRMISH_RANDOM_CATEGORY)
            return 0;
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
    Skirmish_RestoreDefaultVersusSettings();

    /* Leave all built-in tabs (2..8, including War Room) 100% untouched */
    if (category != SKIRMISH_RANDOM_CATEGORY)
    {
        if ((gUnknown_02028030[0x10 + (category >> 3)] & (1 << (category & 7))) == 0)
            return 0;
        return 2;
    }

    /* Dedicated "Random" tab (category == 9) in Versus Mode */
    for (i = 0; i < SKIRMISH_MODE_COUNT; i++)
    {
        gUnknown_02027F78[i] = sSkirmishModes[i].mapId;
    }

    gUnknown_02027F74[0x36] = 0;
    gUnknown_02027F74[0x37] = (u8)(SKIRMISH_MODE_COUNT - 1);

    return 1;
}

void Skirmish_SetMapPlayed(u32 mapId, u32 val)
{
    u8 * p;
    u32 bit;
    u32 mask;

    /*
     * Do not mark the underlying built-in mapId as played when finishing
     * a procedural Random/Mirrored Skirmish battle.
     */
    if ((val & 0xFF) != 0 && Skirmish_IsActiveRandomMap((u16)mapId))
        return;

    p = &gUnknown_02028030[0x30 + (mapId >> 3)];
    bit = mapId & 7;
    mask = 1u << bit;
    *p = (u8)((*p & ~mask) | ((val & 0xFF) << bit));
}

u8 Skirmish_IsMapUnlocked(u8 mapId)
{
    /*
     * On the Versus Map Select screen (gUnknown_08616C54) while viewing the
     * Random tab, return 2 so sub_08087104 hides the facility counts (-1) and
     * sub_08086EB0 / sub_08086F3C draw the native '?' mystery preview.
     */
    if (Proc_Find(gUnknown_08616C54) != NULL && Skirmish_IsActiveRandomMap(mapId))
        return 2;

    return (gUnknown_02028030[0x12 + (mapId >> 3)] & (1 << (mapId & 7))) ? 1 : 0;
}

u8 Skirmish_IsMapPlayed(u32 mapId)
{
    /*
     * Render all modes in the Versus > Random tab with a uniform text color
     * on the Map Select screen, without affecting global completion checks
     * (such as sub_08084938 on the Main Menu).
     */
    if (Proc_Find(gUnknown_08616C54) != NULL && Skirmish_IsActiveRandomMap((u16)mapId))
        return 1;

    return (u8)(gUnknown_02028030[0x30 + (mapId >> 3)] & (1u << (mapId & 7)));
}

const char * Skirmish_GetMapName(u16 mapId)
{
    u16 nameIdx;

    if (Skirmish_IsActiveRandomMap(mapId))
    {
        const struct SkirmishModeConfig * cfg = Skirmish_GetModeConfig(mapId);
        if (cfg != NULL)
        {
            if (Proc_Find(gUnknown_08616C54) != NULL)
                return cfg->name;
            return cfg->title;
        }
    }

    nameIdx = *(const u16 *)(gUnknown_085C77A0 + (mapId * 0x5C) + 0x14);
    return gUnknown_08610A38[nameIdx];
}

static void Skirmish_RandomizeCpuCos(u32 seed)
{
    u32 rng = seed ^ 0xA5A5F00Du;
    int numUnlocked = (int)sub_08043CA0();
    int co;
    int army;

    /* Fallback to standard COs if save data has none flagged */
    if (numUnlocked <= 0)
    {
        static const u8 sDefaultCos[13] = {
            1, 2, 4, 3, 5, 8, 9, 6, 7, 11, 12, 13, 14
        };
        for (co = 0; co < 13; co++)
            gUnknown_020288A0[co] = sDefaultCos[co];
        gUnknown_020288A0[13] = 0xFF;
        numUnlocked = 13;
    }

    /* Ensure Player 1's CO slot is initialized on fresh boot before dedup */
    if (gUnknown_03003FFD[1] == 0)
        gUnknown_03003FFD[1] = gUnknown_020288A0[0];

    /* Assign random distinct unlocked COs to CPU armies (Armies 2, 3, 4) */
    for (army = 2; army <= 4; army++)
    {
        int tries;
        u8 chosen = gUnknown_020288A0[Skirmish_RandRange(&rng, (u32)numUnlocked)];
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
            chosen = gUnknown_020288A0[Skirmish_RandRange(&rng, (u32)numUnlocked)];
        }
        gUnknown_03003FFD[army] = chosen;
    }
}

void Skirmish_OnSelectMap(u8 mapId)
{
    const struct SkirmishModeConfig * cfg = Skirmish_GetModeConfig(mapId);

    gUnknown_03003FC0[2] = mapId;

    if (!Skirmish_IsSkirmishMode()
        || gUnknown_0300596C != SKIRMISH_RANDOM_CATEGORY
        || cfg == NULL)
    {
        if (Skirmish_IsLocalVersus())
            *(u32 *)(gUnknown_03003FC0 + 0x10) = 0;
        Skirmish_RestoreDefaultVersusSettings();
        return;
    }

    Skirmish_EnsureStateInit();
    Skirmish_AdvanceHardwareSeed(mapId);
    SKIRMISH_STATE->cachedMapId = mapId;
    SKIRMISH_STATE->needsReroll = 0;
    SKIRMISH_STATE->mapLocked = 1;
    SKIRMISH_STATE->modeApplied = 1;
    *(u32 *)(gUnknown_03003FC0 + 0x10) = SKIRMISH_STATE->activeSeed;
    gUnknown_03001FD4 ^= SKIRMISH_STATE->activeSeed;

    /* Force 1 Human Player (OS) vs CPU opponents (2 = CPU, 0 = Off) */
    gUnknown_03003FF8[1] = 1; /* Army 1 (OS): Human */
    gUnknown_03003FF8[2] = 2; /* Army 2 (BM): CPU */
    gUnknown_03003FF8[3] = (cfg->numArmies >= 3) ? 2 : 0;
    gUnknown_03003FF8[4] = (cfg->numArmies >= 4) ? 2 : 0;

    /* Randomize CPU COs from currently unlocked COs on the save file */
    Skirmish_RandomizeCpuCos(SKIRMISH_STATE->activeSeed);

    /* Apply mode team assignments (0..3 = Team A..D) */
    gUnknown_03004002[1] = cfg->teams[0];
    gUnknown_03004002[2] = cfg->teams[1];
    gUnknown_03004002[3] = cfg->teams[2];
    gUnknown_03004002[4] = cfg->teams[3];
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

static int Skirmish_IsBuildLand(u8 k)
{
    return (k == KIND_PLAIN || k == KIND_WOOD);
}

static int Skirmish_IsVehiclePassable(u8 k)
{
    return (k == KIND_PLAIN || k == KIND_WOOD || k == KIND_ROAD
         || k == KIND_BRIDGE || k == KIND_SEA_BRIDGE);
}

static int Skirmish_IsTallKind(u8 k)
{
    return (k == KIND_WOOD || k == KIND_MOUNTAIN || (k >= KIND_HQ && k <= KIND_AIRPORT));
}

static int Skirmish_IsSeaWater(u8 k)
{
    return (k == KIND_SEA || k == KIND_REEF || k == KIND_SEA_BRIDGE);
}

static int Skirmish_IsHorizBridge(const u8 * kind, int w, int h, int x, int y)
{
    u8 k = kind[y * w + x];
    if (k == KIND_BRIDGE)
    {
        return (y > 0 && kind[(y - 1) * w + x] == KIND_RIVER)
            || (y < h - 1 && kind[(y + 1) * w + x] == KIND_RIVER);
    }
    if (k == KIND_SEA_BRIDGE)
    {
        return (y == 0 || kind[(y - 1) * w + x] == KIND_SEA || kind[(y - 1) * w + x] == KIND_REEF)
            && (y == h - 1 || kind[(y + 1) * w + x] == KIND_SEA || kind[(y + 1) * w + x] == KIND_REEF);
    }
    return 0;
}

static int Skirmish_IsRoadLinkDir(const u8 * kind, int w, int h, int nx, int ny, int isHorizStep)
{
    u8 k = kind[ny * w + nx];
    if (k == KIND_ROAD)
        return 1;
    if (k == KIND_BRIDGE || k == KIND_SEA_BRIDGE)
        return (Skirmish_IsHorizBridge(kind, w, h, nx, ny) == isHorizStep);
    return 0;
}

static void Skirmish_ApplyRoadStep(u8 * kind, int idx)
{
    u8 k = kind[idx];
    if (k == KIND_RIVER)
        kind[idx] = KIND_BRIDGE;
    else if (k == KIND_SEA || k == KIND_REEF)
        kind[idx] = KIND_SEA_BRIDGE;
    else if (k != KIND_BRIDGE && k != KIND_SEA_BRIDGE && k < KIND_HQ)
        kind[idx] = KIND_ROAD;
}

static void Skirmish_CarveRoad(
    u8 * kind, int w, int h, int x0, int y0, int x1, int y1, int hvFirst)
{
    int x = x0;
    int y = y0;
    (void)h;

    while (x != x1 || y != y1)
    {
        Skirmish_ApplyRoadStep(kind, y * w + x);

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

    Skirmish_ApplyRoadStep(kind, y * w + x);
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
        if (kind[nIdx] != KIND_ROAD && kind[nIdx] != KIND_RIVER
            && kind[nIdx] != KIND_BRIDGE && !Skirmish_IsSeaWater(kind[nIdx]))
        {
            kind[nIdx] = KIND_PLAIN;
            owner[nIdx] = 0;
        }
    }

    /* Ensure immediate cardinal neighbors of HQ are passable */
    if (hx + dirX >= 1 && hx + dirX < w - 1)
    {
        int idx = hy * w + (hx + dirX);
        if (Skirmish_IsBuildLand(kind[idx]) || kind[idx] == KIND_MOUNTAIN)
            kind[idx] = KIND_ROAD;
    }
    if (hy + dirY >= 1 && hy + dirY < h - 1)
    {
        int idx = (hy + dirY) * w + hx;
        if (kind[idx] == KIND_WOOD || kind[idx] == KIND_MOUNTAIN)
            kind[idx] = KIND_PLAIN;
    }

    /* Place starting Bases (Factories) around HQ without overwriting carved roads */
    {
        int bx[10];
        int by[10];
        bx[0] = hx + dirX * 2;
        by[0] = hy;
        bx[1] = hx;
        by[1] = hy + dirY * 2;
        bx[2] = hx + dirX;
        by[2] = hy + dirY;
        bx[3] = hx + dirX * 2;
        by[3] = hy + dirY;
        bx[4] = hx - dirX;
        by[4] = hy + dirY;
        bx[5] = hx + dirX * 2;
        by[5] = hy + dirY * 2;
        bx[6] = hx + dirX;
        by[6] = hy - dirY;
        bx[7] = hx + dirX * 2;
        by[7] = hy - dirY;
        bx[8] = hx - dirX;
        by[8] = hy;
        bx[9] = hx - dirX;
        by[9] = hy + dirY * 2;

        placed = 0;
        for (i = 0; i < 10 && placed < numBases; i++)
        {
            int x = bx[i];
            int y = by[i];
            if (x >= 1 && x < w - 1 && y >= 1 && y < h - 1)
            {
                int idx = y * w + x;
                if ((Skirmish_IsBuildLand(kind[idx]) || kind[idx] == KIND_MOUNTAIN)
                    && !(x == hx && y == hy - 1))
                {
                    kind[idx] = KIND_BASE;
                    owner[idx] = army;
                    placed++;
                }
            }
        }
    }

    /* Place starting Airports (0..2) without overwriting carved roads */
    {
        int axList[8];
        int ayList[8];
        axList[0] = hx + dirX * 3;
        ayList[0] = hy + dirY;
        axList[1] = hx + dirX;
        ayList[1] = hy + dirY * 3;
        axList[2] = hx + dirX * 3;
        ayList[2] = hy;
        axList[3] = hx + dirX * 2;
        ayList[3] = hy + dirY;
        axList[4] = hx + dirX;
        ayList[4] = hy + dirY * 2;
        axList[5] = hx;
        ayList[5] = hy + dirY * 3;
        axList[6] = hx + dirX * 3;
        ayList[6] = hy + dirY * 2;
        axList[7] = hx - dirX;
        ayList[7] = hy + dirY * 2;

        placed = 0;
        for (i = 0; i < 8 && placed < numAirports; i++)
        {
            int ax = axList[i];
            int ay = ayList[i];
            if (ax >= 1 && ax < w - 1 && ay >= 1 && ay < h - 1)
            {
                int idx = ay * w + ax;
                if ((Skirmish_IsBuildLand(kind[idx]) || kind[idx] == KIND_MOUNTAIN)
                    && !(ax == hx && ay == hy - 1))
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
            if (Skirmish_IsBuildLand(kind[idx])
                && !(x == hx && y == hy - 1)
                && !(x == hx && y == hy + dirY))
            {
                kind[idx] = KIND_CITY;
                owner[idx] = army;
                placed++;
            }
        }
    }
}

static void Skirmish_SanitizeRiversAndBridges(u8 * kind, u8 * owner, int w, int h)
{
    int x;
    int y;
    int pass;

    /* 1. Keep rivers separated from sea */
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            int idx = y * w + x;
            if (kind[idx] == KIND_RIVER)
            {
                if ((y > 0 && Skirmish_IsSeaWater(kind[(y - 1) * w + x]))
                    || (y < h - 1 && Skirmish_IsSeaWater(kind[(y + 1) * w + x]))
                    || (x > 0 && Skirmish_IsSeaWater(kind[y * w + (x - 1)]))
                    || (x < w - 1 && Skirmish_IsSeaWater(kind[y * w + (x + 1)])))
                {
                    kind[idx] = KIND_PLAIN;
                }
            }
        }
    }

    /* 2. Ensure every sea bridge spans straight across open sea on both perpendicular sides */
    for (pass = 0; pass < 2; pass++)
    {
        for (y = 0; y < h; y++)
        {
            for (x = 0; x < w; x++)
            {
                int idx = y * w + x;
                if (kind[idx] == KIND_SEA_BRIDGE)
                {
                    int sN = (y == 0) || (kind[(y - 1) * w + x] == KIND_SEA) || (kind[(y - 1) * w + x] == KIND_REEF);
                    int sS = (y == h - 1) || (kind[(y + 1) * w + x] == KIND_SEA) || (kind[(y + 1) * w + x] == KIND_REEF);
                    int sW = (x == 0) || (kind[y * w + (x - 1)] == KIND_SEA) || (kind[y * w + (x - 1)] == KIND_REEF);
                    int sE = (x == w - 1) || (kind[y * w + (x + 1)] == KIND_SEA) || (kind[y * w + (x + 1)] == KIND_REEF);
                    int lW = (x > 0) && (kind[y * w + (x - 1)] != KIND_SEA) && (kind[y * w + (x - 1)] != KIND_REEF) && (kind[y * w + (x - 1)] != KIND_RIVER);
                    int lE = (x < w - 1) && (kind[y * w + (x + 1)] != KIND_SEA) && (kind[y * w + (x + 1)] != KIND_REEF) && (kind[y * w + (x + 1)] != KIND_RIVER);
                    int lN = (y > 0) && (kind[(y - 1) * w + x] != KIND_SEA) && (kind[(y - 1) * w + x] != KIND_REEF) && (kind[(y - 1) * w + x] != KIND_RIVER);
                    int lS = (y < h - 1) && (kind[(y + 1) * w + x] != KIND_SEA) && (kind[(y + 1) * w + x] != KIND_REEF) && (kind[(y + 1) * w + x] != KIND_RIVER);
                    int validWE = (sN && sS && lW && lE);
                    int validNS = (sW && sE && lN && lS);

                    if (!validWE && !validNS)
                        kind[idx] = KIND_ROAD;
                }
            }
        }
    }

    /* 3. Keep reefs strictly in open 3x3 sea water (never touching land/river/bridge shorelines) */
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            int idx = y * w + x;
            if (kind[idx] == KIND_REEF)
            {
                int dy;
                int dx;
                int ok = (x > 0 && x < w - 1 && y > 0 && y < h - 1);
                for (dy = -1; ok && dy <= 1; dy++)
                {
                    for (dx = -1; ok && dx <= 1; dx++)
                    {
                        u8 nk = kind[(y + dy) * w + (x + dx)];
                        if (nk != KIND_SEA && nk != KIND_REEF)
                            ok = 0;
                    }
                }
                if (!ok)
                    kind[idx] = KIND_SEA;
            }
        }
    }

    /* 4. Ensure every river bridge has valid straight river orientation on both river ends and land on road ends */
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
                int lW = (x > 0) && (kind[y * w + (x - 1)] != KIND_SEA) && (kind[y * w + (x - 1)] != KIND_REEF) && (kind[y * w + (x - 1)] != KIND_RIVER);
                int lE = (x < w - 1) && (kind[y * w + (x + 1)] != KIND_SEA) && (kind[y * w + (x + 1)] != KIND_REEF) && (kind[y * w + (x + 1)] != KIND_RIVER);
                int lN = (y > 0) && (kind[(y - 1) * w + x] != KIND_SEA) && (kind[(y - 1) * w + x] != KIND_REEF) && (kind[(y - 1) * w + x] != KIND_RIVER);
                int lS = (y < h - 1) && (kind[(y + 1) * w + x] != KIND_SEA) && (kind[(y + 1) * w + x] != KIND_REEF) && (kind[(y + 1) * w + x] != KIND_RIVER);
                int validNS = (rN && rS && lW && lE);
                int validWE = (rW && rE && lN && lS);

                if (!validNS && !validWE)
                    kind[idx] = KIND_ROAD;
            }
        }
    }

    /* 5. Ensure every Factory (KIND_BASE) has at least one vehicle-passable cardinal exit */
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            int idx = y * w + x;
            if (kind[idx] == KIND_BASE)
            {
                int vN = (y > 0) && Skirmish_IsVehiclePassable(kind[(y - 1) * w + x]);
                int vS = (y < h - 1) && Skirmish_IsVehiclePassable(kind[(y + 1) * w + x]);
                int vW = (x > 0) && Skirmish_IsVehiclePassable(kind[y * w + (x - 1)]);
                int vE = (x < w - 1) && Skirmish_IsVehiclePassable(kind[y * w + (x + 1)]);
                if (!vN && !vS && !vW && !vE)
                {
                    int inX = (x < (w >> 1)) ? (x + 1) : (x - 1);
                    int outX = (x < (w >> 1)) ? (x - 1) : (x + 1);
                    if (y < h - 1 && kind[(y + 1) * w + x] == KIND_MOUNTAIN)
                        kind[(y + 1) * w + x] = KIND_PLAIN;
                    else if (y > 0 && kind[(y - 1) * w + x] == KIND_MOUNTAIN)
                        kind[(y - 1) * w + x] = KIND_PLAIN;
                    else if (inX >= 0 && inX < w && kind[y * w + inX] == KIND_MOUNTAIN)
                        kind[y * w + inX] = KIND_PLAIN;
                    else if (outX >= 0 && outX < w && kind[y * w + outX] == KIND_MOUNTAIN)
                        kind[y * w + outX] = KIND_PLAIN;
                    else if (inX >= 0 && inX < w && kind[y * w + inX] == KIND_CITY)
                    {
                        kind[y * w + inX] = KIND_PLAIN;
                        owner[y * w + inX] = 0;
                    }
                    else if (y < h - 1 && kind[(y + 1) * w + x] == KIND_CITY)
                    {
                        kind[(y + 1) * w + x] = KIND_PLAIN;
                        owner[(y + 1) * w + x] = 0;
                    }
                    else if (y > 0 && kind[(y - 1) * w + x] == KIND_CITY)
                    {
                        kind[(y - 1) * w + x] = KIND_PLAIN;
                        owner[(y - 1) * w + x] = 0;
                    }
                    else if (outX >= 0 && outX < w && kind[y * w + outX] == KIND_CITY)
                    {
                        kind[y * w + outX] = KIND_PLAIN;
                        owner[y * w + outX] = 0;
                    }
                    else
                        kind[idx] = KIND_CITY;
                }
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
                int sWood = (y < h - 1) && (kind[(y + 1) * w + x] == KIND_WOOD);
                if (sWood)
                    tile = westTall ? 3 : 67;
                else
                    tile = westTall ? 33 : 1;
            }
            else if (k == KIND_WOOD)
            {
                int nCanopy = (y > 0) && (kind[(y - 1) * w + x] == KIND_WOOD
                                       || kind[(y - 1) * w + x] == KIND_PLAIN);
                int sWood = (y < h - 1) && (kind[(y + 1) * w + x] == KIND_WOOD);
                if (nCanopy && sWood)
                    tile = 34;
                else if (nCanopy)
                    tile = 35;
                else if (sWood)
                    tile = 2;
                else
                    tile = 32;
            }
            else if (k == KIND_MOUNTAIN)
            {
                tile = westTall ? 134 : 135;
            }
            else if (k == KIND_ROAD)
            {
                int m = 0;
                if (y > 0 && Skirmish_IsRoadLinkDir(kind, w, h, x, y - 1, 0))
                    m |= 8;
                if (x < w - 1 && Skirmish_IsRoadLinkDir(kind, w, h, x + 1, y, 1))
                    m |= 4;
                if (y < h - 1 && Skirmish_IsRoadLinkDir(kind, w, h, x, y + 1, 0))
                    m |= 2;
                if (x > 0 && Skirmish_IsRoadLinkDir(kind, w, h, x - 1, y, 1))
                    m |= 1;
                tile = westTall ? sRoadShadowTileTable[m] : sRoadTileTable[m];
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
                tile = Skirmish_IsHorizBridge(kind, w, h, x, y) ? 19 : 22;
            }
            else if (k == KIND_SEA)
            {
                /* Use AW2's native 512-entry 3x3 coastline auto-tile table (gUnknown_08485DC4) */
                int m = 0;
                s16 seaTile;
                if (y > 0)
                {
                    if (x > 0 && !Skirmish_IsSeaWater(kind[(y - 1) * w + (x - 1)]))
                        m |= (1 << 8);
                    if (!Skirmish_IsSeaWater(kind[(y - 1) * w + x]))
                        m |= (1 << 7);
                    if (x < w - 1 && !Skirmish_IsSeaWater(kind[(y - 1) * w + (x + 1)]))
                        m |= (1 << 6);
                }
                if (x > 0 && !Skirmish_IsSeaWater(kind[y * w + (x - 1)]))
                    m |= (1 << 5);
                if (x < w - 1 && !Skirmish_IsSeaWater(kind[y * w + (x + 1)]))
                    m |= (1 << 3);
                if (y < h - 1)
                {
                    if (x > 0 && !Skirmish_IsSeaWater(kind[(y + 1) * w + (x - 1)]))
                        m |= (1 << 2);
                    if (!Skirmish_IsSeaWater(kind[(y + 1) * w + x]))
                        m |= (1 << 1);
                    if (x < w - 1 && !Skirmish_IsSeaWater(kind[(y + 1) * w + (x + 1)]))
                        m |= (1 << 0);
                }
                seaTile = gUnknown_08485DC4[m];
                tile = (seaTile >= 0) ? (u16)seaTile : 42;
            }
            else if (k == KIND_SEA_BRIDGE)
            {
                tile = Skirmish_IsHorizBridge(kind, w, h, x, y) ? 20 : 54;
            }
            else if (k == KIND_REEF)
            {
                tile = 360;
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
 * Left-Right Mirrored Map Generator for MIR 1v1 (mapId = 2), MIR FFA (mapId = 84),
 * and MIR 2v2 (mapId = 86).
 * Uses an odd map width (w = 2 * midX + 1) so the center column (x = midX) lies
 * on the exact axis of symmetry, and mirrors every tile and property across
 * x <-> w - 1 - x.
 */
static void Skirmish_GenerateMirroredMap(u16 mapId, u32 seed, u8 * destBuf)
{
    u32 rng = seed;
    int is4P = (mapId != SKIRMISH_MAP_MIR_1V1);
    int sizeClass = (int)Skirmish_RandRange(&rng, 5); /* 0=Compact, 1=Std, 2=Tall/Square, 3=Wide, 4=Large */
    int w;
    int h;
    int midX;
    u8 * kind = destBuf + 1204;
    u8 * owner = destBuf + 1804;
    u16 * outTiles = (u16 *)(destBuf + 2);
    int numBases = 2 + (int)Skirmish_RandRange(&rng, 2);            /* 2..3 starting bases per army */
    int numAirports = (int)Skirmish_RandRange(&rng, 2);             /* 0..1 starting airports per army */
    int numCities = 2 + (int)Skirmish_RandRange(&rng, 3);           /* 2..4 starting cities per army */
    int neutralBasesPerSide = 1 + (int)Skirmish_RandRange(&rng, 2); /* 1..2 neutral bases per side */
    int neutralAirportsPerSide = (int)Skirmish_RandRange(&rng, 2);  /* 0..1 neutral airports per side */
    int neutralCitiesPerSide;
    int hasCenterRiver = (Skirmish_RandRange(&rng, 100) < 45);
    int hasHorizRiver = (Skirmish_RandRange(&rng, 100) < 45);
    int archetype = (int)Skirmish_RandRange(&rng, 5); /* 0=River/Coast, 1=GreatWall, 2=SeaStrait, 3=CenterCrater, 4=Lagoon */
    int midY1 = 2 + (int)Skirmish_RandRange(&rng, 2); /* 2..3 (top crossing) */
    int midY2;
    int cy;
    int hx1;
    int hy1;
    int hx3;
    int hy3;
    int x;
    int y;
    int i;

    if (is4P)
    {
        if (sizeClass == 0) /* Compact Brawl */
        {
            w = 21 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 21 or 23 */
            h = 14 + (int)Skirmish_RandRange(&rng, 2);        /* 14 or 15 */
        }
        else if (sizeClass == 2) /* Square / Tall Arena */
        {
            w = 23 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 23 or 25 */
            h = 20 + (int)Skirmish_RandRange(&rng, 3);        /* 20..22 */
        }
        else if (sizeClass == 3) /* Wide Canyon */
        {
            w = 29 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 29 or 31 (<= 32 minimap limit) */
            h = 14 + (int)Skirmish_RandRange(&rng, 3);        /* 14..16 */
        }
        else if (sizeClass == 4) /* Large / Epic */
        {
            w = 29 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 29 or 31 */
            h = 18 + (int)Skirmish_RandRange(&rng, 2);        /* 18 or 19 */
            neutralBasesPerSide++;
        }
        else /* Standard */
        {
            w = 25 + ((int)Skirmish_RandRange(&rng, 3) << 1); /* 25, 27, or 29 */
            h = 16 + (int)Skirmish_RandRange(&rng, 3);        /* 16..18 */
        }
    }
    else
    {
        if (sizeClass == 0) /* Compact / Blitz */
        {
            w = 19 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 19 or 21 */
            h = 12 + (int)Skirmish_RandRange(&rng, 2);        /* 12 or 13 */
        }
        else if (sizeClass == 2) /* Square / Tall Arena */
        {
            w = 19 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 19 or 21 */
            h = 19 + (int)Skirmish_RandRange(&rng, 4);        /* 19..22 */
        }
        else if (sizeClass == 3) /* Wide Frontline */
        {
            w = 27 + ((int)Skirmish_RandRange(&rng, 3) << 1); /* 27, 29, or 31 */
            h = 12 + (int)Skirmish_RandRange(&rng, 3);        /* 12..14 */
        }
        else if (sizeClass == 4) /* Large / Epic */
        {
            w = 27 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 27 or 29 */
            h = 18 + (int)Skirmish_RandRange(&rng, 3);        /* 18..20 */
            neutralBasesPerSide++;
        }
        else /* Standard */
        {
            w = 21 + ((int)Skirmish_RandRange(&rng, 3) << 1); /* 21, 23, or 25 */
            h = 15 + (int)Skirmish_RandRange(&rng, 3);        /* 15..17 */
        }
    }

    if (h < 14 && archetype >= 2)
        h = 14;

    midX = w >> 1;
    cy = h >> 1;
    if (h <= 16)
        midY1 = 2;
    midY2 = h - 1 - midY1;
    neutralCitiesPerSide = ((w * h) >> 6) + 2 + (int)Skirmish_RandRange(&rng, 5);

    destBuf[0] = (u8)w;
    destBuf[1] = (u8)h;

    for (i = 0; i < w * h; i++)
    {
        kind[i] = KIND_PLAIN;
        owner[i] = 0;
    }

    if (archetype == 0 && !hasCenterRiver && !hasHorizRiver)
        hasCenterRiver = 1;

    if (is4P)
    {
        /* Left half: Army 1 (OS) Bottom-Left, Army 3 (GE) Top-Left (aligned with midY2 / midY1) */
        hx1 = (w <= 23) ? 2 : (2 + ((int)Skirmish_RandRange(&rng, 2) << 1)); /* 2 or 4 */
        hy1 = midY2;
        hx3 = hx1;
        hy3 = midY1;
    }
    else
    {
        /* 1v1: Army 1 (OS) on Left in either Bottom-Left, Top-Left, or Mid-Left */
        hx1 = (w <= 23) ? 2 : (2 + (int)Skirmish_RandRange(&rng, 3));
        if (archetype == 0 && !hasHorizRiver
            && Skirmish_RandRange(&rng, 100) < 35)
            hy1 = cy;
        else if (Skirmish_RandRange(&rng, 2) == 0)
            hy1 = midY2;
        else
            hy1 = midY1;
        hx3 = -10;
        hy3 = -10;
    }

    if (archetype == 2 || archetype == 4)
    {
        if (numAirports < 1)
            numAirports = 1;
    }

    /* 1. Macro-topology on Left half + Centerline (mirrored to Right half) */
    if (archetype == 0)
    {
        int coveTopOk = (hx1 + hy1 > 8) && (!is4P || (hx3 + hy3 > 8));
        int coveBotOk = (hx1 + (h - 1 - hy1) > 8) && (!is4P || (hx3 + (h - 1 - hy3) > 8));

        /* Archetype 0: River Valley + Optional Coastal Sea Corners */
        if (hasHorizRiver)
        {
            int ry = cy;
            int lastStepX = -10;
            for (x = 0; x <= midX; x++)
            {
                if (Skirmish_Abs(x - hx1) + Skirmish_Abs(ry - hy1) > 2
                    && Skirmish_Abs(x - hx3) + Skirmish_Abs(ry - hy3) > 2)
                {
                    kind[ry * w + x] = KIND_RIVER;
                }
                if (x >= 7 && x <= midX - 3
                    && x - lastStepX >= 3
                    && Skirmish_Abs(x - (hx1 + 1)) >= 2
                    && Skirmish_Abs(x - (hx3 + 1)) >= 2
                    && Skirmish_RandRange(&rng, 100) < 40)
                {
                    int step = (Skirmish_RandRange(&rng, 2) == 0) ? -1 : 1;
                    int ny = ry + step;
                    if (ny >= 6 && ny < h - 6
                        && Skirmish_Abs(ny - cy) <= 1
                        && kind[ny * w + (x - 1)] != KIND_RIVER)
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
        /* Carve coastal sea in unoccupied corners */
        for (y = 0; y < h; y++)
        {
            for (x = 0; x <= 4; x++)
            {
                int dTop = x + y;
                int dBot = x + (h - 1 - y);
                if ((dTop <= 3 && coveTopOk) || (dBot <= 3 && coveBotOk))
                {
                    kind[y * w + x] = KIND_SEA;
                }
            }
        }
    }
    else if (archetype == 1)
    {
        /* Archetype 1: The Great Wall (Central Mountain Barrier + Center Pass Outpost) */
        for (y = 0; y < h; y++)
        {
            int thick = 1 + (int)Skirmish_RandRange(&rng, 2); /* midX-1..midX or midX-2..midX */
            for (x = midX - thick; x <= midX; x++)
            {
                if (x >= 0 && Skirmish_Abs(y - cy) > 1)
                    kind[y * w + x] = KIND_MOUNTAIN;
            }
        }
    }
    else if (archetype == 2)
    {
        /* Archetype 2: Bridged Sea Strait + Central Island */
        for (y = 0; y < h; y++)
        {
            int seaHalfW = (Skirmish_Abs(y - cy) <= 2) ? 3 : 1;
            for (x = midX - seaHalfW; x <= midX; x++)
            {
                if (x >= 0)
                    kind[y * w + x] = KIND_SEA;
            }
        }
        /* Central Island at (midX - 1 .. midX + 1, cy - 1 .. cy + 1), surrounded by sea on W/E */
        for (y = cy - 1; y <= cy + 1; y++)
        {
            for (x = midX - 1; x <= midX; x++)
                kind[y * w + x] = KIND_PLAIN;
        }
        /* Deep-water Reefs in the strait (keep at least 2 tiles from crossing bridges) */
        if (cy - 4 >= midY1 + 2)
            kind[(cy - 4) * w + midX] = KIND_REEF;
        if (cy + 4 <= midY2 - 2)
            kind[(cy + 4) * w + midX] = KIND_REEF;
    }
    else if (archetype == 3)
    {
        /* Archetype 3: Center Fortress / Hill Block with Base in the Middle */
        int crRy = (h >= 15) ? 3 : 2;
        for (y = cy - crRy; y <= cy + crRy; y++)
        {
            for (x = midX - 3; x <= midX; x++)
            {
                int dx = midX - x;
                int dy = Skirmish_Abs(y - cy);
                if (dx <= 1 && dy <= 1)
                    kind[y * w + x] = KIND_PLAIN;
                else if (!(dx == 3 && dy == crRy))
                    kind[y * w + x] = KIND_MOUNTAIN;
            }
        }
        /* Carve North/South passes into the crater so all 4 cardinal gates are open */
        for (y = cy - crRy; y <= cy + crRy; y++)
            kind[y * w + midX] = KIND_PLAIN;
    }
    else
    {
        /* Archetype 4: Central Lagoon / Atoll */
        int lagRx = (midX >= 10) ? 4 : 3;
        int lagRy = (h >= 15) ? 3 : 2;
        for (y = cy - lagRy; y <= cy + lagRy; y++)
        {
            for (x = midX - lagRx; x <= midX; x++)
            {
                int dx = midX - x;
                int dy = Skirmish_Abs(y - cy);
                if (dx <= 1 && dy <= 1)
                    kind[y * w + x] = KIND_PLAIN; /* Center Atoll Island */
                else
                    kind[y * w + x] = KIND_SEA;
            }
        }
        if (lagRx >= 4 && lagRy >= 3)
        {
            kind[(cy - 2) * w + (midX - 3)] = KIND_REEF;
            kind[(cy + 2) * w + (midX - 3)] = KIND_REEF;
        }
    }

    /* 2. Irregular terrain blobs on Left half + Centerline based on archetype */
    {
        int numBlobs = ((w * h) * 3) >> 6;
        int mountainChance = (archetype == 1 || archetype == 3) ? 58 : (archetype == 2 ? 32 : 42);
        int hubRx = (archetype >= 1) ? 3 : 2;
        int hubRy = (archetype >= 1) ? 3 : 1;
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
                        && !(Skirmish_Abs(bx - midX) <= hubRx && Skirmish_Abs(by - cy) <= hubRy)
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

    /* 3. Carve organic roads on Left half reaching the centerline (hvFirst = 0 so crossings are horizontal) */
    Skirmish_CarveRoad(kind, w, h, hx1 + 1, hy1, midX, midY2, 0);
    if (archetype >= 1 || hy1 == cy)
        Skirmish_CarveRoad(kind, w, h, hx1 + 1, hy1, midX, cy, 0);
    if (is4P)
    {
        Skirmish_CarveRoad(kind, w, h, hx3 + 1, hy3, midX, midY1, 0);
        Skirmish_CarveRoad(kind, w, h, hx1 + 1, hy1, hx3 + 1, hy3, 0);
    }
    else
    {
        Skirmish_CarveRoad(kind, w, h, hx1 + 1, hy1, midX, midY1, 0);
    }

    /* 4. Place Left-side Home Clusters */
    Skirmish_PlaceHomeCluster(
        kind, owner, w, h,
        hx1, hy1, 1, (hy1 >= cy) ? -1 : 1,
        1, numBases, numAirports, numCities, &rng);

    if (is4P)
    {
        Skirmish_PlaceHomeCluster(
            kind, owner, w, h,
            hx3, hy3, 1, 1,
            3, numBases, numAirports, numCities, &rng);
    }

    /* 5. Place Center Set-Piece Objectives + randomized neutral facilities on Left half */
    if (archetype >= 1)
    {
        /* Contested central objective in the Pass / Island / Crater / Atoll (keeping (midX, cy) road intact) */
        kind[(cy - 1) * w + midX] = KIND_BASE;
        owner[(cy - 1) * w + midX] = 0;
        kind[(cy + 1) * w + midX] = KIND_SILO;
        owner[(cy + 1) * w + midX] = 0;
        kind[(cy - 1) * w + (midX - 1)] = KIND_CITY;
        owner[(cy - 1) * w + (midX - 1)] = 0;
        if (archetype == 3)
        {
            kind[(cy + 1) * w + (midX - 1)] = KIND_AIRPORT;
            owner[(cy + 1) * w + (midX - 1)] = 0;
        }
    }

    {
        int placed = 0;
        for (i = 0; i < 40 && placed < neutralBasesPerSide; i++)
        {
            int nx = 2 + (int)Skirmish_RandRange(&rng, midX - 2);
            int ny = 2 + (int)Skirmish_RandRange(&rng, h - 4);
            int idx = ny * w + nx;
            if (Skirmish_IsBuildLand(kind[idx])
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
            if (Skirmish_IsBuildLand(kind[idx])
                && Skirmish_Abs(nx - hx1) + Skirmish_Abs(ny - hy1) > 4
                && Skirmish_Abs(nx - hx3) + Skirmish_Abs(ny - hy3) > 4)
            {
                kind[idx] = KIND_AIRPORT;
                owner[idx] = 0;
                placed++;
            }
        }

        if (archetype == 0 && Skirmish_RandRange(&rng, 100) < 70)
        {
            for (i = 0; i < 25; i++)
            {
                int sx = midX - (int)Skirmish_RandRange(&rng, 3);
                int sy = 3 + (int)Skirmish_RandRange(&rng, h - 6);
                int idx = sy * w + sx;
                if (Skirmish_IsBuildLand(kind[idx])
                    && Skirmish_Abs(sx - hx1) + Skirmish_Abs(sy - hy1) > 4
                    && Skirmish_Abs(sx - hx3) + Skirmish_Abs(sy - hy3) > 4)
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
            if (Skirmish_IsBuildLand(kind[idx])
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

    Skirmish_SanitizeRiversAndBridges(kind, owner, w, h);

    /* Re-enforce Left-to-Right mirror after directional sanitization passes */
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < midX; x++)
        {
            int srcIdx = y * w + x;
            int dstIdx = y * w + (w - 1 - x);
            u8 own = owner[srcIdx];

            kind[dstIdx] = kind[srcIdx];
            if (own == 1)
                owner[dstIdx] = 2;
            else if (own == 3)
                owner[dstIdx] = 4;
            else
                owner[dstIdx] = 0;
        }
    }

    Skirmish_ConvertToTiles(kind, owner, w, h, outTiles);
}

/*
 * Fully Random Irregular Map Generator for non-mirrored Random tab modes.
 */
static void Skirmish_GenerateRandomMap(u16 mapId, u32 seed, u8 * destBuf)
{
    const struct SkirmishModeConfig * cfg = Skirmish_GetModeConfig(mapId);
    u32 rng = seed;
    int w;
    int h;
    int numArmies = (cfg != NULL) ? cfg->numArmies : 2;
    int sizeClass = (int)Skirmish_RandRange(&rng, 5); /* 0=Compact, 1=Std, 2=Tall/Square, 3=Wide, 4=Large */
    int archetype = (int)Skirmish_RandRange(&rng, 8);
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
    int cx;
    int cy;
    int hasMidEdgeHq = 0;
    int topBay = 0;
    int penY = 2;
    int riverMode = 3;
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

    if (numArmies == 2)
    {
        if (sizeClass == 0) /* Compact / Blitz */
        {
            w = 18 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 18 or 20 */
            h = 12 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 12 or 14 */
        }
        else if (sizeClass == 2) /* Square / Tall Arena */
        {
            w = 18 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 18 or 20 */
            h = 20 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 20 or 22 */
        }
        else if (sizeClass == 3) /* Wide Canyon */
        {
            w = 26 + ((int)Skirmish_RandRange(&rng, 3) << 1); /* 26, 28, or 30 */
            h = 12 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 12 or 14 */
        }
        else if (sizeClass == 4) /* Large / Epic */
        {
            w = 26 + ((int)Skirmish_RandRange(&rng, 3) << 1); /* 26, 28, or 30 */
            h = 18 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 18 or 20 */
            neutralBases += 2;
        }
        else /* Standard */
        {
            w = 20 + ((int)Skirmish_RandRange(&rng, 3) << 1); /* 20, 22, or 24 */
            h = 14 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 14 or 16 */
        }
    }
    else if (numArmies == 3)
    {
        if (sizeClass == 0) /* Compact */
        {
            w = 20 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 20 or 22 */
            h = 14;
        }
        else if (sizeClass == 2) /* Square / Tall Arena */
        {
            w = 20 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 20 or 22 */
            h = 22 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 22 or 24 */
        }
        else if (sizeClass == 3) /* Wide Canyon */
        {
            w = 28 + ((int)Skirmish_RandRange(&rng, 3) << 1); /* 28, 30, or 32 */
            h = 14 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 14 or 16 */
        }
        else if (sizeClass == 4) /* Large / Epic */
        {
            w = 28 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 28 or 30 */
            h = 18 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 18 or 20 */
            neutralBases += 2;
        }
        else /* Standard */
        {
            w = 24 + ((int)Skirmish_RandRange(&rng, 3) << 1); /* 24, 26, or 28 */
            h = 16 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 16 or 18 */
        }
    }
    else
    {
        if (sizeClass == 0) /* Compact 4P Brawl */
        {
            w = 22 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 22 or 24 */
            h = 14 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 14 or 16 */
        }
        else if (sizeClass == 2) /* Square / Tall Arena */
        {
            w = 22 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 22 or 24 */
            h = 22 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 22 or 24 */
        }
        else if (sizeClass == 3) /* Wide Canyon */
        {
            w = 30 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 30 or 32 */
            h = 14 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 14 or 16 */
        }
        else if (sizeClass == 4) /* Large / Epic */
        {
            w = 28 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 28 or 30 */
            h = 20;
            neutralBases += 2;
        }
        else /* Standard */
        {
            w = 26 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 26 or 28 */
            h = 16 + ((int)Skirmish_RandRange(&rng, 2) << 1); /* 16 or 18 */
        }
    }

    if (w < 22 && (archetype == 4 || archetype == 6))
        w = 22;
    if (h < 14 && (archetype >= 2 && archetype <= 6))
        h = 14;

    cx = w >> 1;
    cy = h >> 1;

    /* Survival 1v3: give the solo player a fortified home cluster */
    if (mapId == SKIRMISH_MAP_1V3)
    {
        playerBases = 4;
        playerAirports = 2;
        playerCities = 5;
    }

    /* Guarantee starting Airports on sea-heavy archetypes (Archipelago / Lagoon / Bay) */
    if (archetype == 3 || archetype == 4 || archetype == 6)
    {
        if (playerAirports < 1)
            playerAirports = 1;
        if (aiAirports < 1)
            aiAirports = 1;
    }
    if (archetype == 7)
        neutralBases += 2;

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
        if (w <= 24 || archetype == 6)
        {
            oxL = 2;
            oxR = 2;
        }
        if (h <= 16 || archetype == 5 || archetype == 6)
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

        /* Varied HQ Spawn Formations (Mid-Edge 1v1 & Triangle 3P) on open-center archetypes */
        if ((archetype == 0 || archetype == 7) && h >= 14)
        {
            if (numArmies == 2 && Skirmish_RandRange(&rng, 100) < 32)
            {
                /* Mid-Edge Confrontation: West-Center vs East-Center (offset by +-2 from cy) */
                int leftArmy = (cornerOrder[0] & 1) ? 1 : 0;
                int rightArmy = 1 - leftArmy;
                int ySign = (Skirmish_RandRange(&rng, 2) == 0) ? -2 : 2;
                hasMidEdgeHq = 1;
                hx[leftArmy] = oxL;
                hy[leftArmy] = cy + ySign;
                dirX[leftArmy] = 1;
                dirY[leftArmy] = (hy[leftArmy] >= cy) ? -1 : 1;

                hx[rightArmy] = w - 1 - oxR;
                hy[rightArmy] = cy - ySign;
                dirX[rightArmy] = -1;
                dirY[rightArmy] = (hy[rightArmy] >= cy) ? -1 : 1;
            }
            else if (numArmies == 3 && Skirmish_RandRange(&rng, 100) < 38)
            {
                /* Triangle Formation: 2 corners on one side vs 1 near center on the opposite edge */
                int apexIdx = (int)Skirmish_RandRange(&rng, 3);
                int i1 = (apexIdx + 1) % 3;
                int i2 = (apexIdx + 2) % 3;
                int apexY = cy + ((Skirmish_RandRange(&rng, 2) == 0) ? -2 : 2);
                hasMidEdgeHq = 1;
                if (Skirmish_RandRange(&rng, 2) == 0)
                {
                    hx[apexIdx] = w - 1 - oxR;
                    hy[apexIdx] = apexY;
                    dirX[apexIdx] = -1;
                    dirY[apexIdx] = (apexY >= cy) ? -1 : 1;
                    hx[i1] = oxL;
                    hy[i1] = h - 1 - oyB;
                    dirX[i1] = 1;
                    dirY[i1] = -1;
                    hx[i2] = oxL;
                    hy[i2] = oyT;
                    dirX[i2] = 1;
                    dirY[i2] = 1;
                }
                else
                {
                    hx[apexIdx] = oxL;
                    hy[apexIdx] = apexY;
                    dirX[apexIdx] = 1;
                    dirY[apexIdx] = (apexY >= cy) ? -1 : 1;
                    hx[i1] = w - 1 - oxR;
                    hy[i1] = oyT;
                    dirX[i1] = -1;
                    dirY[i1] = 1;
                    hx[i2] = w - 1 - oxR;
                    hy[i2] = h - 1 - oyB;
                    dirX[i2] = -1;
                    dirY[i2] = -1;
                }
            }
        }
    }

    /* Central river axes and separated road hub waypoint */
    rxBase = cx + (int)Skirmish_RandRange(&rng, 3) - 1;
    ryBase = cy;
    hubX = rxBase + ((Skirmish_RandRange(&rng, 2) == 0) ? -2 : 2);
    hubY = (h <= 12) ? (ryBase - 2) : (ryBase + ((Skirmish_RandRange(&rng, 2) == 0) ? -2 : 2));

    /*
     * 2. Carve Macro-Archetype Geography (0..7):
     *    0 = Classic Riverlands + Coastal Coves
     *    1 = The Great Wall / Hill Barrier + Canyon Passes
     *    2 = Center Crater / Hill Block with Base in the Middle
     *    3 = Bridged Archipelago (Sea Channels + Central Island)
     *    4 = Central Lagoon / Atoll ("Donut" Sea Map)
     *    5 = Twin Massifs / Twin Lakes (3-Lane Corridor)
     *    6 = Coastal Sea Bay & Peninsula
     *    7 = Dense Forest Maze / Ambush Woods
     */
    if (archetype == 0 || archetype == 7)
    {
        riverMode = hasMidEdgeHq
                  ? ((Skirmish_RandRange(&rng, 4) == 0) ? 3 : 0)
                  : (int)Skirmish_RandRange(&rng, 4); /* 0=V, 1=H, 2=Both, 3=Dry */
        if (riverMode == 0 || riverMode == 2)
        {
            int rx = rxBase;
            int lastStepY = -10;
            for (y = 0; y < h; y++)
            {
                if (!Skirmish_NearAnyHq(rx, y, hx, hy, numArmies, 3))
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
                        && Skirmish_Abs(nx - rxBase) <= 1
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
                if (!Skirmish_NearAnyHq(x, ry, hx, hy, numArmies, 2))
                    kind[ry * w + x] = KIND_RIVER;
                if (x >= 7 && x < w - 7
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
        /* Carve coastal sea coves in unoccupied corners on archetype 0 */
        if (archetype == 0)
        {
            int cTL = !Skirmish_NearAnyHq(0, 0, hx, hy, numArmies, 8);
            int cTR = !Skirmish_NearAnyHq(w - 1, 0, hx, hy, numArmies, 8);
            int cBL = !Skirmish_NearAnyHq(0, h - 1, hx, hy, numArmies, 8);
            int cBR = !Skirmish_NearAnyHq(w - 1, h - 1, hx, hy, numArmies, 8);
            for (y = 0; y < h; y++)
            {
                for (x = 0; x < w; x++)
                {
                    int dTL = x + y;
                    int dTR = (w - 1 - x) + y;
                    int dBL = x + (h - 1 - y);
                    int dBR = (w - 1 - x) + (h - 1 - y);
                    if ((cTL && dTL <= 3) || (cTR && dTR <= 3)
                        || (cBL && dBL <= 3) || (cBR && dBR <= 3))
                    {
                        kind[y * w + x] = KIND_SEA;
                    }
                }
            }
        }
    }
    else if (archetype == 1)
    {
        /* Archetype 1: The Great Wall (Thick Jagged Mountain Barrier + Central Pass Clearing) */
        int wx = rxBase;
        hubX = rxBase;
        hubY = ryBase;
        for (y = 0; y < h; y++)
        {
            int halfT = 1 + (int)Skirmish_RandRange(&rng, 2); /* 2..3 tiles thick */
            if (y >= 3 && y < h - 3 && Skirmish_RandRange(&rng, 100) < 35)
            {
                int nx = wx + ((Skirmish_RandRange(&rng, 2) == 0) ? -1 : 1);
                if (nx >= cx - 2 && nx <= cx + 2)
                    wx = nx;
            }
            for (x = wx - halfT; x <= wx + halfT - 1; x++)
            {
                if (x >= 1 && x < w - 1
                    && !(Skirmish_Abs(x - hubX) <= 1 && Skirmish_Abs(y - hubY) <= 1)
                    && !Skirmish_NearAnyHq(x, y, hx, hy, numArmies, 3))
                {
                    kind[y * w + x] = KIND_MOUNTAIN;
                }
            }
        }
        /*On 3P/4P, add a horizontal ridge arm on one side */
        if (numArmies >= 3)
        {
            int side = (Skirmish_RandRange(&rng, 2) == 0) ? -1 : 1;
            int xStart = (side < 0) ? 1 : (hubX + 2);
            int xEnd = (side < 0) ? (hubX - 2) : (w - 2);
            for (x = xStart; x <= xEnd; x++)
            {
                for (y = ryBase - 1; y <= ryBase; y++)
                {
                    if (!Skirmish_NearAnyHq(x, y, hx, hy, numArmies, 3))
                        kind[y * w + x] = KIND_MOUNTAIN;
                }
            }
        }
    }
    else if (archetype == 2)
    {
        /* Archetype 2: Center Crater / Citadel (Hollow Mountain Block with Base in the Middle) */
        int crRx = (w >= 22) ? 4 : 3;
        int crRy = (h >= 16) ? 3 : 2;
        hubX = cx;
        hubY = cy;
        for (y = cy - crRy; y <= cy + crRy; y++)
        {
            for (x = cx - crRx; x <= cx + crRx; x++)
            {
                int dx = Skirmish_Abs(x - cx);
                int dy = Skirmish_Abs(y - cy);
                if (x >= 1 && x < w - 1 && y >= 1 && y < h - 1)
                {
                    if (dx <= (crRx - 2) && dy <= 1)
                        kind[y * w + x] = KIND_PLAIN;
                    else if (!(dx == crRx && dy == crRy)
                             && !Skirmish_NearAnyHq(x, y, hx, hy, numArmies, 3))
                        kind[y * w + x] = KIND_MOUNTAIN;
                }
            }
        }
        /* Carve 4 cardinal gates into the crater (W/E road + N/S mountain passes) */
        for (y = cy - crRy; y <= cy + crRy; y++)
            kind[y * w + cx] = KIND_PLAIN;
        for (x = cx - crRx; x <= cx + crRx; x++)
            kind[cy * w + x] = KIND_ROAD;
    }
    else if (archetype == 3)
    {
        /* Archetype 3: Bridged Archipelago (Sea Strait + Central Basin & Island + Coastal Bays) */
        hubX = cx;
        hubY = cy;
        /* Vertical Sea Strait widening into a central Sea Basin around the Central Island */
        for (y = 0; y < h; y++)
        {
            int seaHalfW = (Skirmish_Abs(y - cy) <= 2) ? 3 : 1;
            for (x = cx - seaHalfW; x <= cx + seaHalfW; x++)
            {
                if (x >= 0 && x < w && !Skirmish_NearAnyHq(x, y, hx, hy, numArmies, 4))
                    kind[y * w + x] = KIND_SEA;
            }
        }
        /* Restore 3x3 Central Island at (cx - 1 .. cx + 1, cy - 1 .. cy + 1), surrounded by 2-tile sea channels */
        for (y = cy - 1; y <= cy + 1; y++)
        {
            for (x = cx - 1; x <= cx + 1; x++)
                kind[y * w + x] = KIND_PLAIN;
        }
        /* Carve coastal sea corners away from HQs */
        {
            int cTL = !Skirmish_NearAnyHq(0, 0, hx, hy, numArmies, 9);
            int cTR = !Skirmish_NearAnyHq(w - 1, 0, hx, hy, numArmies, 9);
            int cBL = !Skirmish_NearAnyHq(0, h - 1, hx, hy, numArmies, 9);
            int cBR = !Skirmish_NearAnyHq(w - 1, h - 1, hx, hy, numArmies, 9);
            for (y = 0; y < h; y++)
            {
                for (x = 0; x < w; x++)
                {
                    int dTL = x + y;
                    int dTR = (w - 1 - x) + y;
                    int dBL = x + (h - 1 - y);
                    int dBR = (w - 1 - x) + (h - 1 - y);
                    if ((cTL && dTL <= 4) || (cTR && dTR <= 4)
                        || (cBL && dBL <= 4) || (cBR && dBR <= 4))
                    {
                        kind[y * w + x] = KIND_SEA;
                    }
                }
            }
        }
        if (cy - 4 >= 5)
            kind[(cy - 4) * w + cx] = KIND_REEF;
        if (cy + 4 <= h - 6)
            kind[(cy + 4) * w + cx] = KIND_REEF;
    }
    else if (archetype == 4)
    {
        /* Archetype 4: Central Lagoon / Atoll ("Donut" Sea Map with Bridged Center Atoll) */
        int rx = (w >= 28) ? 5 : ((w >= 22) ? 4 : 3);
        int ry = (h >= 16) ? 3 : 2;
        hubX = cx;
        hubY = cy;
        for (y = cy - ry; y <= cy + ry; y++)
        {
            for (x = cx - rx; x <= cx + rx; x++)
            {
                int dx = Skirmish_Abs(x - cx);
                int dy = Skirmish_Abs(y - cy);
                if (x >= 1 && x < w - 1 && y >= 1 && y < h - 1)
                {
                    if (dx <= 1 && dy <= 1)
                        kind[y * w + x] = KIND_PLAIN; /* Center Atoll Island */
                    else if (!Skirmish_NearAnyHq(x, y, hx, hy, numArmies, 4))
                        kind[y * w + x] = KIND_SEA;
                }
            }
        }
        if (rx >= 4 && ry >= 3)
        {
            kind[(cy - 2) * w + (cx - 3)] = KIND_REEF;
            kind[(cy - 2) * w + (cx + 3)] = KIND_REEF;
            kind[(cy + 2) * w + (cx - 3)] = KIND_REEF;
            kind[(cy + 2) * w + (cx + 3)] = KIND_REEF;
        }
    }
    else if (archetype == 5)
    {
        /* Archetype 5: Twin Massifs / Twin Lakes (3-Lane Tactical Corridor) */
        u8 blockKind = (Skirmish_RandRange(&rng, 2) == 0) ? KIND_MOUNTAIN : KIND_SEA;
        int maxDy = (h <= 14) ? 2 : ((h <= 16) ? 3 : 4);
        hubX = cx;
        hubY = cy;
        for (y = 4; y <= h - 5; y++)
        {
            if (Skirmish_Abs(y - cy) >= 2 && Skirmish_Abs(y - cy) <= maxDy)
            {
                for (x = cx - 3; x <= cx + 3; x++)
                {
                    if (x >= 2 && x < w - 2 && !Skirmish_NearAnyHq(x, y, hx, hy, numArmies, 4))
                        kind[y * w + x] = blockKind;
                }
            }
        }
    }
    else if (archetype == 6)
    {
        int cTL = !Skirmish_NearAnyHq(0, 0, hx, hy, numArmies, 9);
        int cTR = !Skirmish_NearAnyHq(w - 1, 0, hx, hy, numArmies, 9);
        int cBL = !Skirmish_NearAnyHq(0, h - 1, hx, hy, numArmies, 9);
        int cBR = !Skirmish_NearAnyHq(w - 1, h - 1, hx, hy, numArmies, 9);

        /* Archetype 6: Coastal Sea Bay & Peninsula */
        topBay = (Skirmish_RandRange(&rng, 2) == 0);
        penY = topBay ? 2 : (h - 3);
        hubX = cx;
        hubY = cy;
        for (y = 0; y < h; y++)
        {
            for (x = 0; x < w; x++)
            {
                int inTopBay = (topBay && y <= 4 && Skirmish_Abs(x - cx) <= 4);
                int inBotBay = (!topBay && y >= h - 5 && Skirmish_Abs(x - cx) <= 4);
                int dTL = x + y;
                int dTR = (w - 1 - x) + y;
                int dBL = x + (h - 1 - y);
                int dBR = (w - 1 - x) + (h - 1 - y);
                if (((inTopBay || inBotBay)
                     && !Skirmish_NearAnyHq(x, y, hx, hy, numArmies, 4))
                    || (cTL && dTL <= 4) || (cTR && dTR <= 4)
                    || (cBL && dBL <= 4) || (cBR && dBR <= 4))
                {
                    kind[y * w + x] = KIND_SEA;
                }
            }
        }
        /* Jutting Peninsula inside the Bay */
        for (y = (topBay ? 1 : (h - 5)); y <= (topBay ? 4 : (h - 2)); y++)
        {
            for (x = cx - 1; x <= cx + 1; x++)
            {
                if (y >= 1 && y < h - 1)
                    kind[y * w + x] = KIND_PLAIN;
            }
        }
        /* Deep-water Reefs flanking the Peninsula in the Bay */
        kind[(topBay ? 1 : (h - 2)) * w + (cx - 3)] = KIND_REEF;
        kind[(topBay ? 1 : (h - 2)) * w + (cx + 3)] = KIND_REEF;
        /* Mountain ridge spine on the opposite mainland half (leaving perimeter road rows open) */
        for (y = (topBay ? (cy + 3) : 3); y <= (topBay ? (h - 4) : (cy - 3)); y++)
        {
            if (!Skirmish_NearAnyHq(cx, y, hx, hy, numArmies, 3))
                kind[y * w + cx] = KIND_MOUNTAIN;
        }
        Skirmish_CarveRoad(kind, w, h, cx, cy, cx, topBay ? (penY + 1) : (penY - 1), 0);
    }

    /* 3. Scatter organic, irregular Mountain Ridges and Wood Groves */
    {
        int numClusters = (archetype == 7) ? ((w * h) >> 4) : (((w * h) * 6) >> 6);
        int mtnWeight = (archetype == 1 || archetype == 2) ? 60 : (archetype == 7 ? 20 : 42);
        int hubRx = (archetype >= 1 && archetype <= 6) ? 3 : 2;
        int hubRy = (archetype >= 1 && archetype <= 6) ? 3 : 1;

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
                    int inPeninsula = (archetype == 6
                                    && Skirmish_Abs(mx - cx) <= 2
                                    && (topBay ? (my <= 5) : (my >= h - 6)));
                    if (kind[my * w + mx] == KIND_PLAIN
                        && !inPeninsula
                        && !(Skirmish_Abs(mx - hubX) <= hubRx && Skirmish_Abs(my - hubY) <= hubRy)
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
                int inPeninsula = (archetype == 6
                                && Skirmish_Abs(x - cx) <= 2
                                && (topBay ? (y <= 5) : (y >= h - 6)));
                if (kind[idx] == KIND_PLAIN
                    && !inPeninsula
                    && !(Skirmish_Abs(x - hubX) <= hubRx && Skirmish_Abs(y - hubY) <= hubRy)
                    && !Skirmish_NearAnyHq(x, y, hx, hy, numArmies, 3))
                {
                    u32 roll = Skirmish_RandRange(&rng, 100);
                    if (roll < (u32)((archetype == 7) ? 18 : 11))
                        kind[idx] = KIND_WOOD;
                    else if (roll < (u32)((archetype == 7) ? 23 : 17))
                        kind[idx] = KIND_MOUNTAIN;
                }
            }
        }
    }

    /* 4. Carve organic road network through central hub waypoint */
    for (i = 0; i < numArmies; i++)
    {
        int next = (i + 1 < numArmies) ? (i + 1) : 0;
        int x0 = hx[i] + dirX[i];
        int y0 = hy[i];
        int x1 = hx[next] + dirX[next];
        int y1 = hy[next];
        int perimHv = (int)Skirmish_RandRange(&rng, 2);
        int skipPerim = 0;
        int toHubHvFirst = (archetype >= 1 && archetype <= 6)
                         ? 0
                         : ((riverMode == 0) ? 0
                         : ((riverMode == 1) ? 1
                         : ((riverMode == 2) ? (((x0 < rxBase) != (hubX < rxBase)) ? 0 : 1)
                         : ((Skirmish_Abs(x0 - hubX) >= Skirmish_Abs(y0 - hubY)) ? 0 : 1))));

        if (archetype == 6 && ((x0 < cx) != (x1 < cx)))
        {
            int inBay0 = topBay ? (y0 <= 4) : (y0 >= h - 5);
            int inBay1 = topBay ? (y1 <= 4) : (y1 >= h - 5);
            if (inBay0 && inBay1)
                skipPerim = 1;
            else if (inBay0)
                perimHv = 0;
            else if (inBay1)
                perimHv = 1;
        }

        if (!skipPerim)
        {
            Skirmish_CarveRoad(
                kind, w, h, x0, y0, x1, y1,
                perimHv);
        }
        if (i <= 1 || archetype >= 1 || Skirmish_RandRange(&rng, 2) == 0)
        {
            Skirmish_CarveRoad(
                kind, w, h, x0, y0, hubX, hubY,
                toHubHvFirst);
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
        int a = aiAirports;
        int c = aiCities + (int)Skirmish_RandRange(&rng, 2);
        if (b < 2)
            b = 2;
        /* Boss Raid 2v1: give the solo Boss CPU (Army 3 = GE) a fortified stronghold */
        if (mapId == SKIRMISH_MAP_2V1 && i == 2)
        {
            b = 4;
            a = 2;
            c = 5;
        }
        Skirmish_PlaceHomeCluster(
            kind, owner, w, h,
            hx[i], hy[i], dirX[i], dirY[i],
            (u8)(i + 1), b, a, c, &rng);
    }

    /* 6. Place Archetype Set-Piece Objectives in the Center + Scatter Neutral Properties */
    if (archetype >= 1 && archetype <= 6
        && !Skirmish_NearAnyHq(hubX, hubY, hx, hy, numArmies, 4))
    {
        int siloY = (archetype == 6 && topBay) ? (hubY + 1) : (hubY - 1);
        if (Skirmish_IsBuildLand(kind[siloY * w + hubX]))
        {
            kind[siloY * w + hubX] = KIND_SILO;
            owner[siloY * w + hubX] = 0;
        }

        if (!Skirmish_NearAnyHq(hubX - 1, hubY - 1, hx, hy, numArmies, 3)
            && Skirmish_IsBuildLand(kind[(hubY - 1) * w + (hubX - 1)]))
        {
            kind[(hubY - 1) * w + (hubX - 1)] = KIND_BASE;
            owner[(hubY - 1) * w + (hubX - 1)] = 0;
        }
        if (!Skirmish_NearAnyHq(hubX + 1, hubY + 1, hx, hy, numArmies, 3)
            && Skirmish_IsBuildLand(kind[(hubY + 1) * w + (hubX + 1)]))
        {
            kind[(hubY + 1) * w + (hubX + 1)] = (archetype == 2 || archetype == 4) ? KIND_BASE : KIND_CITY;
            owner[(hubY + 1) * w + (hubX + 1)] = 0;
        }
        if (archetype == 2 || archetype == 3 || archetype == 4 || archetype == 6)
        {
            if (!Skirmish_NearAnyHq(hubX + 1, hubY - 1, hx, hy, numArmies, 3)
                && Skirmish_IsBuildLand(kind[(hubY - 1) * w + (hubX + 1)]))
            {
                kind[(hubY - 1) * w + (hubX + 1)] = KIND_AIRPORT;
                owner[(hubY - 1) * w + (hubX + 1)] = 0;
            }
            if (!Skirmish_NearAnyHq(hubX - 1, hubY + 1, hx, hy, numArmies, 3)
                && Skirmish_IsBuildLand(kind[(hubY + 1) * w + (hubX - 1)]))
            {
                kind[(hubY + 1) * w + (hubX - 1)] = KIND_CITY;
                owner[(hubY + 1) * w + (hubX - 1)] = 0;
            }
        }
        if (archetype == 6)
        {
            kind[penY * w + cx] = KIND_BASE;
            owner[penY * w + cx] = 0;
            kind[penY * w + (cx - 1)] = KIND_CITY;
            owner[penY * w + (cx - 1)] = 0;
            kind[penY * w + (cx + 1)] = KIND_CITY;
            owner[penY * w + (cx + 1)] = 0;
        }
    }

    {
        int placed = 0;
        for (i = 0; i < 60 && placed < neutralBases; i++)
        {
            int nx = 2 + (int)Skirmish_RandRange(&rng, w - 4);
            int ny = 2 + (int)Skirmish_RandRange(&rng, h - 4);
            int idx = ny * w + nx;
            if (Skirmish_IsBuildLand(kind[idx]) && !Skirmish_NearAnyHq(nx, ny, hx, hy, numArmies, 5))
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
            if (Skirmish_IsBuildLand(kind[idx]) && !Skirmish_NearAnyHq(nx, ny, hx, hy, numArmies, 5))
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
            if (Skirmish_IsBuildLand(kind[idx]) && !Skirmish_NearAnyHq(sx, sy, hx, hy, numArmies, 4))
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
            if (Skirmish_IsBuildLand(kind[idx]) && !Skirmish_NearAnyHq(nx, ny, hx, hy, numArmies, 4))
            {
                kind[idx] = KIND_CITY;
                owner[idx] = 0;
                placed++;
            }
        }
    }

    Skirmish_SanitizeRiversAndBridges(kind, owner, w, h);
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
        if (savedSeed == 0 || Proc_Find(gUnknown_08616C54) != NULL)
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
