#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NSP="${1:-$DIR/../vpx/release/vpnx.nsp}"
BACKEND="${EDEN_BACKEND:-1}" # 1=Vulkan (default), 0=OpenGL
OUT_DIR="${OUT_DIR:-/tmp/vpnx_test}"
LOAD_TIMEOUT="${LOAD_TIMEOUT:-45}"

if [ ! -f "$NSP" ]; then
    echo "ERROR: NSP not found: $NSP" >&2
    exit 1
fi

export DISPLAY="${DISPLAY:-:0}"
if [ -z "${XAUTHORITY:-}" ]; then
    XAUTH="$(ls "/run/user/$(id -u)"/.mutter-Xwaylandauth.* 2>/dev/null | head -n 1 || true)"
    [ -n "$XAUTH" ] && export XAUTHORITY="$XAUTH"
fi

SDMC_VPNX="${HOME}/.local/share/eden/sdmc/switch/vpnx"
EDEN_CFG="${HOME}/.config/eden/sdl2-config.ini"
CFG_BACKUP=""
EPID=""

cleanup() {
    if [ -n "$EPID" ]; then
        kill "$EPID" 2>/dev/null || true
        wait "$EPID" 2>/dev/null || true
    fi
    if [ -n "$CFG_BACKUP" ] && [ -f "$CFG_BACKUP" ]; then
        mv -f "$CFG_BACKUP" "$EDEN_CFG"
    fi
}
trap cleanup EXIT

mkdir -p "$OUT_DIR"
rm -f "$OUT_DIR/menu.png" "$OUT_DIR/play.png" "$OUT_DIR/eden.log"
rm -f "$SDMC_VPNX/stdio.log" "$SDMC_VPNX/vpinball.log" "$SDMC_VPNX/exception_dump.txt" "$SDMC_VPNX/terminate.txt"

if [ -f "$EDEN_CFG" ]; then
    CFG_BACKUP="$(mktemp /tmp/eden_cfg_backup.XXXXXX)"
    cp "$EDEN_CFG" "$CFG_BACKUP"
    sed -i -e 's/^backend\\default=.*/backend\\default=false/' -e "s/^backend=.*/backend=${BACKEND}/" "$EDEN_CFG"
fi

echo "==> Testing $NSP in Eden (backend=$BACKEND, DISPLAY=$DISPLAY)..."
"$DIR/eden-cli" -g "$NSP" > "$OUT_DIR/eden.log" 2>&1 &
EPID=$!

python3 - "$EPID" "$SDMC_VPNX/vpinball.log" "$OUT_DIR" "$LOAD_TIMEOUT" <<'PY'
import ctypes, os, struct, sys, time, zlib

epid = int(sys.argv[1])
vpinball_log = sys.argv[2]
out_dir = sys.argv[3]
load_timeout = int(sys.argv[4])

x11 = ctypes.CDLL("libX11.so.6")
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XDefaultRootWindow.restype = ctypes.c_ulong
x11.XGetImage.restype = ctypes.c_void_p

class XWindowAttributes(ctypes.Structure):
    _fields_ = [
        ("x", ctypes.c_int), ("y", ctypes.c_int), ("width", ctypes.c_int), ("height", ctypes.c_int),
        ("border_width", ctypes.c_int), ("depth", ctypes.c_int), ("visual", ctypes.c_void_p),
        ("root", ctypes.c_ulong), ("c_class", ctypes.c_int), ("bit_gravity", ctypes.c_int),
        ("win_gravity", ctypes.c_int), ("backing_store", ctypes.c_int), ("backing_planes", ctypes.c_ulong),
        ("backing_pixel", ctypes.c_ulong), ("save_under", ctypes.c_int), ("colormap", ctypes.c_ulong),
        ("map_installed", ctypes.c_int), ("map_state", ctypes.c_int), ("all_event_masks", ctypes.c_long),
        ("your_event_mask", ctypes.c_long), ("do_not_propagate_mask", ctypes.c_long), ("override_redirect", ctypes.c_int),
        ("screen", ctypes.c_void_p),
    ]

class XKeyEvent(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_int), ("serial", ctypes.c_ulong), ("send_event", ctypes.c_int),
        ("display", ctypes.c_void_p), ("window", ctypes.c_ulong), ("root", ctypes.c_ulong),
        ("subwindow", ctypes.c_ulong), ("time", ctypes.c_ulong),
        ("x", ctypes.c_int), ("y", ctypes.c_int), ("x_root", ctypes.c_int), ("y_root", ctypes.c_int),
        ("state", ctypes.c_uint), ("keycode", ctypes.c_uint), ("same_screen", ctypes.c_int),
    ]

class XEvent(ctypes.Union):
    _fields_ = [("xkey", XKeyEvent), ("pad", ctypes.c_long * 24)]

class XImage(ctypes.Structure):
    _fields_ = [
        ("width", ctypes.c_int), ("height", ctypes.c_int), ("xoffset", ctypes.c_int), ("format", ctypes.c_int),
        ("data", ctypes.c_void_p), ("byte_order", ctypes.c_int), ("bitmap_unit", ctypes.c_int),
        ("bitmap_bit_order", ctypes.c_int), ("bitmap_pad", ctypes.c_int), ("depth", ctypes.c_int),
        ("bytes_per_line", ctypes.c_int), ("bits_per_pixel", ctypes.c_int),
    ]

def walk_wins(dpy, w, out):
    name = ctypes.c_char_p()
    x11.XFetchName(dpy, w, ctypes.byref(name))
    nm = name.value.decode("utf-8", "ignore") if name.value else ""
    attr = XWindowAttributes()
    x11.XGetWindowAttributes(dpy, w, ctypes.byref(attr))
    out.append((w, nm, attr.width, attr.height, attr.map_state))
    r_ret, p_ret = ctypes.c_ulong(), ctypes.c_ulong()
    children = ctypes.POINTER(ctypes.c_ulong)()
    n = ctypes.c_uint()
    if x11.XQueryTree(dpy, w, ctypes.byref(r_ret), ctypes.byref(p_ret), ctypes.byref(children), ctypes.byref(n)):
        for i in range(n.value):
            walk_wins(dpy, children[i], out)

def send_key(dpy, root, win, keysym):
    kc = x11.XKeysymToKeycode(dpy, keysym)
    for ev_type, mask in ((2, 1), (3, 2)):
        ev = XEvent()
        ev.xkey.type = ev_type
        ev.xkey.display = dpy
        ev.xkey.window = win
        ev.xkey.root = root
        ev.xkey.keycode = kc
        ev.xkey.same_screen = 1
        x11.XSendEvent(dpy, win, True, mask, ctypes.byref(ev))
        x11.XFlush(dpy)
        time.sleep(0.15)

def save_png(path, w, h, rgb):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    raw = b"".join(b"\x00" + rgb[y*w*3:(y+1)*w*3] for y in range(h))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)

def snap(dpy, win, w, h, path):
    img_p = x11.XGetImage(dpy, win, 0, 0, w, h, 0xffffffff, 2)
    if not img_p:
        return 0, b""
    img = ctypes.cast(img_p, ctypes.POINTER(XImage)).contents
    buf = ctypes.string_at(img.data, img.height * img.bytes_per_line)
    rgb = bytearray(img.width * img.height * 3)
    # Count non-black pixels inside the game viewport (skip top 80px titlebar)
    viewport_nonblack = 0
    for y in range(img.height):
        row = buf[y*img.bytes_per_line : y*img.bytes_per_line + img.width*4]
        for x in range(img.width):
            b, g, r = row[x*4], row[x*4+1], row[x*4+2]
            o = (y*img.width + x)*3
            rgb[o], rgb[o+1], rgb[o+2] = r, g, b
            if y >= 80 and (r or g or b):
                viewport_nonblack += 1
    data = bytes(rgb)
    save_png(path, img.width, img.height, data)
    return viewport_nonblack, data

def pid_alive(pid):
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False

dpy = x11.XOpenDisplay(None)
if not dpy:
    print("FAIL: Cannot open X display", file=sys.stderr)
    sys.exit(1)
root = x11.XDefaultRootWindow(dpy)

# 1. Wait for Eden window
target = None
for _ in range(20):
    wins = []
    walk_wins(dpy, root, wins)
    eden_wins = [x for x in wins if "Eden" in x[1] and x[4] == 2]
    if eden_wins:
        target = eden_wins[-1]
        break
    time.sleep(0.5)

if not target:
    print("FAIL: Eden window did not appear", file=sys.stderr)
    sys.exit(1)

time.sleep(6)
menu_nb, menu_rgb = snap(dpy, target[0], target[2], target[3], os.path.join(out_dir, "menu.png"))
print(f"Menu screenshot: {out_dir}/menu.png (viewport non-black={menu_nb})", flush=True)

# 2. Press A to launch the selected table
sub = []
walk_wins(dpy, target[0], sub)
for s in sub:
    send_key(dpy, root, s[0], 0x0061)

# 3. Wait for VPX to reach "playing" and emit at least one [perf] line
reached_playing = False
deadline = time.time() + load_timeout
stdio_log = os.path.join(os.path.dirname(vpinball_log), "stdio.log")
last_retry = time.time()
while time.time() < deadline:
    if not pid_alive(epid):
        print("FAIL: Eden process exited unexpectedly", file=sys.stderr)
        sys.exit(1)
    if not os.path.isfile(stdio_log) and time.time() - last_retry > 3.0:
        for s in sub:
            send_key(dpy, root, s[0], 0x0061)
        last_retry = time.time()
    if os.path.isfile(vpinball_log):
        txt = open(vpinball_log, "r", errors="ignore").read()
        if "[perf]" in txt:
            reached_playing = True
            break
    time.sleep(1)

if not reached_playing:
    print(f"FAIL: Table did not reach gameplay within {load_timeout}s", file=sys.stderr)
    sys.exit(1)

time.sleep(int(os.environ.get("PERF_WAIT", "3")))
play_nb, play_rgb = snap(dpy, target[0], target[2], target[3], os.path.join(out_dir, "play.png"))
print(f"Play screenshot: {out_dir}/play.png (viewport non-black={play_nb})", flush=True)
x11.XCloseDisplay(dpy)

if play_nb < 50000:
    print(f"FAIL: Gameplay screen appears blank (viewport non-black={play_nb})", file=sys.stderr)
    sys.exit(1)
if play_rgb == menu_rgb:
    print("FAIL: Gameplay screenshot is identical to menu screenshot", file=sys.stderr)
    sys.exit(1)
PY

if [ -f "$SDMC_VPNX/exception_dump.txt" ] || [ -f "$SDMC_VPNX/terminate.txt" ]; then
    echo "FAIL: Crash dump detected in $SDMC_VPNX" >&2
    exit 1
fi

echo "==> BGFX / Window status:"
grep -E "Window #0|BGFX initialized" "$SDMC_VPNX/stdio.log" || true
echo "==> Perf:"
grep "\[perf\]" "$SDMC_VPNX/vpinball.log" || true
echo "PASS: $NSP rendered menu and gameplay cleanly!"
