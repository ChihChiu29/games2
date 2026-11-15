-- Broadsword (Weapon Base Class shared by Skeleton enemies AND Player via [TAB])
register_weapon("broadsword", {
    name = "Broadsword",
    kind = "MELEE",
    hand = "RIGHT",

    idle_pose = {
        { fwd = 2, right = 2, tile = "SWORD_TIP_U", fg = "WHITE",  bg = "BLACK" },
        { fwd = 1, right = 2, tile = "SHAFT_V",     fg = "WHITE",  bg = "BLACK" },
        { fwd = 0, right = 2, tile = "SWORD_HILT",  fg = "YELLOW", bg = "BLACK" },
    },

    -- Normal Attack: Wide Horizontal Slash
    normal_attack = {
        name            = "SLASH",
        windup_frames   = 26,
        recovery_frames = 22,
        damage          = 22,
        chip_damage     = 3,
        stamina_cost    = 20,
        stamina_damage  = 24,
        causes_stagger  = 0,

        windup_pose = {
            { fwd =  2, right = 0, ch = "!",             fg = "YELLOW", bg = "BROWN" },
            { fwd =  1, right = 3, tile = "SWORD_TIP_U", fg = "YELLOW", bg = "BROWN" },
            { fwd =  0, right = 3, tile = "SHAFT_V",     fg = "YELLOW", bg = "BROWN" },
            { fwd = -1, right = 2, tile = "SWORD_HILT",  fg = "YELLOW", bg = "BLACK" },
        },

        phases = {
            {
                frames = 5,
                rect = {
                    fwd1 = 2, fwd2 = 4, right1 = 0, right2 = 3,
                    tile = "SLASH_ARC", tip_tile = "PARRY_STAR",
                    fg = "YELLOW", bg = "RED", tip_fg = "YELLOW", tip_bg = "RED"
                }
            },
            {
                frames = 5,
                rect = {
                    fwd1 = 3, fwd2 = 5, right1 = -2, right2 = 2,
                    tile = "SLASH_ARC", tip_tile = "SWORD_TIP_U",
                    fg = "LIGHTRED", bg = "RED", tip_fg = "YELLOW", tip_bg = "RED"
                }
            },
            {
                frames = 4,
                rect = {
                    fwd1 = 2, fwd2 = 4, right1 = -3, right2 = 0,
                    tile = "SLASH_ARC", tip_tile = "PARRY_STAR",
                    fg = "YELLOW", bg = "RED", tip_fg = "YELLOW", tip_bg = "RED"
                }
            },
        },

        recovery_pose = {
            { fwd = 2, right = -2, tile = "SWORD_TIP_U", fg = "LIGHTRED", bg = "BLACK" },
            { fwd = 1, right = -2, tile = "SWORD_HILT",  fg = "YELLOW",   bg = "BLACK" },
        },
    },

    -- Special Attack: Heavy Overhead Cleave
    special_attack = {
        name            = "CLEAVE",
        windup_frames   = 32,
        recovery_frames = 28,
        damage          = 34,
        chip_damage     = 6,
        stamina_cost    = 32,
        stamina_damage  = 36,
        causes_stagger  = 1,

        windup_pose = {
            { fwd = 2, right = -1, ch = "!",             fg = "YELLOW", bg = "RED"   },
            { fwd = 2, right =  1, ch = "!",             fg = "YELLOW", bg = "RED"   },
            { fwd = 2, right =  2, tile = "SWORD_TIP_U", fg = "YELLOW", bg = "RED"   },
            { fwd = 1, right =  2, tile = "SHAFT_V",     fg = "YELLOW", bg = "RED"   },
            { fwd = 0, right =  2, tile = "SWORD_HILT",  fg = "YELLOW", bg = "BLACK" },
        },

        phases = {
            {
                frames = 14,
                rect = {
                    fwd1 = 3, fwd2 = 6, right1 = -1, right2 = 1,
                    tile = "SLASH_ARC", tip_tile = "SWORD_TIP_U",
                    fg = "YELLOW", bg = "RED", tip_fg = "WHITE", tip_bg = "RED"
                }
            },
        },

        recovery_pose = {
            { fwd = 2, right = -2, tile = "SWORD_TIP_U", fg = "LIGHTRED", bg = "BLACK" },
            { fwd = 1, right = -2, tile = "SWORD_HILT",  fg = "YELLOW",   bg = "BLACK" },
        },
    },

    -- Critical Attack: Executioner's Strike
    crit_attack = {
        name            = "EXECUTE!",
        windup_frames   = 10,
        recovery_frames = 15,
        damage          = 70,
        chip_damage     = 10,
        stamina_cost    = 15,
        stamina_damage  = 50,
        causes_stagger  = 1,

        windup_pose = {
            { fwd = 3, right = 0, tile = "CRIT_SPARK",  fg = "YELLOW", bg = "RED" },
            { fwd = 2, right = 0, tile = "SWORD_TIP_U", fg = "WHITE",  bg = "RED" },
            { fwd = 1, right = 0, tile = "SWORD_HILT",  fg = "YELLOW", bg = "RED" },
        },

        phases = {
            {
                frames = 10,
                rect = {
                    fwd1 = 3, fwd2 = 6, right1 = -2, right2 = 2,
                    tile = "CRIT_SPARK", tip_tile = "SWORD_TIP_U",
                    fg = "YELLOW", bg = "RED", tip_fg = "WHITE", tip_bg = "MAGENTA"
                }
            },
        },

        recovery_pose = {
            { fwd = 3, right = 0, tile = "SWORD_TIP_U", fg = "YELLOW", bg = "RED"   },
            { fwd = 2, right = 0, tile = "SWORD_HILT",  fg = "YELLOW", bg = "BLACK" },
        },
    },
})
