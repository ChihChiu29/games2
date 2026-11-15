-- Player Configuration (Knight Class in 320x200 pixel space, Dark Souls 1 pace)
configure_player({
    name               = "Knight",
    body_style         = "KNIGHT",
    scale_pct          = 100,
    radius             = 8,
    max_hp             = 100,
    max_stamina        = 100,
    move_speed_fp      = 13,  -- ~0.81 px/frame (weighty Dark Souls 1 walk speed)
    stagger_duration   = 36,
    crit_hurt_duration = 42,

    roll_frames        = 18,
    roll_iframes       = 12,
    roll_speed_fp      = 34,  -- ~2.1 px/frame dodge roll burst
    roll_stamina_cost  = 22,

    left_weapon        = "knight_shield",
    right_weapon       = "spear",
    alt_right_weapon   = "broadsword",
})
