# Dark Souls III (PC) Natural HP Regen Mod

Drop-in Windows x64 proxy DLL (`dinput8.dll`) built against verified `DarkSoulsIII.exe` offsets.

## Features
- **Natural HP Regeneration (~2% Max HP / sec)** while alive (`hp > 0 && hp < maxHp`).
- Press **F5** in-game to toggle on/off.
- Configurable via `ds3_regen.ini`; logs to `ds3_regen.log`.

## Installation
1. Copy `release/dinput8.dll` and `release/ds3_regen.ini` into your Dark Souls III `Game/` folder (next to `DarkSoulsIII.exe`).
2. **Play in Offline Mode** to avoid online softbans when using mods.
3. To uninstall, delete `dinput8.dll` (and `ds3_regen.ini` / `ds3_regen.log`).

## Building
```bash
./build.sh
```
*(You do not need `DarkSoulsIII.exe` present to rebuild.)*
