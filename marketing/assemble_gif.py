"""
Assemble animated GIF from raw QEMU screendump PPM frames.
- Collapses runs of identical/near-identical frames into one frame with
  accumulated duration (capped at MAX_STATIC_MS).
- Keeps animation frames at FRAME_MS each.
- Output: marketing/pebblekuma_demo.gif
"""
import os, sys
from PIL import Image
import numpy as np

IN_DIR  = "/tmp/gif_anim_frames"
OUT_GIF = os.path.join(os.path.dirname(__file__), "pebblekuma_demo.gif")

FRAME_MS      = 80    # display time for each animation frame
MAX_STATIC_MS = 1800  # cap on collapsed static pauses
DIFF_THRESHOLD = 500  # sum-of-abs pixel diff to classify as "changed"

# ── Load all frames ──────────────────────────────────────────────────────────
paths = sorted(f for f in os.listdir(IN_DIR) if f.endswith(".ppm"))
print(f"Loading {len(paths)} frames...")

frames = []
for name in paths:
    img = Image.open(os.path.join(IN_DIR, name)).convert("RGB")
    frames.append(img)

arrs = [np.array(img, dtype=np.int16) for img in frames]

# ── Compute per-frame diffs ──────────────────────────────────────────────────
diffs = [0]   # first frame always "changed"
for i in range(1, len(arrs)):
    d = int(np.sum(np.abs(arrs[i] - arrs[i-1])))
    diffs.append(d)

# ── Collapse static runs ─────────────────────────────────────────────────────
out_frames   = []
out_durations = []

i = 0
while i < len(frames):
    if diffs[i] < DIFF_THRESHOLD:
        # Static frame — look ahead to find how long the run lasts
        run_ms = 0
        j = i
        while j < len(frames) and diffs[j] < DIFF_THRESHOLD:
            run_ms += FRAME_MS
            j += 1
        out_frames.append(frames[i])
        out_durations.append(min(run_ms, MAX_STATIC_MS))
        i = j
    else:
        out_frames.append(frames[i])
        out_durations.append(FRAME_MS)
        i += 1

print(f"Reduced {len(frames)} frames → {len(out_frames)} output frames")

# ── Build a global palette from representative frames ────────────────────────
# Sample the list frame + one static frame per status color to ensure the
# palette covers every background color (pink=DOWN, yellow=PENDING, green=UP,
# blue=MAINT) as well as the white list view.
rep_indices = [0]   # list view
for dur, idx in sorted(zip(out_durations, range(len(out_frames))), reverse=True)[:6]:
    if idx not in rep_indices:
        rep_indices.append(idx)

# Tile the representative frames side-by-side to build a combined palette image
rep_imgs = [out_frames[i] for i in rep_indices[:6]]
w, h = rep_imgs[0].size
combined = Image.new("RGB", (w * len(rep_imgs), h))
for j, img in enumerate(rep_imgs):
    combined.paste(img, (j * w, 0))
global_pal = combined.quantize(colors=255, method=Image.Quantize.MEDIANCUT)

gif_frames = []
for img in out_frames:
    q = img.quantize(colors=255, palette=global_pal, dither=Image.Dither.NONE)
    gif_frames.append(q)

# ── Save GIF ─────────────────────────────────────────────────────────────────
print(f"Saving GIF to {OUT_GIF} ...")
gif_frames[0].save(
    OUT_GIF,
    save_all=True,
    append_images=gif_frames[1:],
    duration=out_durations,
    loop=0,
    optimize=False,
)

size_kb = os.path.getsize(OUT_GIF) // 1024
print(f"Done — {size_kb} KB, {len(gif_frames)} frames")
print(f"Frame durations: {out_durations}")
