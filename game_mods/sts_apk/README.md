# Slay the Spire (Android APK v2.6.1) Mod

Version is tracked in `VERSION` (currently `1.2`).

## Changelog
- **1.1:** 3x Gold (`AbstractPlayer::gainGold` @ `0x27248D0`).
- **1.2:** Added **5 starting potion slots** (`AbstractPlayer::<init>` @ `0x271F230`) and **6 cards drawn per turn** (`AbstractPlayer::initializeClass` @ `0x2720088`).

## Built APK
- `release/SlayTheSpire_v2.6.1_mod1.2.apk` (3x Gold, 5 starting potion slots, 6 cards/turn; 4KB-aligned & APK v2 signed)

## Rebuild with Custom Values
```bash
python3 patch_sts_apk.py --mult 3 --potions 5 --draw 6
```

## Installing on Android
Uninstall the existing StS app first (back up your save if needed), then install `release/SlayTheSpire_v2.6.1_mod1.2.apk`.
