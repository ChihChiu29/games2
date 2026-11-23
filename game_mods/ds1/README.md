# Dark Souls Remastered (PC) Natural HP Regen Mod

Drop-in Windows x64 proxy DLL (`dinput8.dll`) built against verified `DarkSoulsRemastered.exe` offsets.

## Features
- **Natural HP Regeneration (~2% Max HP / sec)** while alive (`hp > 0 && hp < maxHp`).
- Press **F5** in-game to toggle on/off.
- Configurable via `ds1_regen.ini`; logs to `ds1_regen.log`.

## Installation
1. Copy `release/dinput8.dll` and `release/ds1_regen.ini` into your Dark Souls Remastered folder (next to `DarkSoulsRemastered.exe`).
2. Launch the game in **Offline Mode** (recommended for any mod).
3. To uninstall, delete `dinput8.dll` (and `ds1_regen.ini` / `ds1_regen.log`).

## Building
```bash
./build.sh
```
*(You do not need `DarkSoulsRemastered.exe` present to rebuild.)*
