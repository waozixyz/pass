#!/usr/bin/env python3
"""Package the existing fingerprint as a native libdraw RGBA32 image."""
from pathlib import Path
import sys
from PIL import Image

root = Path(__file__).resolve().parents[1]
target = Path(sys.argv[1] if len(sys.argv) > 1 else root / "build/plan9/assets")
target.mkdir(parents=True, exist_ok=True)
image = Image.open(root / "assets/app/fingerprint.png").convert("RGBA")
# Libdraw uses premultiplied channels, stored as A,B,G,R on little endian hosts.
data = bytearray()
pixels = image.tobytes()
for index in range(0, len(pixels), 4):
    red, green, blue, alpha = pixels[index:index + 4]
    data.extend((alpha, blue * alpha // 255, green * alpha // 255, red * alpha // 255))
header = f'{"r8g8b8a8":>11} {0:11} {0:11} {image.width:11} {image.height:11} '
(target / "fingerprint.bit").write_bytes(header.encode("ascii") + data)
print(f"Plan 9 fingerprint: {image.width}x{image.height}, alpha preserved")
