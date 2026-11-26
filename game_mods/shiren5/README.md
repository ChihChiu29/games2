# Shiren the Wanderer 5 (NDS, `B5FJ`) — melonDS Boot Fix & QoL Mod

Patches for the Chinese fan-translated Nintendo DS ROM of *Shiren the Wanderer 5: The Tower of Fortune and the Dice of Fate* (`B5FJ`).

## What This Patch Does

1. **Fixes melonDS / Real-Hardware Boot Hang**
   - **Root cause:** The fan translation injected a custom Chinese font loader at `ARM9` `0x020D8786..0x020D87F4` that calls `CARDi_ReadRomSyncCore` (`0x0209FF2D`) directly without granting ARM9 Slot-1 cartridge ownership in `REG_EXMEMCNT` (`0x04000204`, bit 11). DeSmuME ignores `EXMEMCNT` ownership, whereas melonDS and real NDS hardware return `0` for `REG_ROMCTRL`, causing an infinite poll loop inside `CARDi_ReadRomIDCore` (`0x0209FE3E`).
   - **Fix:** Installs a Thumb-1 wrapper at `0x020D8834` (redirected from `0x020D8830`) that clears bit 11 of `REG_EXMEMCNT` before calling `CARDi_ReadRomSyncCore` and restores it afterwards. Also pads the ROM to 64 MiB (`0x04000000`) with `0xFF` and recalculates both Secure Area CRC16 (`0x06C`) and Header CRC16 (`0x15E`).

2. **Disables Night Mode (Always Day)**
   - **`InitFloorDayNight` (`0x02025FDC`):** At `0x02026084` (file `0x02A084`), patches `ldrb r0, [r0]` (`00 78`) → `movs r0, #0` (`00 20`), forcing every floor into Mode `0` (Always Day) with daytime lighting and the Sun/Moon HUD gauge cleanly hidden.
   - **`UpdateDayNight` (`0x0202E318`):** At `0x0202E32C` (file `0x03232C`), patches `ldrb r1, [r5, r0]` (`29 5c`) → `movs r1, #0` (`00 21`), so daytime never ticks toward Dusk/Night. Existing saves saved mid-Night will naturally tick down to Dawn, switch back to Day, and remain Day permanently.

3. **3x Gold (Gitan) Earn**
   - **`OV9[14]` `DungeonCharacter::AddGitan` (`0x0217814C`):** Hooks positive Gitan deltas (`r1 > 0`) via ARM9 helper `0x020D8860` to multiply earned Gitan by `3x` (picking up Gitan, selling items in dungeon shops, using Gitan bags). Spending (`r1 < 0`) is untouched. `OV9[14]` is stored uncompressed at the end of the ROM (`flags &= ~0x01FFFFFF`).
   - **`ARM9` `SaveData::AddCarriedGitan` (`0x02019F6C`):** Hooks positive Gitan deltas via `0x020D8870` (`3x`). Bank withdrawals (`0x0208DCB8`) are untouched.

4. **3x Backpack Size (72 Slots / 9 Pages)**
   - Expands `ItemBag` from `24` slots (3 pages) to `72` slots (9 pages) — still within `ItemMenu`'s native `80`-slot pagination used by `ItemWarehouse`.
   - Allocates `0x19C` bytes (`0x7C + 72 * 4`) via `0x020D8880` at both `new ItemBag` call sites (`0x0203C3E0`, `0x0203C584`), keeping `ItemBag + 0x78` untouched and pointing `ItemContainer`'s pointer array (`0x0204812A`) to `this + 0x7C` with capacity `72`.
   - Auto-upgrades existing 24-slot saves on load (`0x02049450` → `0x020D8890`) and bounds-checks equipped bracelet indices (`0x020494D0` → `0x020D88A0`, serialize sentinel `0xFF` at `0x0204957A` / `0x02049580`) for full save compatibility.

## Building

Place the source fan-translated ROM at `game_mods/shiren5/shiren5.nds`, then run:

```bash
cd game_mods/shiren5
./build.sh
```

Output is written to `release/shiren5_mod<VERSION>.nds`. Custom options are also supported:

```bash
# Example: 2x backpack (48 slots), 3x gold, keep night mode
python3 patch_shiren5.py shiren5.nds release/shiren5_custom.nds --bag-size 48 --gold-mult 3 --keep-night
```

## Changelog

- **1.1** — Added 3x Gold (Gitan) earn and 3x Backpack size (72 slots, auto-upgrading existing saves).
- **1.0** — Initial release: melonDS `EXMEMCNT` boot fix + No-Night-Mode patch.
