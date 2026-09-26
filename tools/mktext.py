#!/usr/bin/env python3
"""Preview only: renders the credits bitmap the Windows build draws with GDI (build/text.raw, 2048x256, bottom-up)."""
from PIL import Image, ImageDraw, ImageFont
import os
W, H = 2048, 256
im = Image.new('L', (W, H), 0); d = ImageDraw.Draw(im)
F = '/usr/share/fonts/truetype/dejavu/DejaVuSans-ExtraLight.ttf'
if not os.path.exists(F): F = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
f1 = ImageFont.truetype(F, 150); f2 = ImageFont.truetype(F, 40)
for txt, f, y in (('LINKED//SPIRITS', f1, 10), ('By: Claude Opus 5.5  //  Prompts by: DarkCenobyte', f2, 190)):
    w = d.textlength(txt, font=f); d.text(((W - w) / 2, y), txt, fill=255, font=f)
os.makedirs('build', exist_ok=True)
open('build/text.raw', 'wb').write(im.transpose(Image.FLIP_TOP_BOTTOM).tobytes())
