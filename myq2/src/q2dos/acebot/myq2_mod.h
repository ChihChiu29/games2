#ifndef MYQ2_MOD_H
#define MYQ2_MOD_H

extern cvar_t *myq2_random;
extern cvar_t *myq2_bots;
extern cvar_t *myq2_mappool;
extern cvar_t *myq2_hpregen;
extern cvar_t *myq2_ammoregen;
extern cvar_t *myq2_shuffle;
extern cvar_t *myq2_mutator;

void MYQ2_InitGame(void);
void MYQ2_OnLevelSpawn(void);
void MYQ2_RunFrame(void);
gitem_t *MYQ2_MaybeShuffleItem(gitem_t *item);
void MYQ2_ApplySpawnLoadout(edict_t *ent);
void MYQ2_OnFrag(edict_t *attacker);
void MYQ2_AnnounceMatch(edict_t *ent);
qboolean MYQ2_EndDMLevel(void);
const char *MYQ2_GetMutatorName(void);

#endif /* MYQ2_MOD_H */
