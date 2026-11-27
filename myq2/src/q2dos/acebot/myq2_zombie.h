#ifndef MYQ2_ZOMBIE_H
#define MYQ2_ZOMBIE_H

extern cvar_t *myq2_zombie;
extern cvar_t *myq2_zrounds;
extern cvar_t *myq2_zdiff;

void MYQ2_ZombieInit(void);
void MYQ2_ZombieOnLevelSpawn(void);
void MYQ2_ZombieOnPlayerSpawn(edict_t *ent);
void MYQ2_ZombieOnPlayerDeath(edict_t *ent);
int  MYQ2_ZombieModifyDamage(edict_t *targ, edict_t *attacker, int damage);
void MYQ2_ZombieOnDamageDealt(edict_t *targ, edict_t *attacker, int take);
void MYQ2_ZombieOnMonsterKilled(edict_t *targ, edict_t *attacker);
void MYQ2_ZombieRunFrame(void);
qboolean MYQ2_ZombieClientCommand(edict_t *ent, const char *cmd);

#endif /* MYQ2_ZOMBIE_H */
