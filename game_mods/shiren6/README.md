# Shiren the Wanderer 6 Plus (Android APK) — QoL Mod

Patches for *Shiren the Wanderer: The Mystery Dungeon of Serpentcoil Island Plus* (`com.spikechunsoft.shirenthewanderer6plus.ww`, Unity IL2CPP v31, ARM64).

## Features

1. **3x Gold (Gitan) Earn** — Inline patch in `UserState::AddGitan` (`.bsf::plm` @ `0x031F0528`, replacing the redundant duplicate static-init block at `0x031F0578..0x031F0587`) multiplying positive Gitan deltas (`w20 > 0`) by **3x** while leaving spending (`w20 <= 0`) untouched.
2. **3x Backpack Size (72 Slots)** — Patches `F7.Scripts.Dungeon.ItemBag..ctor` (`0x033D151C`: `mov w1, #72`) and hooks `ItemBag::uoe` (`0x033D1514` → `0x031F0588`) so both new runs and loaded saves automatically expand to **72 slots**.
3. **4KB-Aligned & APK Signature Scheme v2 Signed** — Repacked with `game_mods/common/apk_v2_sign.py`.

## Building

```bash
cd game_mods/shiren6
./build.sh
# Or customize:
python3 patch_shiren6_apk.py --gold-mult 3 --bag-size 72
```

Output: `release/Shiren6Plus_mod<VERSION>.apk`.

## Installing on Android

Uninstall the existing Shiren 6 app first (back up your save if needed), then install `release/Shiren6Plus_mod<VERSION>.apk`.

## Changelog

- **1.0** — Initial release: 3x Gold (Gitan) earn + 3x Backpack size (72 slots, auto-upgrading saves).
