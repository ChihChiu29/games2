# Shiren the Wanderer DS (Shiren 1, `AFUJ`) — QoL Mod

Patches for the Chinese fan-translated Nintendo DS ROM of *Mystery Dungeon: Shiren the Wanderer DS* (`AFUJ`).

## Features

1. **3x Gold (Gitan) Earn** — Hooks `ARM9` `AddGitan` (`0x02097774` → `0x02135450`) to multiply positive Gitan gains (`delta > 0`) by **3x** while leaving spending (`delta < 0`) untouched.
2. **64 MiB Power-of-2 Padding & Header CRC16** — Pads the 48 MiB ROM cleanly to 64 MiB (`0x04000000`) with `0xFF` and recalculates Header CRC16 (`0x15E`).

> **Notes:**
> - Shiren 1 DS does not have Night Mode.
> - Unlike Shiren 4/5's heap-allocated `ItemBag` pointer array, Shiren 1 DS embeds its 20-slot inventory (`20 × 50 B`) directly inside the fixed player/save struct with 50+ hardcoded `cmp #0x14` loops, so backpack expansion was intentionally omitted to avoid save/UI corruption.

## Building

```bash
cd game_mods/shiren1
./build.sh
```

Output: `release/shiren1_mod<VERSION>.nds`.

## Changelog

- **1.0** — Initial release: 3x Gold (Gitan) earn + 64 MiB padding & CRC16.
