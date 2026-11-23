# Bloodborne (Lantern Engine / shadPS4 v1.09) Natural HP Regen Patch

Hooks `CSChrUpdatePostFragment::Execute` (`0x1A66B10`) in Bloodborne's decrypted `eboot.bin` to naturally regenerate **~2% Max HP / sec** (at 60fps; ~1%/sec at 30fps) while alive (`hp > 0 && hp < maxHp`).

## Usage (Windows or Linux, Python 3 only — zero extra packages)
```bash
python release/patch_bloodborne_eboot.py "C:\path\to\Bloodborne\eboot.bin"
```
- Automatically creates `eboot.bin.bak` before patching.
- To revert:
```bash
python release/patch_bloodborne_eboot.py --restore "C:\path\to\Bloodborne\eboot.bin"
```
