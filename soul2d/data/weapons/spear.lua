-- Right-Hand Spear (Procedural Spear with Thrust, Sweep, and Impale Critical)
register_weapon("spear", {
    name           = "Spear",
    kind           = "MELEE",
    style          = "SPEAR",
    level          = 1,
    length_px      = 22,
    blade_width_px = 2,

    skill = {
        name            = "PIERCE CHARGE",
        stamina_cost    = 28,
        cooldown_frames = 90,
        atk_slot        = 1,
        bonus_damage    = 10,
    },

    -- [K] Normal Attack: Fast Spear Thrust
    normal_attack = {
        name            = "THRUST",
        anim            = "THRUST",
        windup_frames   = 11,
        active_frames   = 11,
        recovery_frames = 14,
        reach_px        = 28,
        width_px        = 5,
        damage          = 18,
        chip_damage     = 3,
        stamina_cost    = 18,
        stamina_damage  = 22,
        causes_stagger  = 0,
    },

    -- [I] Special Attack: Slow Heavy Spear Sweep
    special_attack = {
        name            = "SWEEP",
        anim            = "SLASH",
        windup_frames   = 22,
        active_frames   = 16,
        recovery_frames = 20,
        reach_px        = 26,
        width_px        = 16,
        damage          = 38,
        chip_damage     = 6,
        stamina_cost    = 34,
        stamina_damage  = 36,
        causes_stagger  = 1,
    },

    -- [K on Stunned or from Behind] Critical Attack: Deep Impale!
    crit_attack = {
        name            = "IMPALE!",
        anim            = "IMPALE",
        windup_frames   = 12,
        active_frames   = 16,
        recovery_frames = 18,
        reach_px        = 32,
        width_px        = 8,
        damage          = 68,
        chip_damage     = 10,
        stamina_cost    = 15,
        stamina_damage  = 50,
        causes_stagger  = 1,
    },
})
