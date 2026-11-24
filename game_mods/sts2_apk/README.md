# Slay the Spire 2 (Android APK v0.111.0) Gold Multiplier Mod

Patches `MegaCrit.Sts2.Core.Hooks.Hook.ModifyGoldGained` inside `assets/payload/SlayTheSpire2.zip` → `data_sts2_windows_x86_64/sts2.dll` (relocated to `.text` padding at `RVA 0x94FAC0`, multiplying the returned `System.Decimal` by `mult`), repacks with 4KB alignment, and signs with **APK Signature Scheme v2** (Android 7.0–16+).

## Built APK
- `release/SlayTheSpire2_v0.111.0_3xGold.apk` (3x Gold)

## Rebuild for any multiplier (e.g. 3x or 5x)
```bash
python3 patch_sts2_apk.py --mult 5
```

## Installing on Android
Because the APK is re-signed with a new key, uninstall the existing StS2 app first (back up your save if needed), then install `release/SlayTheSpire2_v0.111.0_3xGold.apk`.
