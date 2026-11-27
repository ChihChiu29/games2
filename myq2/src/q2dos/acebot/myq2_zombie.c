/*
 * myq2_zombie.c - CoD-style Zombie / Wave Survival Roguelite Mode for MyQ2
 */

#include <time.h>
#include "g_local.h"
#include "myq2_mod.h"
#include "myq2_zombie.h"

cvar_t *myq2_zombie;
cvar_t *myq2_zrounds;
cvar_t *myq2_zdiff;

extern void SP_monster_berserk(edict_t *self);
extern void SP_monster_mutant(edict_t *self);
extern void SP_monster_soldier_light(edict_t *self);
extern void SP_monster_soldier(edict_t *self);
extern void SP_monster_soldier_ss(edict_t *self);
extern void SP_monster_infantry(edict_t *self);
extern void SP_monster_parasite(edict_t *self);
extern void SP_monster_gunner(edict_t *self);
extern void SP_monster_gladiator(edict_t *self);
extern void SP_monster_flyer(edict_t *self);
extern void SP_monster_tank(edict_t *self);
extern void FoundTarget(edict_t *self);
extern qboolean visible(edict_t *self, edict_t *other);

enum {
	ZM_STATE_IDLE = 0,
	ZM_STATE_PRE_ROUND,
	ZM_STATE_ACTIVE,
	ZM_STATE_UPGRADE,
	ZM_STATE_VICTORY,
	ZM_STATE_GAMEOVER
};

enum {
	UPG_LIFESTEAL = 0,
	UPG_DAMAGE,
	UPG_JUGGERNAUT,
	UPG_ARMOR,
	UPG_KILLHEAL,
	UPG_REGEN,
	UPG_WEAP_HEAVY,
	UPG_WEAP_EXPLOSIVE,
	UPG_WEAP_ENERGY,
	UPG_QUAD_SURGE,
	UPG_COUNT
};

typedef struct {
	const char *title;
	const char *desc;
} zm_upgrade_t;

static const zm_upgrade_t zm_upgrades[UPG_COUNT] = {
	{ "Vampiric Rounds",   "+5 pct Heal per Damage"      },
	{ "Heavy Caliber",     "+20 pct Weapon Damage"       },
	{ "Juggernaut",        "+20 Max HP & +50 Heal"       },
	{ "Nano-Plating",      "+40 Armor & -8 pct Dmg Taken"},
	{ "Bloodthirst",       "+8 HP per Kill"              },
	{ "Regen Overdrive",   "Faster Regen & +25 HP"       },
	{ "Arsenal: Heavy",    "SSG + Chaingun (or +10 dmg)" },
	{ "Arsenal: Explosive","Rocket + Grenade (or +10)"   },
	{ "Arsenal: Energy",   "Railgun + HyperB (or +10)"   },
	{ "Quad Surge",        "Round-Start Quad (+8s/+4s)"  }
};

static const char *zm_diff_names[3] = {
	"Normal", "Hard", "Nightmare"
};

#define ZM_MAX_TRACKED 24

typedef struct {
	edict_t *ent;
	vec3_t   last_pos;
	float    last_move_time;
} zm_mon_track_t;

static int      zm_state = ZM_STATE_IDLE;
static int      zm_round = 0;
static int      zm_to_spawn = 0;
static int      zm_alive = 0;
static int      zm_kills_total = 0;
static float    zm_state_timer = 0.0f;
static float    zm_next_spawn_time = 0.0f;
static float    zm_next_hud_time = 0.0f;
static float    zm_next_watchdog_time = 0.0f;
static float    zm_menu_reopen_time = 0.0f;

/* Roguelite RPG stats (reset on new run / death) */
static int      zm_dmg_pct = 0;
static int      zm_lifesteal_pct = 5;
static int      zm_kill_heal = 0;
static int      zm_armor_dr_pct = 0;
static int      zm_bonus_maxhp = 0;
static int      zm_quad_secs = 0;
static float    zm_lifesteal_acc = 0.0f;

static int      zm_offered[3] = { 0, 1, 2 };
static char     zm_menu_lines[3][48];
static char     zm_menu_descs[3][48];
static char     zm_menu_header[48];
static char     zm_menu_stats[48];

static zm_mon_track_t zm_tracked[ZM_MAX_TRACKED];
static unsigned int zm_rng = 0x5A0B1E42u;

/* Optional automated multi-round test driver (off unless `zm_autotest` called) */
static int      zm_autotest_rounds = 0;
static float    zm_autotest_next = 0.0f;

static int ZM_GetDiff(void)
{
	int d = myq2_zdiff ? (int)myq2_zdiff->value : 0;
	if (d < 0) d = 0;
	if (d > 2) d = 2;
	return d;
}

static unsigned int ZM_Rand(void)
{
	zm_rng = zm_rng * 1664525u + 1013904223u;
	return (zm_rng >> 8);
}

static edict_t *ZM_GetPlayer(void)
{
	int i;
	edict_t *e;
	for (i = 0; i < maxclients->value; i++)
	{
		e = g_edicts + 1 + i;
		if (e->inuse && e->client && !e->is_bot)
			return e;
	}
	return NULL;
}

static void ZM_ResetRunStats(void)
{
	int d = ZM_GetDiff();
	zm_round = 0;
	zm_to_spawn = 0;
	zm_alive = 0;
	zm_kills_total = 0;
	zm_dmg_pct = 0;
	/* Base Heal-per-Damage scales with difficulty: Normal 5%, Hard 4%, Nightmare 3% */
	zm_lifesteal_pct = (d == 0) ? 5 : (d == 1 ? 4 : 3);
	zm_kill_heal = 0;
	zm_armor_dr_pct = 0;
	zm_bonus_maxhp = 0;
	zm_quad_secs = 0;
	zm_lifesteal_acc = 0.0f;
	zm_autotest_rounds = 0;
	memset(zm_tracked, 0, sizeof(zm_tracked));
}

void MYQ2_ZombieInit(void)
{
	myq2_zombie  = gi.cvar("myq2_zombie", "0", CVAR_ARCHIVE);
	myq2_zrounds = gi.cvar("myq2_zrounds", "10", CVAR_ARCHIVE);
	myq2_zdiff   = gi.cvar("myq2_zdiff", "0", CVAR_ARCHIVE);
	zm_rng ^= (unsigned int)time(NULL);
}

void MYQ2_ZombieOnLevelSpawn(void)
{
	zm_rng ^= (unsigned int)time(NULL) ^ (unsigned int)rand();
	ZM_ResetRunStats();
	if (myq2_zombie && myq2_zombie->value)
	{
		zm_state = ZM_STATE_PRE_ROUND;
		zm_state_timer = 2.5f;
	}
	else
	{
		zm_state = ZM_STATE_IDLE;
	}
}

static qboolean ZM_HasItem(edict_t *ent, const char *name)
{
	gitem_t *it = FindItem((char *)name);
	if (!ent || !ent->client || !it)
		return false;
	return ent->client->pers.inventory[ITEM_INDEX(it)] > 0;
}

static void ZM_GiveItem(edict_t *ent, const char *name, int count)
{
	gitem_t *it;
	int idx;

	if (!ent || !ent->client)
		return;
	it = FindItem((char *)name);
	if (!it)
		return;
	idx = ITEM_INDEX(it);
	if (count > 0)
		ent->client->pers.inventory[idx] += count;
	else if (ent->client->pers.inventory[idx] < 1)
		ent->client->pers.inventory[idx] = 1;
}

void MYQ2_ZombieOnPlayerSpawn(edict_t *ent)
{
	gitem_t *sg;
	int d;

	if (!myq2_zombie || !myq2_zombie->value)
		return;
	if (!ent || !ent->client || ent->is_bot)
		return;

	if (zm_state == ZM_STATE_GAMEOVER || zm_state == ZM_STATE_VICTORY)
	{
		int i;
		for (i = 0; i < ZM_MAX_TRACKED; i++)
		{
			if (zm_tracked[i].ent && zm_tracked[i].ent->inuse)
				G_FreeEdict(zm_tracked[i].ent);
			zm_tracked[i].ent = NULL;
		}
		ZM_ResetRunStats();
		zm_state = ZM_STATE_PRE_ROUND;
		zm_state_timer = level.time + 2.5f;
	}

	ent->client->pers.max_health = 100 + zm_bonus_maxhp;
	ent->health = ent->client->pers.max_health;

	ZM_GiveItem(ent, "Shotgun", 0);
	ZM_GiveItem(ent, "Shells", 24);
	ZM_GiveItem(ent, "Jacket Armor", 25);

	sg = FindItem("Shotgun");
	if (sg)
	{
		ent->client->newweapon = sg;
		ChangeWeapon(ent);
	}

	d = ZM_GetDiff();
	safe_centerprintf(ent,
		"\n======================================\n"
		"MYQ2 ZOMBIE SURVIVAL (ROGUELITE)\n"
		"Map: %s  |  Diff: %s\n"
		"Target: %s  |  Heal/Dmg: %d pct\n"
		"Survive waves & pick RPG upgrades!\n"
		"======================================\n",
		level.mapname,
		zm_diff_names[d],
		(myq2_zrounds->value <= 0) ? "ENDLESS" : va("%d Rounds", (int)myq2_zrounds->value),
		zm_lifesteal_pct);
}

void MYQ2_ZombieOnPlayerDeath(edict_t *ent)
{
	if (!myq2_zombie || !myq2_zombie->value)
		return;
	if (!ent || ent->is_bot)
		return;

	if (ent->client && ent->client->menu)
		PMenu_Close(ent);

	zm_state = ZM_STATE_GAMEOVER;
	safe_centerprintf(ent,
		"\n======================================\n"
		"YOU DIED ON ROUND %d!\n"
		"Total Kills: %d\n"
		"Press FIRE to restart Round 1\n"
		"or press [Z] for a new random arena!\n"
		"======================================\n",
		zm_round, zm_kills_total);
}

static void ZM_ApplyUpgrade(edict_t *ent, int upg_id)
{
	if (!ent || !ent->client)
		return;

	switch (upg_id)
	{
	case UPG_LIFESTEAL:
		zm_lifesteal_pct += 5;
		if (zm_lifesteal_pct > 25)
			zm_lifesteal_pct = 25;
		safe_bprintf(PRINT_HIGH, "UPGRADE: Vampiric Rounds! Heal/Dmg is now %d pct!\n", zm_lifesteal_pct);
		break;

	case UPG_DAMAGE:
		zm_dmg_pct += 20;
		safe_bprintf(PRINT_HIGH, "UPGRADE: Heavy Caliber! Bonus Weapon Damage is now +%d pct!\n", zm_dmg_pct);
		break;

	case UPG_JUGGERNAUT:
		zm_bonus_maxhp += 20;
		ent->client->pers.max_health = 100 + zm_bonus_maxhp;
		ent->health += 50;
		if (ent->health > ent->client->pers.max_health)
			ent->health = ent->client->pers.max_health;
		safe_bprintf(PRINT_HIGH, "UPGRADE: Juggernaut! Max HP=%d (HP=%d)!\n", ent->client->pers.max_health, ent->health);
		break;

	case UPG_ARMOR:
		zm_armor_dr_pct += 8;
		if (zm_armor_dr_pct > 35)
			zm_armor_dr_pct = 35;
		ZM_GiveItem(ent, "Combat Armor", 40);
		safe_bprintf(PRINT_HIGH, "UPGRADE: Nano-Plating! +40 Armor & -%d pct Dmg Taken!\n", zm_armor_dr_pct);
		break;

	case UPG_KILLHEAL:
		zm_kill_heal += 8;
		safe_bprintf(PRINT_HIGH, "UPGRADE: Bloodthirst! +%d HP on every kill!\n", zm_kill_heal);
		break;

	case UPG_REGEN:
		gi.cvar_set("myq2_hpregen", "2");
		gi.cvar_set("myq2_ammoregen", "2");
		ent->health += 25;
		if (ent->health > ent->client->pers.max_health)
			ent->health = ent->client->pers.max_health;
		safe_bprintf(PRINT_HIGH, "UPGRADE: Regen Overdrive! HP & Ammo Regen boosted +25 HP!\n");
		break;

	case UPG_WEAP_HEAVY:
		if (ZM_HasItem(ent, "Super Shotgun") && ZM_HasItem(ent, "Chaingun"))
			zm_dmg_pct += 10;
		ZM_GiveItem(ent, "Super Shotgun", 0);
		ZM_GiveItem(ent, "Shells", 24);
		ZM_GiveItem(ent, "Chaingun", 0);
		ZM_GiveItem(ent, "Bullets", 80);
		safe_bprintf(PRINT_HIGH, "UPGRADE: Arsenal Heavy! SSG + Chaingun + Ammo (DMG +%d pct)!\n", zm_dmg_pct);
		break;

	case UPG_WEAP_EXPLOSIVE:
		if (ZM_HasItem(ent, "Rocket Launcher") && ZM_HasItem(ent, "Grenade Launcher"))
			zm_dmg_pct += 10;
		ZM_GiveItem(ent, "Rocket Launcher", 0);
		ZM_GiveItem(ent, "Rockets", 15);
		ZM_GiveItem(ent, "Grenade Launcher", 0);
		ZM_GiveItem(ent, "Grenades", 12);
		safe_bprintf(PRINT_HIGH, "UPGRADE: Arsenal Explosive! Rockets + Grenades (DMG +%d pct)!\n", zm_dmg_pct);
		break;

	case UPG_WEAP_ENERGY:
		if (ZM_HasItem(ent, "Railgun") && ZM_HasItem(ent, "HyperBlaster"))
			zm_dmg_pct += 10;
		ZM_GiveItem(ent, "Railgun", 0);
		ZM_GiveItem(ent, "Slugs", 15);
		ZM_GiveItem(ent, "HyperBlaster", 0);
		ZM_GiveItem(ent, "Cells", 60);
		safe_bprintf(PRINT_HIGH, "UPGRADE: Arsenal Energy! Railgun + HyperBlaster (DMG +%d pct)!\n", zm_dmg_pct);
		break;

	case UPG_QUAD_SURGE:
		if (zm_quad_secs == 0)
			zm_quad_secs = 8;
		else
			zm_quad_secs += 4;
		if (zm_quad_secs > 16)
			zm_quad_secs = 16;
		safe_bprintf(PRINT_HIGH, "UPGRADE: Quad Surge! %ds Quad at every round start!\n", zm_quad_secs);
		break;
	}

	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/pkup.wav"), 1, ATTN_NORM, 0);

	if (ent->client->menu)
		PMenu_Close(ent);

	zm_state = ZM_STATE_PRE_ROUND;
	zm_state_timer = level.time + 2.5f;
}

static void ZM_PickChoice0(edict_t *ent, pmenu_t *p) { (void)p; ZM_ApplyUpgrade(ent, zm_offered[0]); }
static void ZM_PickChoice1(edict_t *ent, pmenu_t *p) { (void)p; ZM_ApplyUpgrade(ent, zm_offered[1]); }
static void ZM_PickChoice2(edict_t *ent, pmenu_t *p) { (void)p; ZM_ApplyUpgrade(ent, zm_offered[2]); }

static pmenu_t zm_upg_menu[] = {
	{ "*MYQ2 ZOMBIE SURVIVAL",      PMENU_ALIGN_CENTER, NULL, NULL },
	{ zm_menu_header,               PMENU_ALIGN_CENTER, NULL, NULL },
	{ NULL,                         PMENU_ALIGN_CENTER, NULL, NULL },
	{ zm_menu_lines[0],             PMENU_ALIGN_LEFT,   NULL, ZM_PickChoice0 },
	{ zm_menu_descs[0],             PMENU_ALIGN_LEFT,   NULL, NULL },
	{ NULL,                         PMENU_ALIGN_LEFT,   NULL, NULL },
	{ zm_menu_lines[1],             PMENU_ALIGN_LEFT,   NULL, ZM_PickChoice1 },
	{ zm_menu_descs[1],             PMENU_ALIGN_LEFT,   NULL, NULL },
	{ NULL,                         PMENU_ALIGN_LEFT,   NULL, NULL },
	{ zm_menu_lines[2],             PMENU_ALIGN_LEFT,   NULL, ZM_PickChoice2 },
	{ zm_menu_descs[2],             PMENU_ALIGN_LEFT,   NULL, NULL },
	{ NULL,                         PMENU_ALIGN_LEFT,   NULL, NULL },
	{ zm_menu_stats,                PMENU_ALIGN_CENTER, NULL, NULL },
	{ "Use [ / ] + Enter or F1/F2/F3", PMENU_ALIGN_CENTER, NULL, NULL }
};

static void ZM_OpenUpgradeMenu(edict_t *player, qboolean reroll)
{
	int used[UPG_COUNT];
	int i, pick;

	if (reroll)
	{
		memset(used, 0, sizeof(used));
		for (i = 0; i < 3; i++)
		{
			do {
				pick = (int)(ZM_Rand() % UPG_COUNT);
			} while (used[pick]);
			used[pick] = 1;
			zm_offered[i] = pick;
			Com_sprintf(zm_menu_lines[i], sizeof(zm_menu_lines[i]), "*[%d] %s", i + 1, zm_upgrades[pick].title);
			Com_sprintf(zm_menu_descs[i], sizeof(zm_menu_descs[i]), "    %s", zm_upgrades[pick].desc);
		}
		safe_bprintf(PRINT_HIGH, "=== ROUND %d CLEAR! UPGRADE CHOICES ===\n", zm_round);
		for (i = 0; i < 3; i++)
			safe_bprintf(PRINT_HIGH, "  [%d] %s: %s\n", i + 1, zm_upgrades[zm_offered[i]].title, zm_upgrades[zm_offered[i]].desc);
	}

	Com_sprintf(zm_menu_header, sizeof(zm_menu_header), "ROUND %d CLEAR! CHOOSE 1 OF 3", zm_round);
	Com_sprintf(zm_menu_stats, sizeof(zm_menu_stats), "DMG:+%d  HEAL/DMG:%d  HP:%d",
		zm_dmg_pct, zm_lifesteal_pct, player->client->pers.max_health);

	PMenu_Open(player, zm_upg_menu, 3, sizeof(zm_upg_menu) / sizeof(pmenu_t));
	zm_menu_reopen_time = level.time + 3.0f;
}

qboolean MYQ2_ZombieClientCommand(edict_t *ent, const char *cmd)
{
	int idx;
	if (!myq2_zombie || !myq2_zombie->value)
		return false;
	if (Q_stricmp((char *)cmd, "zm_pick") == 0)
	{
		if (zm_state != ZM_STATE_UPGRADE)
			return true;
		idx = atoi(gi.argv(1)) - 1;
		if (idx >= 0 && idx < 3)
			ZM_ApplyUpgrade(ent, zm_offered[idx]);
		return true;
	}
	if (Q_stricmp((char *)cmd, "zm_menu") == 0)
	{
		if (zm_state == ZM_STATE_UPGRADE)
			ZM_OpenUpgradeMenu(ent, false);
		return true;
	}
	if (Q_stricmp((char *)cmd, "zm_autotest") == 0)
	{
		zm_autotest_rounds = atoi(gi.argv(1));
		if (zm_autotest_rounds <= 0)
			zm_autotest_rounds = 3;
		zm_autotest_next = level.time + 1.0f;
		safe_bprintf(PRINT_HIGH, "ZM_AUTOTEST: enabled for %d rounds\n", zm_autotest_rounds);
		return true;
	}
	return false;
}

static edict_t *ZM_PickSpawnSpot(edict_t *player)
{
	edict_t *spot = NULL;
	edict_t *candidates[32];
	int count = 0;
	int i;
	edict_t *best = NULL;
	float best_score = -1e9f;

	while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL && count < 32)
		candidates[count++] = spot;

	if (count == 0)
		return NULL;

	for (i = 0; i < count; i++)
	{
		vec3_t diff;
		float dist, score;
		edict_t *c = candidates[i];

		if (!player)
			return candidates[ZM_Rand() % count];

		VectorSubtract(c->s.origin, player->s.origin, diff);
		dist = VectorLength(diff);

		if (dist < 220.0f)
			score = -1000.0f + dist;
		else if (dist <= 1100.0f)
			score = 500.0f + (float)(ZM_Rand() % 300);
		else
			score = 300.0f - (dist - 1100.0f) * 0.25f + (float)(ZM_Rand() % 200);

		if (score > best_score)
		{
			best_score = score;
			best = c;
		}
	}
	return best ? best : candidates[ZM_Rand() % count];
}

static void ZM_SpawnOneMonster(edict_t *player, qboolean is_boss)
{
	edict_t *spot;
	edict_t *m;
	float saved_dm;
	int roll, i, d, hp_scale;
	int slot = -1;

	for (i = 0; i < ZM_MAX_TRACKED; i++)
	{
		if (!zm_tracked[i].ent || !zm_tracked[i].ent->inuse)
		{
			slot = i;
			break;
		}
	}
	if (slot < 0)
		return;

	spot = ZM_PickSpawnSpot(player);
	if (!spot)
		return;

	m = G_Spawn();
	VectorCopy(spot->s.origin, m->s.origin);
	m->s.origin[2] += 16;
	VectorCopy(spot->s.angles, m->s.angles);

	saved_dm = deathmatch->value;
	deathmatch->value = 0;

	if (is_boss)
	{
		m->classname = "monster_tank_commander";
		SP_monster_tank(m);
	}
	else if (zm_round <= 2)
	{
		roll = (int)(ZM_Rand() % 3);
		if (roll == 0)      { m->classname = "monster_berserk";       SP_monster_berserk(m); }
		else if (roll == 1) { m->classname = "monster_soldier_light"; SP_monster_soldier_light(m); }
		else                { m->classname = "monster_mutant";        SP_monster_mutant(m); }
	}
	else if (zm_round <= 4)
	{
		roll = (int)(ZM_Rand() % 5);
		if (roll == 0)      { m->classname = "monster_berserk";    SP_monster_berserk(m); }
		else if (roll == 1) { m->classname = "monster_mutant";     SP_monster_mutant(m); }
		else if (roll == 2) { m->classname = "monster_soldier";    SP_monster_soldier(m); }
		else if (roll == 3) { m->classname = "monster_infantry";   SP_monster_infantry(m); }
		else                { m->classname = "monster_parasite";   SP_monster_parasite(m); }
	}
	else
	{
		roll = (int)(ZM_Rand() % 6);
		if (roll == 0)      { m->classname = "monster_berserk";    SP_monster_berserk(m); }
		else if (roll == 1) { m->classname = "monster_mutant";     SP_monster_mutant(m); }
		else if (roll == 2) { m->classname = "monster_soldier_ss"; SP_monster_soldier_ss(m); }
		else if (roll == 3) { m->classname = "monster_gunner";     SP_monster_gunner(m); }
		else if (roll == 4) { m->classname = "monster_gladiator";  SP_monster_gladiator(m); }
		else                { m->classname = "monster_flyer";      SP_monster_flyer(m); }
	}

	deathmatch->value = saved_dm;

	if (!m->inuse)
		return;

	/* Difficulty & per-round HP scaling */
	d = ZM_GetDiff();
	if (d == 0)      hp_scale = 100 + (zm_round > 1 ? (zm_round - 1) * 12 : 0);
	else if (d == 1) hp_scale = 125 + (zm_round > 1 ? (zm_round - 1) * 18 : 0);
	else             hp_scale = 160 + (zm_round > 1 ? (zm_round - 1) * 25 : 0);

	m->health = (m->health * hp_scale) / 100;
	m->max_health = m->health;

	m->s.event = EV_PLAYER_TELEPORT;
	gi.linkentity(m);

	if (player && player->health > 0)
	{
		m->enemy = player;
		FoundTarget(m);
	}

	zm_tracked[slot].ent = m;
	VectorCopy(m->s.origin, zm_tracked[slot].last_pos);
	zm_tracked[slot].last_move_time = level.time;
	zm_alive++;
}

int MYQ2_ZombieModifyDamage(edict_t *targ, edict_t *attacker, int damage)
{
	int d;

	if (!myq2_zombie || !myq2_zombie->value)
		return damage;

	if (attacker && attacker->client && !attacker->is_bot && (targ->svflags & SVF_MONSTER))
	{
		if (zm_dmg_pct > 0)
			damage = (damage * (100 + zm_dmg_pct)) / 100;
	}
	else if (targ && targ->client && !targ->is_bot && (attacker && (attacker->svflags & SVF_MONSTER)))
	{
		d = ZM_GetDiff();
		if (d == 1)
			damage = (damage * 125) / 100;
		else if (d == 2)
			damage = (damage * 150) / 100;

		if (zm_armor_dr_pct > 0)
		{
			damage = (damage * (100 - zm_armor_dr_pct)) / 100;
			if (damage < 1)
				damage = 1;
		}
	}

	return damage;
}

void MYQ2_ZombieOnDamageDealt(edict_t *targ, edict_t *attacker, int take)
{
	if (!myq2_zombie || !myq2_zombie->value)
		return;
	if (!attacker || !attacker->client || attacker->is_bot || attacker->health <= 0)
		return;
	if (!targ || !(targ->svflags & SVF_MONSTER) || take <= 0)
		return;

	if (zm_lifesteal_pct > 0)
	{
		zm_lifesteal_acc += ((float)take * (float)zm_lifesteal_pct) / 100.0f;
		if (zm_lifesteal_acc >= 1.0f)
		{
			int heal = (int)zm_lifesteal_acc;
			zm_lifesteal_acc -= (float)heal;
			if (attacker->health < attacker->client->pers.max_health)
			{
				attacker->health += heal;
				if (attacker->health > attacker->client->pers.max_health)
					attacker->health = attacker->client->pers.max_health;
			}
		}
	}
}

void MYQ2_ZombieOnMonsterKilled(edict_t *targ, edict_t *attacker)
{
	int i;

	if (!myq2_zombie || !myq2_zombie->value)
		return;

	for (i = 0; i < ZM_MAX_TRACKED; i++)
	{
		if (zm_tracked[i].ent == targ)
		{
			zm_tracked[i].ent = NULL;
			if (zm_alive > 0)
				zm_alive--;
			zm_kills_total++;
			break;
		}
	}

	if (attacker && attacker->client && !attacker->is_bot)
	{
		attacker->client->resp.score++;
		if (zm_kill_heal > 0 && attacker->health > 0 && attacker->health < attacker->client->pers.max_health)
		{
			attacker->health += zm_kill_heal;
			if (attacker->health > attacker->client->pers.max_health)
				attacker->health = attacker->client->pers.max_health;
		}
	}
}

static void ZM_StartNextRound(edict_t *player)
{
	qboolean boss_round;
	int d = ZM_GetDiff();

	zm_round++;
	zm_to_spawn = 4 + zm_round * 2 + d;
	if (zm_to_spawn > 32)
		zm_to_spawn = 32;
	zm_alive = 0;
	zm_state = ZM_STATE_ACTIVE;
	zm_next_spawn_time = level.time + 0.4f;
	zm_next_hud_time = level.time + 5.0f;
	zm_next_watchdog_time = level.time + 6.0f;

	boss_round = ((zm_round % 5) == 0) || (d == 2 && (zm_round % 3) == 0);

	if (player && player->client && zm_quad_secs > 0)
	{
		player->client->quad_framenum = level.framenum + zm_quad_secs * 10;
		gi.sound(player, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
	}

	if (player)
	{
		safe_centerprintf(player,
			"\n======================================\n"
			"%sROUND %d (%s)%s\n"
			"Enemies Incoming: %d\n"
			"======================================\n",
			boss_round ? "*** BOSS " : "",
			zm_round,
			zm_diff_names[d],
			boss_round ? " ***" : "",
			zm_to_spawn + (boss_round ? 1 : 0));
	}

	if (boss_round)
		ZM_SpawnOneMonster(player, true);
}

static void ZM_Watchdog(edict_t *player)
{
	int i;
	int actual_alive = 0;

	for (i = 0; i < ZM_MAX_TRACKED; i++)
	{
		edict_t *m = zm_tracked[i].ent;
		vec3_t diff;
		float moved;

		if (!m)
			continue;
		if (!m->inuse || m->health <= 0 || m->deadflag != DEAD_NO)
		{
			zm_tracked[i].ent = NULL;
			continue;
		}

		actual_alive++;

		if (player && player->health > 0 && m->enemy != player)
		{
			m->enemy = player;
			FoundTarget(m);
		}

		VectorSubtract(m->s.origin, zm_tracked[i].last_pos, diff);
		moved = VectorLength(diff);
		if (moved > 40.0f)
		{
			VectorCopy(m->s.origin, zm_tracked[i].last_pos);
			zm_tracked[i].last_move_time = level.time;
		}
		else if (level.time - zm_tracked[i].last_move_time > 10.0f)
		{
			if (player && !visible(m, player))
			{
				edict_t *spot = ZM_PickSpawnSpot(player);
				if (spot)
				{
					VectorCopy(spot->s.origin, m->s.origin);
					m->s.origin[2] += 16;
					m->s.event = EV_PLAYER_TELEPORT;
					gi.linkentity(m);
					VectorCopy(m->s.origin, zm_tracked[i].last_pos);
					zm_tracked[i].last_move_time = level.time;
				}
			}
		}
	}

	zm_alive = actual_alive;
}

static void ZM_RunAutoTest(edict_t *player)
{
	int i;
	if (zm_autotest_rounds <= 0 || level.time < zm_autotest_next)
		return;

	zm_autotest_next = level.time + 0.4f;

	if (zm_state == ZM_STATE_ACTIVE && zm_alive > 0)
	{
		/* Hurt player slightly once per round to verify Heal-per-Damage visibly works */
		if (player->health > 60)
			player->health = 60;

		for (i = 0; i < ZM_MAX_TRACKED; i++)
		{
			edict_t *m = zm_tracked[i].ent;
			if (m && m->inuse && m->health > 0)
			{
				int hp_before = player->health;
				vec3_t dir = { 1, 0, 0 };
				T_Damage(m, player, player, dir, m->s.origin, vec3_origin, 600, 0, 0, MOD_RAILGUN);
				if (player->health > hp_before)
					safe_bprintf(PRINT_HIGH, "ZM_AUTOTEST: Lifesteal healed player %d -> %d HP!\n", hp_before, player->health);
				break;
			}
		}
	}
	else if (zm_state == ZM_STATE_UPGRADE)
	{
		int pick = (zm_round - 1) % 3;
		safe_bprintf(PRINT_HIGH, "ZM_AUTOTEST: Picking upgrade [%d] (%s) after Round %d\n",
			pick + 1, zm_upgrades[zm_offered[pick]].title, zm_round);
		ZM_ApplyUpgrade(player, zm_offered[pick]);
		if (zm_round >= zm_autotest_rounds)
		{
			safe_bprintf(PRINT_HIGH, "ZM_AUTOTEST: Completed %d rounds cleanly! (Kills=%d, DMG=+%d, Heal/Dmg=%d, MaxHP=%d)\n",
				zm_round, zm_kills_total, zm_dmg_pct, zm_lifesteal_pct, player->client->pers.max_health);
			zm_autotest_rounds = 0;
		}
	}
}

void MYQ2_ZombieRunFrame(void)
{
	edict_t *player;
	int target_rounds, d, max_alive;
	float spawn_delay;

	if (!myq2_zombie || !myq2_zombie->value)
		return;

	player = ZM_GetPlayer();
	if (!player)
		return;

	d = ZM_GetDiff();
	max_alive = (d == 0) ? 7 : (d == 1 ? 9 : 11);
	spawn_delay = (d == 0) ? 1.2f : (d == 1 ? 0.9f : 0.65f);

	if (zm_state == ZM_STATE_PRE_ROUND)
	{
		if (level.time >= zm_state_timer && player->health > 0)
			ZM_StartNextRound(player);
	}
	else if (zm_state == ZM_STATE_ACTIVE)
	{
		if (zm_to_spawn > 0 && zm_alive < max_alive && level.time >= zm_next_spawn_time)
		{
			ZM_SpawnOneMonster(player, false);
			zm_to_spawn--;
			zm_next_spawn_time = level.time + spawn_delay;
		}

		if (level.time >= zm_next_watchdog_time)
		{
			zm_next_watchdog_time = level.time + 4.0f;
			ZM_Watchdog(player);
		}

		if (level.time >= zm_next_hud_time)
		{
			zm_next_hud_time = level.time + 8.0f;
			safe_cprintf(player, PRINT_HIGH, "[Round %d] Enemies remaining: %d  (Kills: %d)\n",
				zm_round, zm_alive + zm_to_spawn, zm_kills_total);
		}

		if (zm_to_spawn <= 0 && zm_alive <= 0)
		{
			target_rounds = (int)myq2_zrounds->value;
			if (target_rounds > 0 && zm_round >= target_rounds)
			{
				zm_state = ZM_STATE_VICTORY;
				safe_centerprintf(player,
					"\n======================================\n"
					"VICTORY! SURVIVED ALL %d ROUNDS!\n"
					"Difficulty: %s  |  Kills: %d\n"
					"Press [Z] to start a new arena run!\n"
					"======================================\n",
					zm_round, zm_diff_names[d], zm_kills_total);
			}
			else
			{
				if (player->health > 0)
				{
					player->health += 20;
					if (player->health > player->client->pers.max_health)
						player->health = player->client->pers.max_health;
				}
				zm_state = ZM_STATE_UPGRADE;
				ZM_OpenUpgradeMenu(player, true);
			}
		}
	}
	else if (zm_state == ZM_STATE_UPGRADE)
	{
		/* Never softlock if player accidentally dismissed the upgrade menu */
		if (!player->client->menu && level.time >= zm_menu_reopen_time)
			ZM_OpenUpgradeMenu(player, false);
	}

	ZM_RunAutoTest(player);
}
