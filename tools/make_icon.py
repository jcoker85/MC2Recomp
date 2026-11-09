"""Draws hub/icon/hub.ico, the launcher's icon: a dark rounded square with "MC2" in the launcher's red. Made from scratch (no game
artwork), so it can be kept in the repository.  Needs Pillow:  python tools/make_icon.py"""
import os
from PIL import Image, ImageDraw, ImageFont

here = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(here, '..', 'hub', 'icon', 'hub.ico')
size = 256
img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
d.rounded_rectangle((8, 8, size - 8, size - 8), radius=44, fill=(18, 18, 22, 255), outline=(222, 30, 24, 255), width=10)
font = None
for name in ('impact.ttf', 'arialbd.ttf'):
    try:
        font = ImageFont.truetype(os.path.join(os.environ.get('WINDIR', r'C:\Windows'), 'Fonts', name), 118)
        break
    except OSError:
        pass
font = font or ImageFont.load_default()
box = d.textbbox((0, 0), 'MC2', font=font)
w, h = box[2] - box[0], box[3] - box[1]
d.text(((size - w) / 2 - box[0], (size - h) / 2 - box[1]), 'MC2', font=font, fill=(222, 30, 24, 255))
os.makedirs(os.path.dirname(out), exist_ok=True)
img.save(out, sizes=[(256, 256), (64, 64), (48, 48), (32, 32), (16, 16)])
print(out)
