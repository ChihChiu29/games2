# Slay the Spire 2 (Android APK v0.111.0) Mod

Version is tracked in `VERSION` (currently `1.2`).

## Changelog
- **1.1:** 3x Gold (`Hook.ModifyGoldGained` relocated to `0x94FAC0`).
- **1.2:** Added **5 starting potion slots** (`Player.CreateForNewRun` @ `0x114D56`) and **6 cards drawn per turn** (`Hook.ModifyHandDraw` caller @ `0x3F5181`).

## Built APK
- `release/SlayTheSpire2_v0.111.0_mod1.2.apk` (3x Gold, 5 starting potion slots, 6 cards/turn; 4KB-aligned & APK v2 signed)

## Rebuild with Custom Values
```bash
python3 patch_sts2_apk.py --mult 3 --potions 5 --draw 6
```

## Installing on Android
Uninstall the existing StS2 app first (back up your save if needed), then install `release/SlayTheSpire2_v0.111.0_mod1.2.apk`.
