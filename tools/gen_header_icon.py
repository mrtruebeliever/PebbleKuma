#!/usr/bin/env python3
"""Generates the small top-bar bear icon by downsampling the existing
25x25 launcher icon (resources/images/icon_bear.png, white fill + black
outline) to 18x18 via LANCZOS. A dedicated small resource reads cleanly on
the accent header bar; scaling the full-size icon down at draw time on the
watch would blur the outline instead.

Run with the pebble-tool venv python (has Pillow):
    ~/.local/share/uv/tools/pebble-tool/bin/python3 tools/gen_header_icon.py
"""
from PIL import Image

SIZE = 18

src = Image.open("resources/images/icon_bear.png").convert("RGBA")
small = src.resize((SIZE, SIZE), Image.LANCZOS)
small.save("resources/images/header_icon.png")
print("wrote resources/images/header_icon.png (%dx%d)" % (SIZE, SIZE))
