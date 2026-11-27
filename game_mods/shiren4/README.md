# Shiren the Wanderer 4 (NDS, `B4FJ`) — melonDS Fix & QoL Mod

Patches for the Chinese fan-translated Nintendo DS ROM of *Shiren the Wanderer 4: The Eye of God and the Devil's Navel* (`B4FJ`).

## Features

1. **melonDS / Real-Hardware Boot Fix** — Wraps the fan translation's custom ARM ROM reader (`0x020DFB50`) via `0x020CA080` to clear/restore `REG_EXMEMCNT` bit 11 (ARM9 Slot-1 ownership), pads to 64 MiB, and fixes Secure/Header CRC16s.
2. **No Night Mode (Always Day)** — Forces Mode `0` (Always Day) in `InitFloorDayNight` (`0x0202255E`) and `UpdateDayNight` (`0x0202844C`). Existing Night saves naturally tick back to Day and stay Day.
3. **3x Gold (Gitan) Earn** — Hooks `OV9[2]` `DungeonCharacter::AddGitan` (`0x02143AA4` → `0x020CA0B0`) and `ARM9` `SaveData::AddCarriedGitan` (`0x020186EA` → `0x020CA0C0`) for positive gains only (`delta > 0`).
4. **3x Backpack Size (72 Slots / 9 Pages)** — Expands `ItemBag` from 24 → 72 slots (`0x0203481C`, `0x0203CD94`) with automatic save upgrade (`0x0203D76A`) and bounds-checked bracelet deserialization (`0x0203D84A`, `0x0203D8D4`).
5. **Instant Conversation & Opening/Cutscene Skip** — Hold **L**, **R**, or **X** to skip any conversation with 1-frame box pauses (`0x0201C3AA`, `0x0201C3F0`, `0x020555FA`/`0x02055782`/`0x020559CC`/`0x02055D94`), and hold **L** or **R** outside dungeons to fast-forward opening/cutscene `WaitFrames` (`0x02008F62` → `0x020CA0F0`).

## Building

```bash
cd game_mods/shiren4
./build.sh
```

Output: `release/shiren4_mod<VERSION>.nds`.

## Changelog

- **1.0** — Initial release: melonDS `EXMEMCNT` boot fix, No Night Mode, 3x Gold, 3x Backpack (72 slots), and instant L/R/X conversation & opening/cutscene skip.
