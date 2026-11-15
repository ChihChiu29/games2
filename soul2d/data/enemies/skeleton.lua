-- Skeleton Enemy Type (5x5 ASCII Art + Single-Hand Sword + Lua AI)
-- Coordinates are in DIR_UP reference frame (fwd > 0 is front, right > 0 is right hand).

function skeleton_ai(hp, max_hp, primary_dist, off_axis)
    if primary_dist <= 8 and off_axis <= 3 then
        if irand(3) == 1 then
            return "CLEAVE"
        else
            return "SLASH"
        end
    end
    return "approach"
end

register_enemy("skeleton", {
    name             = "Skeleton",
    radius           = 2,
    max_hp           = 90,
    move_cooldown    = 8,
    stagger_duration = 95,
    ai_func          = "skeleton_ai",

    -- 5x5 Skeleton Body (Facing UP reference)
    body = {
        { fwd =  2, right = -1, ch = ".",  fg = "LIGHTGRAY", bg = "BLACK" },
        { fwd =  2, right =  0, ch = "^",  fg = "YELLOW",    bg = "RED"   },
        { fwd =  2, right =  1, ch = ".",  fg = "LIGHTGRAY", bg = "BLACK" },

        { fwd =  1, right = -1, ch = "o",  fg = "LIGHTRED",  bg = "BLACK" },
        { fwd =  1, right =  0, ch = "A",  fg = "WHITE",     bg = "BLACK" },
        { fwd =  1, right =  1, ch = "o",  fg = "LIGHTRED",  bg = "BLACK" },

        { fwd =  0, right = -2, ch = "/",  fg = "LIGHTGRAY", bg = "BLACK" },
        { fwd =  0, right = -1, ch = "#",  fg = "LIGHTGRAY", bg = "BLACK" },
        { fwd =  0, right =  0, ch = "S",  fg = "WHITE",     bg = "BLACK" },
        { fwd =  0, right =  1, ch = "#",  fg = "LIGHTGRAY", bg = "BLACK" },

        { fwd = -1, right = -1, ch = "x",  fg = "LIGHTGRAY", bg = "BLACK" },
        { fwd = -1, right =  0, ch = "|",  fg = "LIGHTGRAY", bg = "BLACK" },
        { fwd = -1, right =  1, ch = "x",  fg = "LIGHTGRAY", bg = "BLACK" },

        { fwd = -2, right = -1, ch = "/",  fg = "LIGHTGRAY", bg = "BLACK" },
        { fwd = -2, right =  1, ch = "\\", fg = "LIGHTGRAY", bg = "BLACK" },
    },

    -- Right Hand: Single-Hand Sword when idle
    idle_weapon = {
        { fwd =  2, right = 2, ch = "^", fg = "LIGHTRED", bg = "BLACK" },
        { fwd =  1, right = 2, ch = "|", fg = "WHITE",    bg = "BLACK" },
        { fwd =  0, right = 2, ch = "+", fg = "YELLOW",   bg = "BLACK" },
        { fwd = -1, right = 2, ch = "o", fg = "YELLOW",   bg = "BLACK" },
    },

    -- Staggered / Parried Pose (Stars above skull, sword dropped wide)
    stagger_pose = {
        { fwd =  3, right = -1, ch = "*", fg = "YELLOW",   bg = "BLUE"  },
        { fwd =  3, right =  1, ch = "*", fg = "YELLOW",   bg = "BLUE"  },
        { fwd = -1, right =  3, ch = "/", fg = "LIGHTRED", bg = "BLACK" },
        { fwd = -2, right =  2, ch = "+", fg = "YELLOW",   bg = "BLACK" },
    },

    -- Skeleton Sword Attacks
    attacks = {
        -- Attack 1: Wide Horizontal Sword Slash
        {
            name            = "SLASH",
            windup_frames   = 30,
            recovery_frames = 28,
            damage          = 20,
            chip_damage     = 3,
            stamina_damage  = 24,

            windup_pose = {
                { fwd =  3, right = 0, ch = "!", fg = "YELLOW", bg = "BROWN" },
                { fwd =  1, right = 3, ch = "^", fg = "YELLOW", bg = "BROWN" },
                { fwd =  0, right = 3, ch = "|", fg = "YELLOW", bg = "BROWN" },
                { fwd = -1, right = 2, ch = "+", fg = "YELLOW", bg = "BLACK" },
            },

            phases = {
                -- Phase 1: Right-front slash (5 frames)
                {
                    frames = 5,
                    rect = {
                        fwd1 = 2, fwd2 = 4, right1 = 0, right2 = 3,
                        ch = "/", tip_ch = "*",
                        fg = "YELLOW", bg = "RED", tip_fg = "YELLOW", tip_bg = "RED"
                    }
                },
                -- Phase 2: Center wide arc (5 frames)
                {
                    frames = 5,
                    rect = {
                        fwd1 = 3, fwd2 = 5, right1 = -2, right2 = 2,
                        ch = "-", tip_ch = "^",
                        fg = "LIGHTRED", bg = "RED", tip_fg = "YELLOW", tip_bg = "RED"
                    }
                },
                -- Phase 3: Left-front follow-through (4 frames)
                {
                    frames = 4,
                    rect = {
                        fwd1 = 2, fwd2 = 4, right1 = -3, right2 = 0,
                        ch = "\\", tip_ch = "*",
                        fg = "YELLOW", bg = "RED", tip_fg = "YELLOW", tip_bg = "RED"
                    }
                },
            },

            recovery_pose = {
                { fwd = 2, right = -2, ch = "/", fg = "LIGHTRED", bg = "BLACK" },
                { fwd = 1, right = -2, ch = "+", fg = "YELLOW",   bg = "BLACK" },
            },
        },

        -- Attack 2: Heavy Overhead Sword Cleave
        {
            name            = "CLEAVE",
            windup_frames   = 36,
            recovery_frames = 32,
            damage          = 30,
            chip_damage     = 6,
            stamina_damage  = 36,

            windup_pose = {
                { fwd = 3, right = -1, ch = "!", fg = "YELLOW", bg = "RED"   },
                { fwd = 3, right =  1, ch = "!", fg = "YELLOW", bg = "RED"   },
                { fwd = 2, right =  2, ch = "^", fg = "YELLOW", bg = "RED"   },
                { fwd = 1, right =  2, ch = "|", fg = "YELLOW", bg = "RED"   },
                { fwd = 0, right =  2, ch = "+", fg = "YELLOW", bg = "BLACK" },
            },

            phases = {
                {
                    frames = 14,
                    rect = {
                        fwd1 = 3, fwd2 = 6, right1 = -1, right2 = 1,
                        ch = "|", tip_ch = "^",
                        fg = "YELLOW", bg = "RED", tip_fg = "WHITE", tip_bg = "RED"
                    }
                },
            },

            recovery_pose = {
                { fwd = 2, right = -2, ch = "/", fg = "LIGHTRED", bg = "BLACK" },
                { fwd = 1, right = -2, ch = "+", fg = "YELLOW",   bg = "BLACK" },
            },
        },
    },
})
