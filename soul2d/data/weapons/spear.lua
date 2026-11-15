-- Right-Hand Spear (Weapon Base Class instance with Normal, Special, and Critical animations)
register_weapon("spear", {
    name = "Spear",
    kind = "MELEE",
    hand = "RIGHT",

    idle_pose = {
        { fwd =  3, right = 2, tile = "SPEAR_TIP_U", fg = "WHITE",  bg = "BLACK" },
        { fwd =  2, right = 2, tile = "SHAFT_V",     fg = "YELLOW", bg = "BLACK" },
        { fwd =  1, right = 2, tile = "SHAFT_V",     fg = "BROWN",  bg = "BLACK" },
        { fwd =  0, right = 2, tile = "SWORD_HILT",  fg = "YELLOW", bg = "BLACK" },
        { fwd = -1, right = 2, tile = "SHAFT_V",     fg = "BROWN",  bg = "BLACK" },
    },

    -- [K] Normal Attack: Fast Spear Thrust
    normal_attack = {
        name            = "THRUST",
        windup_frames   = 7,
        recovery_frames = 8,
        damage          = 18,
        chip_damage     = 3,
        stamina_cost    = 18,
        stamina_damage  = 22,
        causes_stagger  = 0,

        windup_pose = {
            { fwd =  1, right = 2, tile = "SPEAR_TIP_U", fg = "YELLOW", bg = "BROWN" },
            { fwd =  0, right = 2, tile = "SHAFT_V",     fg = "YELLOW", bg = "BROWN" },
            { fwd = -1, right = 2, tile = "SHAFT_V",     fg = "BROWN",  bg = "BLACK" },
            { fwd = -2, right = 2, tile = "SWORD_HILT",  fg = "BROWN",  bg = "BLACK" },
        },

        phases = {
            {
                frames = 4,
                rect = {
                    fwd1 = 3, fwd2 = 5, right1 = 0, right2 = 1,
                    tile = "SHAFT_V", tip_tile = "SPEAR_TIP_U",
                    fg = "WHITE", bg = "RED", tip_fg = "YELLOW", tip_bg = "RED"
                }
            },
            {
                frames = 7,
                rect = {
                    fwd1 = 3, fwd2 = 6, right1 = 0, right2 = 1,
                    tile = "SHAFT_V", tip_tile = "SPEAR_TIP_U",
                    fg = "WHITE", bg = "RED", tip_fg = "YELLOW", tip_bg = "RED"
                }
            },
        },

        recovery_pose = {
            { fwd = 3, right = 1, tile = "SPEAR_TIP_U", fg = "LIGHTRED", bg = "BLACK" },
            { fwd = 2, right = 1, tile = "SHAFT_V",     fg = "BROWN",    bg = "BLACK" },
            { fwd = 1, right = 2, tile = "SHAFT_V",     fg = "BROWN",    bg = "BLACK" },
        },
    },

    -- [I] Special Attack: Slow Heavy Spear Sweep
    special_attack = {
        name            = "SWEEP",
        windup_frames   = 18,
        recovery_frames = 16,
        damage          = 38,
        chip_damage     = 6,
        stamina_cost    = 34,
        stamina_damage  = 36,
        causes_stagger  = 1,

        windup_pose = {
            { fwd =  3, right = 0, ch = "!",             fg = "YELLOW", bg = "MAGENTA" },
            { fwd =  1, right = 3, tile = "SPEAR_TIP_U", fg = "YELLOW", bg = "MAGENTA" },
            { fwd =  0, right = 3, tile = "SHAFT_V",     fg = "YELLOW", bg = "MAGENTA" },
            { fwd = -1, right = 2, tile = "SHAFT_V",     fg = "BROWN",  bg = "BLACK"   },
        },

        phases = {
            {
                frames = 5,
                rect = {
                    fwd1 = 2, fwd2 = 5, right1 = 0, right2 = 3,
                    tile = "SLASH_ARC", tip_tile = "PARRY_STAR",
                    fg = "WHITE", bg = "MAGENTA", tip_fg = "YELLOW", tip_bg = "RED"
                }
            },
            {
                frames = 6,
                rect = {
                    fwd1 = 3, fwd2 = 6, right1 = -2, right2 = 2,
                    tile = "SLASH_ARC", tip_tile = "SPEAR_TIP_U",
                    fg = "WHITE", bg = "MAGENTA", tip_fg = "YELLOW", tip_bg = "RED"
                }
            },
            {
                frames = 5,
                rect = {
                    fwd1 = 2, fwd2 = 5, right1 = -3, right2 = 0,
                    tile = "SLASH_ARC", tip_tile = "PARRY_STAR",
                    fg = "WHITE", bg = "MAGENTA", tip_fg = "YELLOW", tip_bg = "RED"
                }
            },
        },

        recovery_pose = {
            { fwd = 2, right = -3, tile = "SPEAR_TIP_U", fg = "LIGHTRED", bg = "BLACK" },
            { fwd = 1, right = -2, tile = "SHAFT_V",     fg = "BROWN",    bg = "BLACK" },
            { fwd = 0, right = -1, tile = "SHAFT_H",     fg = "BROWN",    bg = "BLACK" },
        },
    },

    -- [K on Stunned or from Behind] Critical Attack: Deep Impale!
    crit_attack = {
        name            = "IMPALE!",
        windup_frames   = 10,
        recovery_frames = 14,
        damage          = 65,
        chip_damage     = 10,
        stamina_cost    = 15,
        stamina_damage  = 50,
        causes_stagger  = 1,

        windup_pose = {
            { fwd =  3, right =  0, tile = "CRIT_SPARK",  fg = "YELLOW", bg = "RED" },
            { fwd =  1, right =  1, tile = "SPEAR_TIP_U", fg = "WHITE",  bg = "RED" },
            { fwd =  0, right =  1, tile = "SHAFT_V",     fg = "YELLOW", bg = "RED" },
            { fwd = -1, right =  1, tile = "SWORD_HILT",  fg = "YELLOW", bg = "RED" },
        },

        phases = {
            -- Phase 1: Deep armor-piercing thrust
            {
                frames = 6,
                rect = {
                    fwd1 = 3, fwd2 = 7, right1 = -1, right2 = 1,
                    tile = "SHAFT_V", tip_tile = "SPEAR_TIP_U",
                    fg = "YELLOW", bg = "RED", tip_fg = "WHITE", tip_bg = "MAGENTA"
                }
            },
            -- Phase 2: Explosive twist & shockwave
            {
                frames = 8,
                rect = {
                    fwd1 = 3, fwd2 = 7, right1 = -2, right2 = 2,
                    tile = "CRIT_SPARK", tip_tile = "PARRY_STAR",
                    fg = "YELLOW", bg = "RED", tip_fg = "WHITE", tip_bg = "RED"
                }
            },
        },

        recovery_pose = {
            { fwd = 4, right = 0, tile = "SPEAR_TIP_U", fg = "YELLOW", bg = "RED"   },
            { fwd = 3, right = 0, tile = "SHAFT_V",     fg = "YELLOW", bg = "BLACK" },
            { fwd = 2, right = 1, tile = "SHAFT_V",     fg = "BROWN",  bg = "BLACK" },
        },
    },
})
