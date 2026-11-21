package com.unciv.logic.map.mapunit

import com.unciv.logic.battle.Battle
import com.unciv.logic.battle.BattleDamage
import com.unciv.logic.battle.MapUnitCombatant
import com.unciv.logic.civilization.Civilization
import com.unciv.logic.map.HexCoord
import com.unciv.testing.GdxTestRunner
import com.unciv.testing.TestGame
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.random.Random

@RunWith(GdxTestRunner::class)
class UnitHeroSystemTest {
    private lateinit var testGame: TestGame
    private lateinit var civ1: Civilization
    private lateinit var civ2: Civilization

    @Before
    fun setUp() {
        testGame = TestGame()
        testGame.makeHexagonalMap(4)
        civ1 = testGame.addCiv()
        civ2 = testGame.addCiv()
    }

    @Test
    fun `every 3 combat wins levels up and grants random stat`() {
        val unit = testGame.addUnit("Warrior", civ1, testGame.getTile(HexCoord.Zero))
        assertNull(unit.hero.getBadgeText())

        UnitHeroSystem.onCombatWin(unit, Random(42))
        assertEquals(1, unit.hero.wins)
        assertEquals(0, unit.hero.level)
        assertEquals("W 1/3", unit.hero.getBadgeText())

        UnitHeroSystem.onCombatWin(unit, Random(42))
        assertEquals(2, unit.hero.wins)
        assertEquals(0, unit.hero.level)
        assertEquals("W 2/3", unit.hero.getBadgeText())

        UnitHeroSystem.onCombatWin(unit, Random(42))
        assertEquals(3, unit.hero.wins)
        assertEquals(1, unit.hero.level)
        assertFalse(unit.hero.isHero())
        val totalStats = unit.hero.bonusAttack + unit.hero.bonusDefense + unit.hero.bonusMovement
        assertEquals(1, totalStats)
        assertTrue(unit.hero.getBadgeText()!!.startsWith("L1 "))
    }

    @Test
    fun `level 3 triggers hero ascension and grants veteran combat bonus`() {
        val heroUnit = testGame.addUnit("Warrior", civ1, testGame.getTile(HexCoord.Zero))
        val enemyUnit = testGame.addUnit("Warrior", civ2, testGame.getTile(1, 0))
        val rng = Random(123)

        repeat(9) {
            UnitHeroSystem.onCombatWin(heroUnit, rng)
        }

        assertEquals(9, heroUnit.hero.wins)
        assertEquals(3, heroUnit.hero.level)
        assertEquals(1, heroUnit.hero.heroTier)
        assertTrue(heroUnit.hero.isHero())
        assertTrue(heroUnit.hero.getBadgeText()!!.startsWith("H3"))

        val attackModifiers = BattleDamage.getAttackModifiers(
            MapUnitCombatant(heroUnit),
            MapUnitCombatant(enemyUnit),
            heroUnit.getTile()
        )
        assertEquals(UnitHeroSystem.HERO_VETERAN_COMBAT_BONUS, attackModifiers["Hero (Veteran)"])
    }

    @Test
    fun `stat bonuses apply to combat strength and max movement`() {
        val unit = testGame.addUnit("Warrior", civ1, testGame.getTile(HexCoord.Zero))
        val baseStr = unit.baseUnit.strength
        val baseMov = unit.baseUnit.movement

        unit.hero.bonusAttack = 2
        unit.hero.bonusDefense = 3
        unit.hero.bonusMovement = 2

        val combatant = MapUnitCombatant(unit)
        assertEquals(baseStr + 2, combatant.getAttackingStrength())
        assertEquals(baseStr + 3, combatant.getDefendingStrength())
        assertEquals(baseMov + 2, unit.getMaxMovement())
    }

    @Test
    fun `extra life saves unit from lethal melee blow`() {
        val attacker = testGame.addUnit("Infantry", civ1, testGame.getTile(1, 0))
        val defender = testGame.addUnit("Warrior", civ2, testGame.getTile(HexCoord.Zero))
        defender.health = 5
        defender.hero.heroTier = 1
        defender.hero.extraLives = 1

        Battle.attack(MapUnitCombatant(attacker), MapUnitCombatant(defender))

        assertFalse(defender.isDestroyed)
        assertEquals(UnitHeroSystem.EXTRA_LIFE_SURVIVE_HP, defender.health)
        assertEquals(0, defender.hero.extraLives)
        assertEquals(0, attacker.hero.wins)
        assertEquals(HexCoord(1, 0), attacker.getTile().position)
    }

    @Test
    fun `extra life saves unit from lethal ranged blow`() {
        val attacker = testGame.addUnit("Artillery", civ1, testGame.getTile(2, 0))
        val defender = testGame.addUnit("Warrior", civ2, testGame.getTile(HexCoord.Zero))
        defender.health = 5
        defender.hero.heroTier = 1
        defender.hero.extraLives = 1

        Battle.attack(MapUnitCombatant(attacker), MapUnitCombatant(defender))

        assertFalse(defender.isDestroyed)
        assertEquals(UnitHeroSystem.EXTRA_LIFE_SURVIVE_HP, defender.health)
        assertEquals(0, defender.hero.extraLives)
        assertEquals(0, attacker.hero.wins)
    }

    @Test
    fun `combat kill in battle awards win to victor`() {
        val attacker = testGame.addUnit("Infantry", civ1, testGame.getTile(1, 0))
        val defender = testGame.addUnit("Warrior", civ2, testGame.getTile(HexCoord.Zero))
        defender.health = 5

        Battle.attack(MapUnitCombatant(attacker), MapUnitCombatant(defender))

        assertTrue(defender.isDestroyed)
        assertEquals(1, attacker.hero.wins)
    }

    @Test
    fun `copyStatisticsTo preserves hero data across upgrades`() {
        val oldUnit = testGame.addUnit("Warrior", civ1, testGame.getTile(HexCoord.Zero))
        oldUnit.hero.wins = 9
        oldUnit.hero.level = 3
        oldUnit.hero.heroTier = 1
        oldUnit.hero.bonusAttack = 2
        oldUnit.hero.bonusDefense = 1
        oldUnit.hero.bonusMovement = 1
        oldUnit.hero.extraLives = 1
        oldUnit.hero.noMaintenance = true

        val newUnit = testGame.addUnit("Swordsman", civ1, testGame.getTile(1, 0))
        oldUnit.copyStatisticsTo(newUnit)

        assertEquals(9, newUnit.hero.wins)
        assertEquals(3, newUnit.hero.level)
        assertEquals(1, newUnit.hero.heroTier)
        assertEquals(2, newUnit.hero.bonusAttack)
        assertEquals(1, newUnit.hero.bonusDefense)
        assertEquals(1, newUnit.hero.bonusMovement)
        assertEquals(1, newUnit.hero.extraLives)
        assertTrue(newUnit.hero.noMaintenance)
    }
}
