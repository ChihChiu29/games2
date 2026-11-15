-- Skeleton & Skeleton Lord Enemy Classes (Procedural Rig + Variable Scale)

function skeleton_ai(hp, max_hp, primary_dist, off_axis)
    if primary_dist <= 28 and off_axis <= 12 then
        if irand(3) == 1 then
            return "CLEAVE"
        else
            return "SLASH"
        end
    end
    return "approach"
end

-- Normal Undead Skeleton Soldier (100% scale)
register_enemy("skeleton", {
    name               = "SKEL",
    body_style         = "SKELETON",
    scale_pct          = 100,
    radius             = 8,
    max_hp             = 90,
    move_speed_fp      = 7,   -- ~0.44 px/frame (deliberate undead walk)
    turn_cooldown      = 24,
    stagger_duration   = 95,
    crit_hurt_duration = 50,
    right_weapon       = "broadsword",
    ai_func            = "skeleton_ai",
})

-- Larger Elite Crowned Skeleton Lord (135% scale, higher HP & wider reach)
register_enemy("skeleton_lord", {
    name               = "LORD",
    body_style         = "SKELETON_LORD",
    scale_pct          = 135,
    radius             = 11,
    max_hp             = 140,
    move_speed_fp      = 6,   -- ~0.38 px/frame (heavy, menacing stride)
    turn_cooldown      = 30,
    stagger_duration   = 90,
    crit_hurt_duration = 55,
    right_weapon       = "broadsword",
    ai_func            = "skeleton_ai",
})
