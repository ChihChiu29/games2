# Demon's Souls (PS5 Remake / KytyPS5) Natural HP Regen Patch

Hooks `DemonsSoul::ChrInsMan::_UpdateChr_ByListsPre` (`0x34DA9E`) in Demon's Souls PS5's decrypted `eboot.bin` to naturally regenerate **~2% Max HP / sec** (at 60fps; ~1%/sec at 30fps) while alive (`hp > 0 && hp < maxHp`).

## Usage (Windows or Linux, Python 3 only — zero extra packages)
```bash
python release/patch_demonsouls_eboot.py "C:\path\to\DemonsSouls\eboot.bin"
```
- Automatically creates `eboot.bin.bak` before patching.
- To revert:
```bash
python release/patch_demonsouls_eboot.py --restore "C:\path\to\DemonsSouls\eboot.bin"
```
