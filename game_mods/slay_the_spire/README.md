# Slay the Spire (PC / `desktop-1.0.jar`) Configurable Mod

Version is tracked in `VERSION` (currently `1.2`).

## Changelog
- **1.1:** Configurable `gold_multiplier` (`AbstractPlayer.gainGold`).
- **1.2:** Added configurable `starting_potion_slots` (default `5`) and `cards_per_turn` (default `6`) via `AbstractPlayer.initializeClass`, plus versioned `release/desktop-<VERSION>.jar` output.

## Settings (`sts_gold.ini`)
- **`gold_multiplier = 3.0`** — multiplies all positive gold gains (Ectoplasm & negative amounts still behave normally).
- **`starting_potion_slots = 5`** — starting potion slots on new runs (vanilla = 3; max 12).
- **`cards_per_turn = 6`** — base cards drawn per turn (vanilla = 5; max 10).

## Installation & Usage
1. Copy `release/desktop-1.2.jar` into your Slay the Spire folder and rename it to **`desktop-1.0.jar`** (back up your original `desktop-1.0.jar` first!), and copy `release/sts_gold.ini` next to it.
   *(Or run `python release/patch_sts_jar.py "C:\path\to\desktop-1.0.jar"` to patch your existing jar in-place with an automatic `.bak` backup.)*
2. Edit `sts_gold.ini` anytime before launching—no need to re-patch!
3. Check `sts_gold.log` in the game folder to confirm `[StS Mod v1.2]` loaded your settings.

## Building
Update `VERSION` and run:
```bash
./build.sh
```
