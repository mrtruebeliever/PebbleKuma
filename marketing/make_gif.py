"""Assemble PebbleKuma navigation screenshots into an animated GIF."""
from PIL import Image
import os

FRAMES_DIR = os.path.join(os.path.dirname(__file__), "gif_frames")
OUT = os.path.join(os.path.dirname(__file__), "pebblekuma_demo.gif")

# Ordered frames with per-frame display duration in milliseconds.
FRAMES = [
    ("f1_list.png",         2000),  # list view
    ("f2_detail_down.png",  2000),  # detail: DOWN
    ("f3_detail_pending.png", 2000),  # detail: PENDING
    ("f4_detail_up.png",    2000),  # detail: UP
    ("f5_detail_maint.png", 2000),  # detail: MAINT
    ("f6_list_end.png",     3000),  # back to list
]

images = []
durations = []
for fname, ms in FRAMES:
    path = os.path.join(FRAMES_DIR, fname)
    if not os.path.exists(path):
        print(f"WARNING: {fname} not found, skipping")
        continue
    img = Image.open(path).convert("P", palette=Image.ADAPTIVE, colors=256)
    images.append(img)
    durations.append(ms)

if not images:
    raise SystemExit("No frames found — check gif_frames/")

images[0].save(
    OUT,
    save_all=True,
    append_images=images[1:],
    duration=durations,
    loop=0,
    optimize=True,
)
print(f"Saved {OUT}  ({len(images)} frames)")
