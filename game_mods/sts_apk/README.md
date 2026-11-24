# Slay the Spire (Android APK v2.6.1) Gold Multiplier Mod

Patches `AbstractPlayer::gainGold` (`_ZN3com8megacrit9cardcrawl10characters29AbstractPlayer$_gainGold_VoidEPNS2_14AbstractPlayerEi`, file offset `0x27248D0`) inside `lib/arm64-v8a/libSpire_ANDROID.so` with a single ARM64 multiply instruction, repacks with 4KB `.so` alignment, and signs with **APK Signature Scheme v2** (Android 7.0–16+).

## Built APK
- `release/SlayTheSpire_v2.6.1_3xGold.apk` (3x Gold)

## Rebuild for 2x, 3x, or 5x Gold
```bash
python3 patch_sts_apk.py --mult 5
```

## Installing on Android
Because the APK is re-signed with a new key, uninstall the existing StS app first (back up your save if needed), then install `release/SlayTheSpire_v2.6.1_3xGold.apk`.
