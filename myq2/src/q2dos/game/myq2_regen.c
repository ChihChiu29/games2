/*
 * myq2_regen.c - Slow HP and Ammo Regen for Classic Quake 2 (baseq2)
 */

#include "g_local.h"

cvar_t *myq2_hpregen;
cvar_t *myq2_ammoregen;

static float classic_next_hp_time = 0.0f;
static float classic_next_ammo_time = 0.0f;

void MYQ2_ClassicInit(void)
{
	myq2_hpregen   = gi.cvar("myq2_hpregen", "1", CVAR_ARCHIVE);
	myq2_ammoregen = gi.cvar("myq2_ammoregen", "1", CVAR_ARCHIVE);
	classic_next_hp_time = 0.0f;
	classic_next_ammo_time = 0.0f;
}

static void MYQ2_ClassicRegenAmmo(edict_t *ent, int mode)
{
	gclient_t *cl = ent->client;
	gitem_t *it;
	int cap_mult = (mode >= 2) ? 60 : 40;
	int i;

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

void MYQ2_ClassicRunFrame(void)
{
	int i;
	edict_t *ent;
	int hp_mode, ammo_mode;

	if (level.intermissiontime)
		return;

	if (level.time < 1.0f)
	{
		classic_next_hp_time = 2.0f;
		classic_next_ammo_time = 3.0f;
		return;
	}

	hp_mode = (int)myq2_hpregen->value;
	if (hp_mode > 0 && level.time >= classic_next_hp_time)
	{
		int gain = (hp_mode >= 2) ? 2 : 1;
		float interval = (hp_mode >= 2) ? 1.0f : 1.5f;
		classic_next_hp_time = level.time + interval;

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

	ammo_mode = (int)myq2_ammoregen->value;
	if (ammo_mode > 0 && level.time >= classic_next_ammo_time)
	{
		float interval = (ammo_mode >= 2) ? 1.5f : 3.0f;
		classic_next_ammo_time = level.time + interval;

		for (i = 0; i < maxclients->value; i++)
		{
			ent = g_edicts + 1 + i;
			if (!ent->inuse || !ent->client)
				continue;
			if (ent->deadflag != DEAD_NO || ent->health <= 0)
				continue;
			MYQ2_ClassicRegenAmmo(ent, ammo_mode);
		}
	}
}
