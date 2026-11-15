-- Skeleton Enemy Class (extends CharacterDef)
-- Wields "broadsword" from data/weapons/sword.lua and defines a bone-shattering
-- special effect when critically attacked (via Backstab or Riposte on Stunned).

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
    name               = "Skeleton",
    radius             = 2,
    max_hp             = 90,
    move_cooldown      = 8,
    stagger_duration   = 95,
    crit_hurt_duration = 50,
    crit_flash_fg      = "YELLOW",
    crit_flash_bg      = "RED",

    right_weapon       = "broadsword",
    ai_func            = "skeleton_ai",

    -- Skeleton Silhouette (Skull, Ribcage, Bone Arms, Pelvis, Bone Legs)
    body = {
        { fwd =  1, right =  0, tile = "SKEL_SKULL",  fg = "WHITE",     bg = "RED"   },

        { fwd =  0, right = -1, tile = "SKEL_LIMB",   fg = "LIGHTGRAY", bg = "BLACK" },
        { fwd =  0, right =  0, tile = "SKEL_RIBS",   fg = "WHITE",     bg = "BLACK" },
        { fwd =  0, right =  1, tile = "SKEL_LIMB",   fg = "LIGHTGRAY", bg = "BLACK" },

        { fwd = -1, right =  0, tile = "SKEL_PELVIS", fg = "LIGHTGRAY", bg = "BLACK" },

        { fwd = -2, right = -1, tile = "SKEL_LIMB",   fg = "LIGHTGRAY", bg = "BLACK" },
        { fwd = -2, right =  1, tile = "SKEL_LIMB",   fg = "LIGHTGRAY", bg = "BLACK" },
    },

    -- Stunned / Parried Pose (vulnerable to Critical Riposte!)
    stagger_pose = {
        { fwd =  2, right = -1, tile = "PARRY_STAR",  fg = "YELLOW",   bg = "BLUE"  },
        { fwd =  2, right =  1, tile = "PARRY_STAR",  fg = "YELLOW",   bg = "BLUE"  },
        { fwd = -1, right =  2, tile = "SWORD_TIP_R", fg = "LIGHTRED", bg = "BLACK" },
        { fwd = -2, right =  2, tile = "SWORD_HILT",  fg = "YELLOW",   bg = "BLACK" },
    },

    -- Special Effect Pose when Critically Attacked (Backstabbed or Riposted):
    -- Flying bone shards & impact sparks bursting outward from the skeleton!
    crit_hurt_pose = {
        { fwd =  2, right = -2, tile = "BONE_SHARD", fg = "WHITE",  bg = "RED"     },
        { fwd =  2, right =  0, tile = "CRIT_SPARK", fg = "YELLOW", bg = "RED"     },
        { fwd =  2, right =  2, tile = "BONE_SHARD", fg = "WHITE",  bg = "RED"     },
        { fwd =  0, right = -2, tile = "CRIT_SPARK", fg = "YELLOW", bg = "MAGENTA" },
        { fwd =  0, right =  2, tile = "CRIT_SPARK", fg = "YELLOW", bg = "MAGENTA" },
        { fwd = -2, right = -2, tile = "BONE_SHARD", fg = "WHITE",  bg = "RED"     },
        { fwd = -2, right =  2, tile = "BONE_SHARD", fg = "WHITE",  bg = "RED"     },
    },
})
