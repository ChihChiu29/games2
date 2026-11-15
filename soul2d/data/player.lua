-- Player Configuration (5x5 Knight with Left-Hand Shield & Right-Hand Spear)
-- Coordinates are in DIR_UP reference frame (fwd > 0 is front, right > 0 is right hand).
-- The C engine automatically rotates coordinates & directional glyphs for UP/RIGHT/DOWN/LEFT.

configure_player({
    radius              = 2,
    max_hp              = 100,
    max_stamina         = 100,
    move_cd_normal      = 3,
    move_cd_block       = 5,
    parry_window_frames = 11,
    roll_frames         = 14,
    roll_iframes        = 10,
    roll_stamina_cost   = 22,

    -- 5x5 Knight Body (fwd = -2..+2, right = -1..+1)
    body = {
        { fwd =  2, right = -1, ch = "/",  fg = "LIGHTCYAN", bg = "BLACK" },
        { fwd =  2, right =  0, ch = "^",  fg = "YELLOW",    bg = "BLUE"  },
        { fwd =  2, right =  1, ch = "\\", fg = "LIGHTCYAN", bg = "BLACK" },

        { fwd =  1, right = -1, ch = "[",  fg = "WHITE",     bg = "BLACK" },
        { fwd =  1, right =  0, ch = "^",  fg = "YELLOW",    bg = "BLACK" },
        { fwd =  1, right =  1, ch = "]",  fg = "WHITE",     bg = "BLACK" },

        { fwd =  0, right = -1, ch = "|",  fg = "LIGHTCYAN", bg = "BLACK" },
        { fwd =  0, right =  0, ch = "@",  fg = "WHITE",     bg = "BLUE"  },
        { fwd =  0, right =  1, ch = "|",  fg = "LIGHTCYAN", bg = "BLACK" },

        { fwd = -1, right = -1, ch = "\\", fg = "WHITE",     bg = "BLACK" },
        { fwd = -1, right =  0, ch = "=",  fg = "LIGHTCYAN", bg = "BLACK" },
        { fwd = -1, right =  1, ch = "/",  fg = "WHITE",     bg = "BLACK" },

        { fwd = -2, right = -1, ch = "/",  fg = "LIGHTGRAY", bg = "BLACK" },
        { fwd = -2, right =  0, ch = "v",  fg = "DARKGRAY",  bg = "BLACK" },
        { fwd = -2, right =  1, ch = "\\", fg = "LIGHTGRAY", bg = "BLACK" },
    },

    -- Left Hand: Shield when idle (at right = -2)
    idle_shield = {
        { fwd =  1, right = -2, ch = "[", fg = "LIGHTCYAN", bg = "BLUE" },
        { fwd =  0, right = -2, ch = "#", fg = "LIGHTCYAN", bg = "BLUE" },
        { fwd = -1, right = -2, ch = "[", fg = "LIGHTCYAN", bg = "BLUE" },
    },

    -- Right Hand: Spear when idle (at right = +2)
    idle_weapon = {
        { fwd =  3, right =  2, ch = "^", fg = "LIGHTRED",  bg = "BLACK" },
        { fwd =  2, right =  2, ch = "|", fg = "YELLOW",    bg = "BLACK" },
        { fwd =  1, right =  2, ch = "|", fg = "BROWN",     bg = "BLACK" },
        { fwd =  0, right =  2, ch = "+", fg = "YELLOW",    bg = "BLACK" },
        { fwd = -1, right =  2, ch = "|", fg = "BROWN",     bg = "BLACK" },
        { fwd = -2, right =  2, ch = "|", fg = "BROWN",     bg = "BLACK" },
    },

    -- [K] Right-Hand Normal Attack: Fast Spear Thrust
    light_attack = {
        name            = "THRUST",
        windup_frames   = 7,
        recovery_frames = 8,
        damage          = 18,
        riposte_damage  = 36,
        stamina_cost    = 18,
        causes_stagger  = 0,

        windup_pose = {
            { fwd =  1, right = 2, ch = "^", fg = "YELLOW", bg = "BROWN" },
            { fwd =  0, right = 2, ch = "|", fg = "YELLOW", bg = "BROWN" },
            { fwd = -1, right = 2, ch = "|", fg = "BROWN",  bg = "BLACK" },
            { fwd = -2, right = 2, ch = "|", fg = "BROWN",  bg = "BLACK" },
            { fwd = -3, right = 2, ch = "+", fg = "BROWN",  bg = "BLACK" },
        },

        phases = {
            -- Phase 1: Initial lunge (4 frames, reaches fwd = 3..5)
            {
                frames = 4,
                rect = {
                    fwd1 = 3, fwd2 = 5, right1 = 0, right2 = 1,
                    ch = "|", tip_ch = "^",
                    fg = "WHITE", bg = "RED", tip_fg = "YELLOW", tip_bg = "RED"
                }
            },
            -- Phase 2: Full extension (7 frames, reaches fwd = 3..6)
            {
                frames = 7,
                rect = {
                    fwd1 = 3, fwd2 = 6, right1 = 0, right2 = 1,
                    ch = "|", tip_ch = "^",
                    fg = "WHITE", bg = "RED", tip_fg = "YELLOW", tip_bg = "RED"
                }
            },
        },

        recovery_pose = {
            { fwd = 3, right = 1, ch = "^", fg = "LIGHTRED", bg = "BLACK" },
            { fwd = 2, right = 1, ch = "|", fg = "BROWN",    bg = "BLACK" },
            { fwd = 1, right = 2, ch = "|", fg = "BROWN",    bg = "BLACK" },
            { fwd = 0, right = 2, ch = "|", fg = "BROWN",    bg = "BLACK" },
        },
    },

    -- [I] Right-Hand Special Attack: Slow Heavy Spear Sweep
    heavy_attack = {
        name            = "SWEEP",
        windup_frames   = 18,
        recovery_frames = 16,
        damage          = 38,
        riposte_damage  = 70,
        stamina_cost    = 34,
        causes_stagger  = 1,

        windup_pose = {
            { fwd =  3, right = 0, ch = "!", fg = "YELLOW", bg = "MAGENTA" },
            { fwd =  1, right = 3, ch = "^", fg = "YELLOW", bg = "MAGENTA" },
            { fwd =  0, right = 3, ch = "|", fg = "YELLOW", bg = "MAGENTA" },
            { fwd = -1, right = 2, ch = "/", fg = "BROWN",  bg = "BLACK"   },
            { fwd = -2, right = 2, ch = "+", fg = "BROWN",  bg = "BLACK"   },
        },

        phases = {
            -- Phase 1: Right-front sweep arc (5 frames)
            {
                frames = 5,
                rect = {
                    fwd1 = 2, fwd2 = 5, right1 = 0, right2 = 3,
                    ch = "/", tip_ch = "*",
                    fg = "WHITE", bg = "MAGENTA", tip_fg = "YELLOW", tip_bg = "RED"
                }
            },
            -- Phase 2: Deep center cleave (6 frames)
            {
                frames = 6,
                rect = {
                    fwd1 = 3, fwd2 = 6, right1 = -2, right2 = 2,
                    ch = "*", tip_ch = "^",
                    fg = "WHITE", bg = "MAGENTA", tip_fg = "YELLOW", tip_bg = "RED"
                }
            },
            -- Phase 3: Left-front follow-through arc (5 frames)
            {
                frames = 5,
                rect = {
                    fwd1 = 2, fwd2 = 5, right1 = -3, right2 = 0,
                    ch = "\\", tip_ch = "*",
                    fg = "WHITE", bg = "MAGENTA", tip_fg = "YELLOW", tip_bg = "RED"
                }
            },
        },

        recovery_pose = {
            { fwd = 2, right = -3, ch = "^",  fg = "LIGHTRED", bg = "BLACK" },
            { fwd = 1, right = -2, ch = "\\", fg = "BROWN",    bg = "BLACK" },
            { fwd = 0, right = -1, ch = "-",  fg = "BROWN",    bg = "BLACK" },
        },
    },
})
