package com.unciv.logic.map.mapunit

import com.unciv.logic.IsPartOfGameInfoSerialization
import com.unciv.logic.civilization.Civilization
import com.unciv.logic.civilization.MapUnitAction
import com.unciv.logic.civilization.NotificationCategory
import yairm210.purity.annotations.LocalState
import yairm210.purity.annotations.Readonly
import kotlin.random.Random

/**
 * Persisted XP, Level-Up & Hero Ascension state for an individual [MapUnit].
 * Applies to both human and AI units.
 *
 * - Every 3 combat wins: +1 Level and +1 random stat (Atk, Def, or Mov; Mov capped at +3).
 * - Every 3 Levels (Lv3, Lv6, Lv9...): Hero Ascension!
 *   Automatically grants Hero Veteran combat bonus (+50%) plus 1 random Hero perk:
 *     (a) No Maintenance (skipped if already maintenance-free)
 *     (b) Aura: +1 random stat to self and all friendly units within 1 tile
 *     (c) +1 random stat on self x3
 *     (d) +1 Extra Life (survives a lethal combat blow at 50 HP)
 */
class UnitHeroData : IsPartOfGameInfoSerialization {
    var wins: Int = 0
    var level: Int = 0
    var heroTier: Int = 0
    var bonusAttack: Int = 0
    var bonusDefense: Int = 0
    var bonusMovement: Int = 0
    var extraLives: Int = 0
    var noMaintenance: Boolean = false

    @Readonly
    fun isHero(): Boolean = heroTier > 0

    @Readonly
    fun hasAnyProgress(): Boolean =
        wins > 0 || level > 0 || heroTier > 0 ||
            bonusAttack > 0 || bonusDefense > 0 || bonusMovement > 0 ||
            extraLives > 0 || noMaintenance

    @Readonly
    fun clone(): UnitHeroData {
        @LocalState val copy = UnitHeroData()
        copy.wins = wins
        copy.level = level
        copy.heroTier = heroTier
        copy.bonusAttack = bonusAttack
        copy.bonusDefense = bonusDefense
        copy.bonusMovement = bonusMovement
        copy.extraLives = extraLives
        copy.noMaintenance = noMaintenance
        return copy
    }

    @Readonly
    fun getBadgeText(): String? {
        if (!hasAnyProgress()) return null
        if (level == 0 && bonusAttack == 0 && bonusDefense == 0 && bonusMovement == 0 && extraLives == 0 && !noMaintenance) {
            return "W $wins/${UnitHeroSystem.WINS_PER_LEVEL}"
        }
        val sb = StringBuilder()
        sb.append(if (isHero()) "H$level" else "L$level")
        if (bonusAttack > 0 || bonusDefense > 0 || bonusMovement > 0) {
            sb.append(' ')
            if (bonusAttack > 0) sb.append("${bonusAttack}A")
            if (bonusDefense > 0) sb.append("${bonusDefense}D")
            if (bonusMovement > 0) sb.append("${bonusMovement}M")
        }
        if (extraLives > 0) sb.append(" ${extraLives}L")
        if (noMaintenance) sb.append(" Free")
        return sb.toString()
    }
}

object UnitHeroSystem {
    const val WINS_PER_LEVEL = 3
    const val LEVELS_PER_HERO = 3
    const val MAX_MOV_BONUS = 3
    const val MAX_STAT_BONUS = 20
    const val MAX_EXTRA_LIVES = 9
    const val HERO_VETERAN_COMBAT_BONUS = 50
    const val EXTRA_LIFE_SURVIVE_HP = 50

    fun addRandomStat(unit: MapUnit, statOverride: Int? = null, random: Random = Random.Default): String {
        val canAtk = unit.baseUnit.strength > 0 || unit.baseUnit.rangedStrength > 0
        val canMov = unit.hero.bonusMovement < MAX_MOV_BONUS
        val stat = if (statOverride == null) {
            val pool = ArrayList<Int>(3)
            if (canAtk) pool.add(0)
            pool.add(1)
            if (canMov) pool.add(2)
            pool[random.nextInt(pool.size)]
        } else {
            var s = statOverride
            if (s == 0 && !canAtk) s = 1
            if (s == 2 && !canMov) s = if (canAtk) random.nextInt(2) else 1
            s
        }
        return when (stat) {
            0 -> {
                if (unit.hero.bonusAttack < MAX_STAT_BONUS) unit.hero.bonusAttack++
                "Atk"
            }
            1 -> {
                if (unit.hero.bonusDefense < MAX_STAT_BONUS) unit.hero.bonusDefense++
                "Def"
            }
            else -> {
                if (unit.hero.bonusMovement < MAX_MOV_BONUS) unit.hero.bonusMovement++
                "Mov"
            }
        }
    }

    fun onCombatWin(unit: MapUnit, random: Random = Random.Default) {
        if (unit.isDestroyed) return
        val hero = unit.hero
        if (hero.wins < 255) hero.wins++
        if (hero.wins % WINS_PER_LEVEL != 0) return

        hero.level++
        val statName = addRandomStat(unit, null, random)

        if (hero.level % LEVELS_PER_HERO != 0) {
            unit.civ.addNotification(
                "${unit.displayName()} Lv${hero.level}: +1 $statName!",
                MapUnitAction(unit),
                NotificationCategory.Units,
                unit.name
            )
            return
        }

        // Every 3rd level: HERO ASCENSION!
        hero.heroTier++

        // Pick one of 4 Hero perks (skip No Maintenance if already free)
        val perk = if (!hero.noMaintenance) random.nextInt(4) else 1 + random.nextInt(3)

        when (perk) {
            0 -> {
                hero.noMaintenance = true
                unit.civ.addNotification(
                    "HERO ${unit.displayName()} Lv${hero.level}: No Upkeep!",
                    MapUnitAction(unit),
                    NotificationCategory.Units,
                    unit.name
                )
            }
            1 -> {
                val auraStat = random.nextInt(3)
                val auraName = when (auraStat) {
                    0 -> "Atk"
                    1 -> "Def"
                    else -> "Mov"
                }
                if (unit.hasTile()) {
                    for (tile in unit.getTile().getTilesInDistance(1)) {
                        for (friendly in tile.getUnits()) {
                            if (friendly.civ == unit.civ) {
                                addRandomStat(friendly, auraStat, random)
                            }
                        }
                    }
                } else {
                    addRandomStat(unit, auraStat, random)
                }
                unit.civ.addNotification(
                    "HERO ${unit.displayName()} Lv${hero.level} Aura: +1 $auraName (nearby)!",
                    MapUnitAction(unit),
                    NotificationCategory.Units,
                    unit.name
                )
            }
            2 -> {
                addRandomStat(unit, null, random)
                addRandomStat(unit, null, random)
                addRandomStat(unit, null, random)
                unit.civ.addNotification(
                    "HERO ${unit.displayName()} Lv${hero.level}: +3 Stats!",
                    MapUnitAction(unit),
                    NotificationCategory.Units,
                    unit.name
                )
            }
            else -> {
                if (hero.extraLives < MAX_EXTRA_LIVES) hero.extraLives++
                unit.civ.addNotification(
                    "HERO ${unit.displayName()} Lv${hero.level}: +1 Extra Life!",
                    MapUnitAction(unit),
                    NotificationCategory.Units,
                    unit.name
                )
            }
        }
    }

    /**
     * Called right before [unit] would die in combat.
     * Returns `true` if an Extra Life saved the unit (restoring HP to [EXTRA_LIFE_SURVIVE_HP]).
     */
    fun tryConsumeExtraLife(unit: MapUnit, enemyCiv: Civilization? = null): Boolean {
        if (unit.hero.extraLives <= 0) return false
        unit.hero.extraLives--
        unit.health = EXTRA_LIFE_SURVIVE_HP
        unit.cache.updateUniques()
        unit.civ.addNotification(
            "HERO ${unit.displayName()} survived! (${unit.hero.extraLives} lives left)",
            MapUnitAction(unit),
            NotificationCategory.War,
            unit.name
        )
        if (enemyCiv != null && enemyCiv != unit.civ) {
            enemyCiv.addNotification(
                "Enemy HERO ${unit.displayName()} survived!",
                unit.getTile().position,
                NotificationCategory.War,
                unit.name
            )
        }
        return true
    }
}
