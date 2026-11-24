# Slay the Spire (PC / `desktop-1.0.jar`) Configurable Gold Multiplier Mod

Patches `AbstractPlayer.gainGold(int)` inside `desktop-1.0.jar` to multiply all positive gold gains by `gold_multiplier` from `sts_gold.ini` (default `3.0`). No ModTheSpire or extra launcher required.

## Installation & Usage
1. Copy `release/sts_gold.ini` into your Slay the Spire folder (next to `desktop-1.0.jar`) and edit `gold_multiplier = 3.0` to whatever you want (e.g., `3.0`, `5.0`).
2. Run the patcher once (Python 3 stdlib only, works on Windows & Linux):
   ```bash
   python release/patch_sts_jar.py "C:\path\to\SlayTheSpire\desktop-1.0.jar"
   ```
   - Automatically creates `desktop-1.0.jar.bak` before patching.
3. Launch Slay the Spire normally. Change `sts_gold.ini` anytime before launching to change the multiplier (no need to re-patch!).
4. To uninstall/revert:
   ```bash
   python release/patch_sts_jar.py --restore "C:\path\to\SlayTheSpire\desktop-1.0.jar"
   ```
