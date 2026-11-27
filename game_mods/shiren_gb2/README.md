# Shiren the Wanderer GB2: Desert Magic Castle (GBC, `AFMJ`) — QoL Mod

Patches for the Game Boy Color ROM of *Shiren the Wanderer GB2* (`AFMJ`).

## Features

1. **3x Gold (Gitan) Earn** — Hooks `Bank 0x07:0x5691` (`AddCarriedGitan`, file `0x1D691`) via a 28-byte SM83 helper at `Bank 0x07:0x7F00` (`file 0x1FF00`) that adds the positive 24-bit Gitan delta (`HRAM[0xFFB0..0xFFB2]`) to carried Gitan (`WRAM Bank 1 [0xD2A9..0xD2AC]`) **3 times** before falling into the game's native `999,999` clamp. Recalculates the GBC Global Checksum (`0x014E..0x014F`).

> **Notes:**
> - Shiren GB2 does not have Night Mode or L/R buttons (GBC hardware).
> - Like Shiren 1 DS, Shiren GB2's 20-slot inventory is a fixed WRAM/SRAM array, so backpack expansion was omitted to keep saves intact.

## Building

```bash
cd game_mods/shiren_gb2
./build.sh
```

Output: `release/shiren_gb2_mod<VERSION>.gbc`.

## Changelog

- **1.0** — Initial release: 3x Gold (Gitan) earn + GBC Global Checksum update.
