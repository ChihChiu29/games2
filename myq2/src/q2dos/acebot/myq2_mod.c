/*
 * myq2_mod.c - MyQ2 Random Bot DM, Item Shuffle, Mutators & Slow HP/Ammo Regen
 */

#include <time.h>
#include "g_local.h"
#include "acebot.h"
#include "myq2_mod.h"
#include "myq2_zombie.h"

extern edict_t *CreateTargetChangeLevel(char *map);

cvar_t *myq2_random;
cvar_t *myq2_bots;
cvar_t *myq2_mappool;
cvar_t *myq2_hpregen;
cvar_t *myq2_ammoregen;
cvar_t *myq2_shuffle;
cvar_t *myq2_mutator;

enum {
	MUT_CLASSIC = 0,
	MUT_LOW_GRAV,
	MUT_HEAVY_LOADOUT,
	MUT_RAIL_ROCKETS,
	MUT_VAMPIRE,
	MUT_ARMOR_SURGE,
	MUT_COUNT
};

static const char *mutator_names[MUT_COUNT] = {
	"Classic Arena",
	"Low Gravity",
	"Heavy Loadout",
	"Rail & Rockets",
	"Vampire (+15 HP/Frag)",
	"Armor Surge"
};

static const char *official_maps[] = {
	"q2dm1", "q2dm2", "q2dm3", "q2dm4",
	"q2dm5", "q2dm6", "q2dm7", "q2dm8"
};
#define NUM_OFFICIAL_MAPS (int)(sizeof(official_maps) / sizeof(official_maps[0]))

static const char *community_maps[] = {
	"aeroq2", "ztn2dm1", "ztn2dm2", "ztn2dm3", "ztn2dm4",
	"ztn2dm5", "q2rdm1", "monastery", "ptrip", "tltf"
};
#define NUM_COMMUNITY_MAPS (int)(sizeof(community_maps) / sizeof(community_maps[0]))

static const char *bot_names[] = {
	"Viper", "Reaper", "Striker", "Phantom",
	"Krag", "Nova", "Cobra", "Sarge",
	"Bitterman", "Grunt", "Slash", "Xero",
	"Raptor", "Blitz", "Maverick", "Tank"
};
#define NUM_BOT_NAMES (int)(sizeof(bot_names) / sizeof(bot_names[0]))

static const char *bot_skins[] = {
	"male/cipher", "male/flak", "male/grunt", "male/howitzer",
	"male/major", "male/nightops", "male/pointman", "male/psycho",
	"male/razor", "male/sniper", "female/athena", "female/brianna",
	"female/cobalt", "female/ensign", "female/jezebel", "female/stiletto"
};
#define NUM_BOT_SKINS (int)(sizeof(bot_skins) / sizeof(bot_skins[0]))

static int active_mutator = MUT_CLASSIC;
static qboolean bots_spawned_this_level = false;
static float next_hp_regen_time = 0.0f;
static float next_ammo_regen_time = 0.0f;
static unsigned int myq2_rng_state = 0x13579BDFu;

static unsigned int MYQ2_Rand(void)
{
	myq2_rng_state = myq2_rng_state * 1664525u + 1013904223u;
	return (myq2_rng_state >> 8);
}

static void MYQ2_SeedRng(void)
{
	unsigned int seed = (unsigned int)time(NULL);
	seed ^= (unsigned int)rand();
	seed ^= (unsigned int)(level.framenum * 2654435761u);
	if (seed == 0)
		seed = 0xCAFEBABEu;
	myq2_rng_state ^= seed;
	(void)MYQ2_Rand();
	(void)MYQ2_Rand();
}

void MYQ2_InitGame(void)
{
	myq2_random    = gi.cvar("myq2_random", "0", CVAR_ARCHIVE);
	myq2_bots      = gi.cvar("myq2_bots", "-1", CVAR_ARCHIVE);
	myq2_mappool   = gi.cvar("myq2_mappool", "0", CVAR_ARCHIVE);
	myq2_hpregen   = gi.cvar("myq2_hpregen", "1", CVAR_ARCHIVE);
	myq2_ammoregen = gi.cvar("myq2_ammoregen", "1", CVAR_ARCHIVE);
	myq2_shuffle   = gi.cvar("myq2_shuffle", "1", CVAR_ARCHIVE);
	myq2_mutator   = gi.cvar("myq2_mutator", "1", CVAR_ARCHIVE);

	MYQ2_SeedRng();
}

const char *MYQ2_GetMutatorName(void)
{
	if (active_mutator < 0 || active_mutator >= MUT_COUNT)
		return mutator_names[0];
	return mutator_names[active_mutator];
}

void MYQ2_OnLevelSpawn(void)
{
	MYQ2_SeedRng();

	bots_spawned_this_level = false;
	next_hp_regen_time = 2.0f;
	next_ammo_regen_time = 3.0f;

	/* Always restore baseline gravity first */
	gi.cvar_set("sv_gravity", "800");
	active_mutator = MUT_CLASSIC;

	if (deathmatch->value && myq2_random->value && myq2_mutator->value)
	{
		active_mutator = (int)(MYQ2_Rand() % MUT_COUNT);
		if (active_mutator == MUT_LOW_GRAV)
		{
			gi.cvar_set("sv_gravity", "450");
		}
	}
}

gitem_t *MYQ2_MaybeShuffleItem(gitem_t *item)
{
	static const char *dm_weapons[] = {
		"Shotgun",
		"Super Shotgun",
		"Machinegun",
		"Chaingun",
		"Grenade Launcher",
		"Rocket Launcher",
		"HyperBlaster",
		"Railgun"
	};
	static const char *dm_armors[] = {
		"Jacket Armor",
		"Combat Armor",
		"Body Armor",
		"Armor Shard"
	};
	static const char *dm_powerups[] = {
		"Quad Damage",
		"Invulnerability",
		"Adrenaline"
	};
	gitem_t *replacement = NULL;

	if (!item || !deathmatch->value || !myq2_shuffle->value)
		return item;
	if (!myq2_random->value && (!myq2_zombie || !myq2_zombie->value))
		return item;

	if (item->flags & IT_WEAPON)
	{
		if (item->pickup_name &&
			Q_stricmp(item->pickup_name, "Blaster") != 0 &&
			Q_stricmp(item->pickup_name, "Grapple") != 0)
		{
			/* 6% chance for BFG10K, otherwise pick evenly among the 8 core DM weapons */
			if ((MYQ2_Rand() % 100) < 6)
				replacement = FindItem("BFG10K");
			else
				replacement = FindItem((char *)dm_weapons[MYQ2_Rand() % 8]);
		}
	}
	else if (item->flags & IT_ARMOR)
	{
		replacement = FindItem((char *)dm_armors[MYQ2_Rand() % 4]);
	}
	else if (item->flags & IT_POWERUP)
	{
		replacement = FindItem((char *)dm_powerups[MYQ2_Rand() % 3]);
	}

	return replacement ? replacement : item;
}

static void MYQ2_GiveItem(edict_t *ent, const char *pickup_name, int count)
{
	gitem_t *it;
	int idx;

	if (!ent || !ent->client)
		return;
	it = FindItem((char *)pickup_name);
	if (!it)
		return;
	idx = ITEM_INDEX(it);
	if (count > 0)
	{
		ent->client->pers.inventory[idx] += count;
	}
	else
	{
		if (ent->client->pers.inventory[idx] < 1)
			ent->client->pers.inventory[idx] = 1;
	}
}

void MYQ2_ApplySpawnLoadout(edict_t *ent)
{
	gitem_t *best = NULL;

	if (!ent || !ent->client || !deathmatch->value || !myq2_random->value)
		return;

	switch (active_mutator)
	{
	case MUT_HEAVY_LOADOUT:
		MYQ2_GiveItem(ent, "Super Shotgun", 0);
		MYQ2_GiveItem(ent, "Shells", 20);
		MYQ2_GiveItem(ent, "Machinegun", 0);
		MYQ2_GiveItem(ent, "Bullets", 60);
		MYQ2_GiveItem(ent, "Rocket Launcher", 0);
		MYQ2_GiveItem(ent, "Rockets", 10);
		MYQ2_GiveItem(ent, "Combat Armor", 50);
		best = FindItem("Super Shotgun");
		break;

	case MUT_RAIL_ROCKETS:
		MYQ2_GiveItem(ent, "Railgun", 0);
		MYQ2_GiveItem(ent, "Slugs", 15);
		MYQ2_GiveItem(ent, "Rocket Launcher", 0);
		MYQ2_GiveItem(ent, "Rockets", 15);
		MYQ2_GiveItem(ent, "Jacket Armor", 25);
		best = FindItem("Railgun");
		break;

	case MUT_ARMOR_SURGE:
		MYQ2_GiveItem(ent, "Shotgun", 0);
		MYQ2_GiveItem(ent, "Shells", 20);
		MYQ2_GiveItem(ent, "Chaingun", 0);
		MYQ2_GiveItem(ent, "Bullets", 80);
		MYQ2_GiveItem(ent, "Combat Armor", 75);
		best = FindItem("Chaingun");
		break;

	default:
		break;
	}

	if (best)
	{
		ent->client->newweapon = best;
		ChangeWeapon(ent);
	}
}

void MYQ2_OnFrag(edict_t *attacker)
{
	if (!attacker || !attacker->client || attacker->health <= 0)
		return;
	if (!deathmatch->value || !myq2_random->value)
		return;

	if (active_mutator == MUT_VAMPIRE)
	{
		attacker->health += 15;
		if (attacker->health > 150)
			attacker->health = 150;
	}
}

void MYQ2_AnnounceMatch(edict_t *ent)
{
	const char *hp_str;
	const char *ammo_str;

	if (!ent || ent->is_bot)
		return;

	if (!myq2_random->value)
	{
		safe_centerprintf(ent,
			"\n======================================\n"
			"MyQ2 (ACE Bot Ready)\n"
			"'sv addbot' to add a bot\n"
			"======================================\n");
		return;
	}

	hp_str   = (myq2_hpregen->value >= 2) ? "MED" : (myq2_hpregen->value >= 1 ? "SLOW" : "OFF");
	ammo_str = (myq2_ammoregen->value >= 2) ? "MED" : (myq2_ammoregen->value >= 1 ? "SLOW" : "OFF");

	safe_centerprintf(ent,
		"\n======================================\n"
		"MYQ2 RANDOM BOT DM\n"
		"Map: %s\n"
		"Mutator: %s\n"
		"Item Shuffle: %s\n"
		"HP Regen: %s  |  Ammo Regen: %s\n"
		"======================================\n",
		level.mapname,
		MYQ2_GetMutatorName(),
		myq2_shuffle->value ? "ON" : "OFF",
		hp_str,
		ammo_str);
}

static void MYQ2_SpawnMatchBots(void)
{
	int target_bots;
	int existing_bots = 0;
	int i, idx;
	int used_names[NUM_BOT_NAMES];
	edict_t *e;

	/* Count any bots already present */
	for (i = 0; i < maxclients->value; i++)
	{
		e = g_edicts + 1 + i;
		if (e->inuse && e->is_bot)
			existing_bots++;
	}

	target_bots = (int)myq2_bots->value;
	if (target_bots < 0)
		target_bots = 3 + (int)(MYQ2_Rand() % 4); /* Random 3..6 */
	if (target_bots > (int)maxclients->value - 1)
		target_bots = (int)maxclients->value - 1;
	if (target_bots > 12)
		target_bots = 12;

	if (existing_bots >= target_bots)
		return;

	memset(used_names, 0, sizeof(used_names));
	for (i = existing_bots; i < target_bots; i++)
	{
		int tries = 0;
		do {
			idx = (int)(MYQ2_Rand() % NUM_BOT_NAMES);
			tries++;
		} while (used_names[idx] && tries < 32);
		used_names[idx] = 1;

		ACESP_SpawnBot(NULL,
			(char *)bot_names[idx],
			(char *)bot_skins[MYQ2_Rand() % NUM_BOT_SKINS],
			NULL);
	}
}

static void MYQ2_RegenAmmoForClient(edict_t *ent, int mode)
{
	gclient_t *cl = ent->client;
	gitem_t *it;
	int cap_mult = (mode >= 2) ? 60 : 40; /* 40% cap in slow, 60% in med */

	struct {
		const char *weapon1;
		const char *weapon2;
		const char *ammo;
		int step;
		int max_ammo;
	} rules[] = {
		{ "Shotgun",          "Super Shotgun",  "Shells",   2, cl->pers.max_shells   },
		{ "Machinegun",       "Chaingun",       "Bullets",  6, cl->pers.max_bullets  },
		{ "Grenade Launcher", "Grenades",       "Grenades", 1, cl->pers.max_grenades },
		{ "Rocket Launcher",  NULL,             "Rockets",  1, cl->pers.max_rockets  },
		{ "HyperBlaster",     "BFG10K",         "Cells",    4, cl->pers.max_cells    },
		{ "Railgun",          NULL,             "Slugs",    1, cl->pers.max_slugs    }
	};
	int i;

	for (i = 0; i < (int)(sizeof(rules) / sizeof(rules[0])); i++)
	{
		qboolean owns = false;
		int cap, cur, idx;

		it = FindItem((char *)rules[i].weapon1);
		if (it && cl->pers.inventory[ITEM_INDEX(it)] > 0)
			owns = true;
		if (!owns && rules[i].weapon2)
		{
			it = FindItem((char *)rules[i].weapon2);
			if (it && cl->pers.inventory[ITEM_INDEX(it)] > 0)
				owns = true;
		}
		if (!owns)
			continue;

		it = FindItem((char *)rules[i].ammo);
		if (!it)
			continue;
		idx = ITEM_INDEX(it);
		cap = (rules[i].max_ammo * cap_mult) / 100;
		if (cap < rules[i].step * 4)
			cap = rules[i].step * 4;
		if (cap > rules[i].max_ammo)
			cap = rules[i].max_ammo;

		cur = cl->pers.inventory[idx];
		if (cur < cap)
		{
			cur += rules[i].step;
			if (cur > cap)
				cur = cap;
			cl->pers.inventory[idx] = cur;
		}
	}
}

void MYQ2_RunFrame(void)
{
	int i;
	edict_t *ent;
	int hp_mode, ammo_mode;

	/* Auto-spawn bots shortly after level start in Random DM (not Zombie mode) */
	if (deathmatch->value && myq2_random->value && (!myq2_zombie || !myq2_zombie->value) && !bots_spawned_this_level && level.time >= 1.2f)
	{
		bots_spawned_this_level = true;
		MYQ2_SpawnMatchBots();
	}

	if (level.intermissiontime)
		return;

	/* Slow HP Regen */
	hp_mode = (int)myq2_hpregen->value;
	if (hp_mode > 0 && level.time >= next_hp_regen_time)
	{
		int gain = (hp_mode >= 2) ? 2 : 1;
		float interval = (hp_mode >= 2) ? 1.0f : 1.5f;
		next_hp_regen_time = level.time + interval;

		for (i = 0; i < maxclients->value; i++)
		{
			ent = g_edicts + 1 + i;
			if (!ent->inuse || !ent->client)
				continue;
			if (ent->deadflag != DEAD_NO || ent->health <= 0)
				continue;
			if (ent->health < ent->client->pers.max_health)
			{
				ent->health += gain;
				if (ent->health > ent->client->pers.max_health)
					ent->health = ent->client->pers.max_health;
			}
		}
	}

	/* Slow Ammo Regen */
	ammo_mode = (int)myq2_ammoregen->value;
	if (ammo_mode > 0 && level.time >= next_ammo_regen_time)
	{
		float interval = (ammo_mode >= 2) ? 1.5f : 3.0f;
		next_ammo_regen_time = level.time + interval;

		for (i = 0; i < maxclients->value; i++)
		{
			ent = g_edicts + 1 + i;
			if (!ent->inuse || !ent->client)
				continue;
			if (ent->deadflag != DEAD_NO || ent->health <= 0)
				continue;
			MYQ2_RegenAmmoForClient(ent, ammo_mode);
		}
	}
}

qboolean MYQ2_EndDMLevel(void)
{
	const char *pool[NUM_OFFICIAL_MAPS + NUM_COMMUNITY_MAPS];
	int count = 0;
	int i, pick;
	int pool_mode;

	if (!myq2_random->value)
		return false;

	pool_mode = (int)myq2_mappool->value;
	if (pool_mode == 0 || pool_mode == 1)
	{
		for (i = 0; i < NUM_OFFICIAL_MAPS; i++)
			pool[count++] = official_maps[i];
	}
	if (pool_mode == 0 || pool_mode == 2)
	{
		for (i = 0; i < NUM_COMMUNITY_MAPS; i++)
			pool[count++] = community_maps[i];
	}
	if (count <= 0)
		return false;

	pick = (int)(MYQ2_Rand() % count);
	if (count > 1 && Q_stricmp(pool[pick], level.mapname) == 0)
		pick = (pick + 1) % count;

	BeginIntermission(CreateTargetChangeLevel((char *)pool[pick]));
	return true;
}
