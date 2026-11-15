-- Left-Hand Knight Shield (Weapon Base Class instance of kind "SHIELD")
register_weapon("knight_shield", {
    name                = "Tower Shield",
    kind                = "SHIELD",
    hand                = "LEFT",
    block_move_cd       = 5,
    parry_window_frames = 11,

    idle_pose = {
        { fwd =  1, right = -2, tile = "SHIELD_EDGE", fg = "LIGHTCYAN", bg = "BLUE" },
        { fwd =  0, right = -2, tile = "SHIELD_MID",  fg = "YELLOW",    bg = "BLUE" },
        { fwd = -1, right = -2, tile = "SHIELD_EDGE", fg = "LIGHTCYAN", bg = "BLUE" },
    },
})
