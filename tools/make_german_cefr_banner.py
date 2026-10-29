from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

S = 2
W, H = 1200 * S, 400 * S
OUT = Path("output")
OUT.mkdir(exist_ok=True)

BG1 = (19, 39, 42)
BG2 = (8, 15, 18)
INK = (243, 235, 221)
MUTED = (143, 166, 160)
MINT = (127, 224, 195)
AMBER = (255, 180, 92)
CORAL = (255, 138, 122)
DARK = (16, 24, 32)

font_dir = Path("assets/fonts")
def font(name, size):
    return ImageFont.truetype(str(font_dir / name), size * S)

archivo = lambda size: font("ArchivoBlack-Regular.ttf", size)
space = lambda size: font("SpaceGrotesk-Variable.ttf", size)
mono = lambda size: font("JetBrainsMono-Bold.ttf", size)

img = Image.new("RGB", (W, H), BG2)
d = ImageDraw.Draw(img, "RGBA")

for y in range(H):
    u = y / max(1, H - 1)
    c = tuple(int(BG1[i] + (BG2[i] - BG1[i]) * u) for i in range(3))
    d.line([(0, y), (W, y)], fill=c)

for j in range(22):
    for i in range(36):
        x = (28 + i * 68 + (j % 2) * 22) * S
        y = (26 + j * 36) * S
        a = 18 + (j % 3) * 6
        d.ellipse((x, y, x + 2 * S, y + 2 * S), fill=(*MINT, a))

# soft accent wash behind the number
wash = Image.new("RGBA", (W, H), (0, 0, 0, 0))
wd = ImageDraw.Draw(wash)
wd.ellipse((-160 * S, 40 * S, 700 * S, 560 * S), fill=(*MINT, 18))
img = Image.alpha_composite(img.convert("RGBA"), wash)
d = ImageDraw.Draw(img, "RGBA")

# vertical rule
for y in range(64 * S, 336 * S, 10 * S):
    d.line([(674 * S, y), (674 * S, y + 4 * S)], fill=(*MUTED, 55), width=1 * S)

d.text((64 * S, 42 * S), "GERMAN CEFR  /  SHORTS", font=mono(12), fill=MINT)
d.text((64 * S, 66 * S), "144", font=archivo(172), fill=INK, stroke_width=0)
d.text((64 * S, 246 * S), "GERMAN SHORTS", font=archivo(39), fill=INK)
d.text((64 * S, 300 * S), "A1 — C2", font=archivo(30), fill=MINT)
d.text((64 * S, 338 * S), "GRAMMAR · VOCABULARY · SITUATIONS", font=space(17), fill=MUTED)
d.text((64 * S, 364 * S), "30-SECOND LESSONS  ·  CAPTION-LED  ·  BGM + SFX", font=mono(11), fill=(*MUTED, 210))

# three miniature 9:16 cards
cards = [
    ("A1", "GRAMMAR", "ich bin", "du bist", MINT),
    ("B1", "VOCABULARY", "die Reform", "die Inflation", AMBER),
    ("C2", "SITUATIONS", "Subtext", "Register", CORAL),
]
cw, ch, gap = 128, 232, 16
x0, y0 = 712, 70

for i, (level, track, line1, line2, accent) in enumerate(cards):
    x = (x0 + i * (cw + gap)) * S
    y = y0 * S
    w, h = cw * S, ch * S
    d.rounded_rectangle((x + 5 * S, y + 6 * S, x + w + 5 * S, y + h + 6 * S), radius=14 * S, fill=(0, 0, 0, 80))
    d.rounded_rectangle((x, y, x + w, y + h), radius=14 * S, fill=(*INK, 255))
    d.rounded_rectangle((x, y, x + w, y + 8 * S), radius=8 * S, fill=accent)
    d.text((x + 14 * S, y + 22 * S), level, font=archivo(22), fill=DARK)
    d.text((x + 14 * S, y + 62 * S), track, font=mono(8), fill=MUTED)
    d.text((x + 14 * S, y + 102 * S), line1, font=space(17), fill=DARK)
    d.text((x + 14 * S, y + 132 * S), line2, font=space(17), fill=DARK)
    d.line([(x + 14 * S, y + 182 * S), (x + w - 14 * S, y + 182 * S)], fill=(*accent, 150), width=2 * S)
    d.text((x + 14 * S, y + 198 * S), "48 REELS", font=mono(8), fill=MUTED)

# footer right
d.text((712 * S, 330 * S), "144 REELS  ·  6 LEVELS", font=mono(11), fill=MUTED)

final = img.convert("RGB").resize((1200, 400), Image.Resampling.LANCZOS)
final.save(OUT / "german-cefr-banner.png", optimize=True)
img.save(OUT / "german-cefr-banner@2x.png", optimize=True)
print(OUT / "german-cefr-banner.png")
