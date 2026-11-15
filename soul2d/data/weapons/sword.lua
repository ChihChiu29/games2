-- Broadsword (Procedural Sword shared by Skeletons & Player via [TAB])
register_weapon("broadsword", {
    name           = "Broadsword",
    kind           = "MELEE",
    style          = "SWORD",
    level          = 1,
    length_px      = 17,
    blade_width_px = 3,

    skill = {
        name            = "HEAVY CLEAVE",
        stamina_cost    = 26,
        cooldown_frames = 80,
        atk_slot        = 1,
        bonus_damage    = 10,
    },

    -- Normal Attack: Wide Horizontal Slash
    normal_attack = {
        name            = "SLASH",
        anim            = "SLASH",
        windup_frames   = 32,
        active_frames   = 15,
        recovery_frames = 28,
        reach_px        = 22,
        width_px        = 15,
        damage          = 22,
        chip_damage     = 3,
        stamina_cost    = 20,
        stamina_damage  = 24,
        causes_stagger  = 0,
    },

    -- Special Attack: Heavy Overhead Cleave
    special_attack = {
        name            = "CLEAVE",
        anim            = "CLEAVE",
        windup_frames   = 38,
        active_frames   = 16,
        recovery_frames = 32,
        reach_px        = 25,
        width_px        = 7,
        damage          = 34,
        chip_damage     = 6,
        stamina_cost    = 32,
        stamina_damage  = 36,
        causes_stagger  = 1,
    },

    -- Critical Attack: Executioner's Strike
    crit_attack = {
        name            = "EXECUTE!",
        anim            = "CLEAVE",
        windup_frames   = 12,
        active_frames   = 14,
        recovery_frames = 18,
        reach_px        = 26,
        width_px        = 10,
        damage          = 72,
        chip_damage     = 10,
        stamina_cost    = 15,
        stamina_damage  = 50,
        causes_stagger  = 1,
    },
})
