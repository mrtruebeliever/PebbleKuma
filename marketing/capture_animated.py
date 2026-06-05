"""
Capture PebbleKuma navigation with slide animations via QEMU screendump.
Run AFTER a fresh `pebble install --emulator emery` of the slow-animation build.
"""
import json, os, socket, subprocess, time, shutil

PEBBLE = os.path.expanduser("~/.local/bin/pebble")
STATE_FILE = "/tmp/pb-emulator.json"
OUT_DIR = "/tmp/gif_anim_frames"
INTERVAL = 0.08   # seconds between screendumps (= 80ms = ~12.5 fps capture rate)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def get_monitor_port():
    with open(STATE_FILE) as f:
        d = json.load(f)
    return list(list(d.values())[0].values())[0]["qemu"]["monitor"]

def connect_monitor(port):
    s = socket.socket()
    s.connect(("127.0.0.1", port))
    s.settimeout(0.5)
    try: s.recv(65536)   # drain banner
    except: pass
    return s

def screendump(sock, path):
    cmd = f"screendump {path}\n"
    sock.sendall(cmd.encode())
    time.sleep(0.04)   # let QEMU write the file
    try: sock.recv(4096)
    except: pass

def press(btn, emulator="emery"):
    subprocess.run(
        [PEBBLE, "emu-button", "click", btn, "--emulator", emulator],
        capture_output=True, timeout=5
    )

# ---------------------------------------------------------------------------
# Capture loop
# ---------------------------------------------------------------------------

os.makedirs(OUT_DIR, exist_ok=True)
# clean old frames
for f in os.listdir(OUT_DIR):
    os.remove(os.path.join(OUT_DIR, f))

port = get_monitor_port()
print(f"QEMU monitor port: {port}")
sock = connect_monitor(port)

frame_i = 0

def shoot(duration, interval=INTERVAL):
    global frame_i
    end = time.time() + duration
    while time.time() < end:
        p = os.path.join(OUT_DIR, f"f_{frame_i:04d}.ppm")
        screendump(sock, p)
        frame_i += 1
        remaining = end - time.time()
        sleep = min(interval, remaining)
        if sleep > 0:
            time.sleep(sleep)

print("Phase 1: list view (hold 2s)...")
shoot(2.0)

print("Phase 2: press DOWN to reach first monitor row...")
press("down")
shoot(0.4)

print("Phase 3: SELECT → open Web Server (DOWN) detail + animation (1.5s)...")
press("select")
shoot(1.5)

print("Phase 4: hold on DOWN card (1.5s)...")
shoot(1.5)

print("Phase 5: DOWN → Database (PENDING) + animation (1.5s)...")
press("down")
shoot(1.5)

print("Phase 6: hold on PENDING card (1.2s)...")
shoot(1.2)

print("Phase 7: DOWN → API Gateway (UP) + animation (1.5s)...")
press("down")
shoot(1.5)

print("Phase 8: hold on UP card (1.2s)...")
shoot(1.2)

print("Phase 9: DOWN → VPN (MAINT) + animation (1.5s)...")
press("down")
shoot(1.5)

print("Phase 10: hold on MAINT card (1.2s)...")
shoot(1.2)

print("Phase 11: BACK → list (animation + settle 1.5s)...")
press("back")
shoot(1.5)

print("Phase 12: hold on list view (1.5s)...")
shoot(1.5)

sock.close()
print(f"\nCaptured {frame_i} raw frames to {OUT_DIR}")
