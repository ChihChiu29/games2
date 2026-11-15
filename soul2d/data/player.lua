-- Player Configuration (Knight Class extending CharacterDef)
-- Equips "knight_shield" on Left Hand and "spear" on Right Hand (press [TAB] to swap to "broadsword").

configure_player({
    name               = "Knight",
    radius             = 2,
    max_hp             = 100,
    max_stamina        = 100,
    move_cd_normal     = 3,
    stagger_duration   = 32,
    crit_hurt_duration = 40,
    roll_frames        = 14,
    roll_iframes       = 10,
    roll_stamina_cost  = 22,

    left_weapon        = "knight_shield",
    right_weapon       = "spear",
    alt_right_weapon   = "broadsword",

    -- Knight Body Silhouette (Helmet, Plate Armor, Pauldrons, Boots)
    body = {
        { fwd =  1, right =  0, tile = "KNIGHT_HELM",  fg = "YELLOW",    bg = "BLUE"  },

        { fwd =  0, right = -1, tile = "KNIGHT_ARMOR", fg = "LIGHTCYAN", bg = "BLACK" },
        { fwd =  0, right =  0, tile = "KNIGHT_ARMOR", fg = "WHITE",     bg = "BLUE"  },
        { fwd =  0, right =  1, tile = "KNIGHT_ARMOR", fg = "LIGHTCYAN", bg = "BLACK" },

        { fwd = -1, right = -1, tile = "KNIGHT_BOOT",  fg = "LIGHTGRAY", bg = "BLACK" },
        { fwd = -1, right =  1, tile = "KNIGHT_BOOT",  fg = "LIGHTGRAY", bg = "BLACK" },
    },

    stagger_pose = {
        { fwd = 2, right = -1, tile = "PARRY_STAR", fg = "YELLOW", bg = "RED" },
        { fwd = 2, right =  1, tile = "PARRY_STAR", fg = "YELLOW", bg = "RED" },
    },

    crit_hurt_pose = {
        { fwd =  2, right = -1, tile = "CRIT_SPARK", fg = "YELLOW", bg = "RED" },
        { fwd =  2, right =  1, tile = "CRIT_SPARK", fg = "YELLOW", bg = "RED" },
        { fwd = -2, right =  0, tile = "CRIT_SPARK", fg = "WHITE",  bg = "RED" },
    },
})
