"""Contact sheet for the two-second draft samples of three-levels-causality."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1] / "out"
files = sorted(p for p in root.glob("three-levels-draft-*.png") if p.stem.rsplit("-", 1)[-1].isdigit())
if len(files) != 28:
    raise SystemExit(f"Expected 28 two-second samples, found {len(files)}")
sheet = Image.new("RGB", (4 * 480, 7 * 302), "#0b1421")
pen = ImageDraw.Draw(sheet)
font = ImageFont.truetype("C:/Windows/Fonts/segoeuib.ttf", 18)
for i, path in enumerate(files):
    x, y = (i % 4) * 480, (i // 4) * 302
    sheet.paste(Image.open(path).convert("RGB").resize((480, 270)), (x, y + 30))
    pen.text((x + 8, y + 5), f"{i * 2 + 1:02d} s", fill="white", font=font)
target = root / "three-levels-draft-contact.png"
sheet.save(target)
print(target)
