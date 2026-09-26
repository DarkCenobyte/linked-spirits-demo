#!/usr/bin/env python3
"""Preview only: renders the text atlas the Windows build draws with GDI (build/text.raw, bottom-up):
the credits (256 rows), then one 64-pixel row per sung line."""
from PIL import Image, ImageDraw, ImageFont
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import voicebank as vb
LY = [l['text'] for l in vb.read_lyrics()]
W, H = 2048, 256 + 64 * len(LY)
im = Image.new('L', (W, H), 0); d = ImageDraw.Draw(im)
F = '/usr/share/fonts/truetype/dejavu/DejaVuSans-ExtraLight.ttf'
if not os.path.exists(F): F = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
f1 = ImageFont.truetype(F, 150); f2 = ImageFont.truetype(F, 40)
for txt, f, y in (('LINKED//SPIRITS', f1, 10), ('By: Claude Opus 5.5  //  Prompts by: DarkCenobyte', f2, 190)):
    w = d.textlength(txt, font=f); d.text(((W - w) / 2, y), txt, fill=255, font=f)
F2 = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
f3 = ImageFont.truetype(F2, 40)
for i, txt in enumerate(LY):
    w = d.textlength(txt, font=f3); d.text(((W - w) / 2, 256 + i * 64 + 10), txt, fill=255, font=f3)
os.makedirs('build', exist_ok=True)
open('build/text.raw', 'wb').write(im.transpose(Image.FLIP_TOP_BOTTOM).tobytes())
