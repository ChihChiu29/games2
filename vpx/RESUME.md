# VPX (VPNX) Switch Port — Context & Status

## Current State
- **Current Version:** `0.1.2` (stored in `vpx/VERSION`; output NSP is always `vpx/release/vpnx.nsp`)
- **Backend:** OpenGL ES 3.2 (`vpx/vpnx/build/vpinball-gl.nro`, Mesa/nouveau).
  - *Note:* Upstream prebuilt Vulkan NRO (`vpinball-vk.nro` in `vpx/.cache/vpnx-0.1.0.zip`, using `nxvk`/NVK) crashes Eden's `Service.NVDRV` (`Unhandled SIGSEGV`) immediately upon NVK init, so GL is required for Eden compatibility.
- **Bundled Tables:** `Attack from Mars.vpxz`, `Medieval Madness.vpxz` (from `vpx/tables/`).
- **Versioning rule (user requirement):**
  - Bump the **patch** version (e.g. `0.1.2` → `0.1.3`) in `vpx/VERSION` on every fix.
  - Bump the **minor** version (e.g. `0.1.x` → `0.2.0`) on every new feature.
  - Always keep the output file named `vpx/release/vpnx.nsp` (no timestamp suffix).

## Build & Test Workflow
- **Incremental C++ rebuild (`vpinball-gl.nro`):**
  ```bash
  export DEVKITPRO=/home/chih/Workspace/git/games2/vpx/.devkitpro
  cmake --build /home/chih/Workspace/git/games2/vpx/vpnx/build/vpinball-switch -j"$(nproc)"
  cp -f /home/chih/Workspace/git/games2/vpx/vpnx/build/vpinball-switch/vpinball.nro /home/chih/Workspace/git/games2/vpx/vpnx/build/vpinball-gl.nro
  ```
  *(Note: `vpinball` itself is built without `-flto`, though linking `libpinmame.a` still takes ~3 minutes.)*
- **Package NSP (`vpx/release/vpnx.nsp`):**
  ```bash
  cd /home/chih/Workspace/git/games2/vpx && ./build.sh
  ```
  *(The forwarder in `vpx/vpnx/forwarder/main.c` syncs `romfs:/switch/vpnx/vpnx.nro` to `sdmc:/switch/vpnx/vpnx.nro` on launch.)*
- **Test in Eden (with OOM guardrail — IMPORTANT):**
  `eden-cli` uses ~3.4–3.6 GiB RSS (mostly `shmem`), which can trigger the Linux OOM killer on this 7.5 GiB RAM laptop and tear down the parent `tmux-spawn-*.scope`. **Always** run Eden inside an isolated systemd scope:
  ```bash
  systemd-run --user --scope -p OOMPolicy=kill -p MemoryHigh=4G -p MemoryMax=5G -- \
    env LOAD_TIMEOUT=90 PERF_WAIT=15 /home/chih/Workspace/git/games2/eden/test_vpxp.sh
  ```
  Check `free -h` before launching Eden.

## Completed Fixes (`v0.1.2`)
1. **Eliminated `AudioStreamPlayer::Create` spam & 100× CPU stall:**
   - Tables with missing PinMAME sound ROMs (e.g., Attack from Mars missing `afm_s2.l1`–`s4.l1`) disable sound and broadcast `sampleRate = 0` ~40–50×/sec, causing unbuffered (`_IONBF`) error writes to `sdmc:/switch/vpnx/stdio.log` through Eden's `Service.FS`.
   - Fixed in `vpx/vpnx/vpinball/src/audio/AudioStreamPlayer.cpp` and `vpx/vpnx/vpinball/src/core/player.cpp` (guard `sampleRate <= 0` / `channels <= 0`).
   - Buffered `stdio.log` (`_IOFBF`/`_IOLBF`) in `vpx/vpnx/vpinball/switch/main.cpp` when `nxlink` is not connected (only using `_IONBF` when `nxlinkConnectToHost` succeeds).
2. **First-run Switch performance defaults (`sdmc:/switch/vpnx/perf_defaults.txt`):**
   - Applied once in `ApplyDisplayDefaults()` (`vpx/vpnx/vpinball/switch/main.cpp`): `DisableAO=1`, `DynamicAO=0`, `PFReflection=2` (Static Only), `ForceBloomOff=1`, `ForceMotionBlurOff=1`.
   - User edits in the in-game menu or `VPinballX.ini` are preserved; delete `perf_defaults.txt` on SDMC to re-apply.
3. **Eden test harness (`eden/test_vpxp.sh`):**
   - Increased menu wait to 6s with retry of `A` until `stdio.log` appears, and added `PERF_WAIT` env var to capture steady-state `[perf]` lines.

## Verified Performance (Eden, 1080p Docked, Attack from Mars)
| Build / Config | Steady-State FPS | `prepare` | `submit` | `flip` | `draws` / `tris` |
|---|---:|---:|---:|---:|---:|
| `v0.1.1` baseline | 0.4–0.7 fps | 542–1207 ms | 112–195 ms | 467–596 ms | 1,019 / 202k |
| Audio + stdio fix only (default gfx) | 11.4 fps | 3.4 ms | 11.9 ms | 78.1 ms | 269 / 86k |
| **`v0.1.2` (audio/stdio + Switch defaults)** | **21.5–23.4 fps** | **2.0–2.5 ms** | **7.7–8.2 ms** | **35.2–39.5 ms** | **176 / 47.5k** |

- Physics runs at 100% real-time (`game-time ≈ real-time`), zero `AudioStreamPlayer` errors.

## Potential Next Steps / Open Items
- Test `Medieval Madness` in Eden (and verify sound ROMs / performance on a second table).
- Test on real Switch hardware (handheld 720p and docked 1080p) and verify controller/flipper input, ScoreView DMD placement, and exit combo (`+`/`-`).
- Optional: supply complete `afm_113b.zip` sound ROMs (`afm_s2.l1`–`s4.l1`) if ROM audio is desired for Attack from Mars.
