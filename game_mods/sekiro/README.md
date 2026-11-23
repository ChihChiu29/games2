# Sekiro (v1.06) Natural HP Regen & Level-Up Roll Mod

Drop-in Windows x64 proxy DLL (`dinput8.dll`) built against verified `sekiro.exe` v1.06 offsets.

## Features
1. **Natural HP Regeneration (~2% Max HP / sec)**
   - Continuously regenerates HP while alive (`hp > 0 && hp < maxHp`), matching our Dark Souls Remastered mod.
   - Press **F5** in-game to toggle on/off.
2. **Random Level-Up Upgrade (every Skill Point earned)**
   - Whenever your XP bar fills and grants a Skill Point, the mod automatically rolls **50/50** (without consuming your Skill Point):
     - **+1 Attack Power** (native `PlayerGameData` stat, capped at 99, persisted in your Sekiro save file).
     - **+5% Max HP** (applied on top of your base Max HP, even past Vitality 20, persisted in `sekiro_regen_state.ini`).
   - Press **F6** in-game to toggle on/off.
   - Distinct beep cues (high chime = Attack Power, lower chime = Max HP) and a log in `sekiro_regen.log`.

## Installation
1. Copy `release/dinput8.dll` and `release/sekiro_regen.ini` into your Sekiro game folder (the same folder as `sekiro.exe`).
2. Launch Sekiro normally.
3. *(Optional)* Edit `sekiro_regen.ini` to adjust regen speed, Attack/HP roll chance, HP bonus %, sound cues, or chain another `dinput8.dll` (such as Sekiro Mod Engine).
4. To uninstall, just delete `dinput8.dll` (and `sekiro_regen*.ini` / `sekiro_regen.log`).

## Building
```bash
./build.sh
```
