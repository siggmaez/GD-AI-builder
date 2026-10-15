"""Build the bundled Latin/Cyrillic BMFont. Requires Pillow and Noto Sans TTF.

Source: https://github.com/google/fonts/tree/main/ofl/notosans (SIL OFL 1.1).
Usage: python tools/make_font.py path/to/NotoSans.ttf
"""
from pathlib import Path
import math
import sys
from PIL import Image, ImageDraw, ImageFont

font = ImageFont.truetype(sys.argv[1], 32)
chars = sorted(set(range(32, 127)) | set(range(0x400, 0x500)) |
               {0xAB, 0xBB, 0xA0, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026, 0x2116, 0x2022})
ascent, descent = font.getmetrics()
cell_w, cell_h = 48, ascent + descent + 4
cols = 16
width = 1024
height = 2 ** math.ceil(math.log2(math.ceil(len(chars) / cols) * cell_h))
atlas = Image.new("RGBA", (width, height))
draw = ImageDraw.Draw(atlas)
lines = [f'info face="AI Builder Sans" size=32 bold=0 italic=0 charset="" unicode=1 stretchH=100 smooth=1 aa=1 padding=0,0,0,0 spacing=0,0',
         f'common lineHeight={ascent + descent} base={ascent} scaleW={width} scaleH={height} pages=1 packed=0',
         'page id=0 file="ai-builder-font.png"', f'chars count={len(chars)}']
for i, code in enumerate(chars):
    char = chr(code)
    left, top, right, bottom = font.getbbox(char, anchor="lt")
    w, h = right - left, bottom - top
    # Baseline-relative offsets preserve accents, descenders and Cyrillic ё/Й.
    bx, by, _, _ = font.getbbox(char, anchor="ls")
    x, y = (i % cols) * cell_w + 2, (i // cols) * cell_h + 2
    draw.text((x - left, y - top), char, font=font, fill="white", anchor="lt")
    lines.append(f'char id={code} x={x} y={y} width={w} height={h} xoffset={bx} yoffset={ascent + by} xadvance={round(font.getlength(char))} page=0 chnl=15')
out = Path(__file__).resolve().parents[1] / "resources"
atlas.save(out / "ai-builder-font.png")
(out / "ai-builder-font.fnt").write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"Generated {len(chars)} glyphs, {width}x{height}")
