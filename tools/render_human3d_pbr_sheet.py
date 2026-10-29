"""Assemble raymotion PBR stills for city-scale human role prefabs."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "out/human3d-previews"
OUTPUT = ROOT / "out/human3d-pbr-prefabs-sheet.png"
ITEMS = [
    ("Resident", "human-resident"),
    ("Commuter", "human-commuter"),
    ("EV driver", "human-ev-driver"),
    ("Charging technician", "human-charging-technician"),
    ("Grid engineer", "human-grid-engineer"),
    ("Construction worker", "human-construction-worker"),
    ("Office worker", "human-office-worker"),
    ("Healthcare worker", "human-healthcare-worker"),
    ("Student", "human-student"),
    ("Teacher", "human-teacher"),
    ("Retail worker", "human-retail-worker"),
    ("Transit operator", "human-transit-operator"),
]


def font(size: int, bold: bool = False):
    path = Path("C:/Windows/Fonts/segoeuib.ttf" if bold else "C:/Windows/Fonts/segoeui.ttf")
    if not path.exists():
        path = Path("C:/Windows/Fonts/arialbd.ttf" if bold else "C:/Windows/Fonts/arial.ttf")
    return ImageFont.truetype(str(path), size)


def main():
    cols, tile_w, tile_h = 3, 480, 460
    margin, header = 32, 115
    rows = (len(ITEMS) + cols - 1) // cols
    sheet = Image.new("RGB", (margin * 2 + tile_w * cols, header + margin + tile_h * rows), "#0b1422")
    pen = ImageDraw.Draw(sheet)
    pen.text((margin, 22), "CITY-SCALE HUMAN ROLES / NATIVE PBR PREFABS",
             font=font(33, True), fill="#eaf2f7")
    pen.text((margin, 70), "12 raymotion g3d object builders • metre-scale figures and role equipment",
             font=font(17), fill="#9db2c0")
    for i, (title, slug) in enumerate(ITEMS):
        x, y = margin + (i % cols) * tile_w, header + (i // cols) * tile_h
        path = SOURCE / f"{slug}.png"
        if not path.exists():
            raise FileNotFoundError(path)
        sheet.paste(Image.open(path).convert("RGB"), (x, y + 48))
        pen.text((x + 12, y + 7), title, font=font(20, True), fill="#eaf2f7")
        pen.text((x + 12, y + 32), slug, font=font(13), fill="#6bc9bd")
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    main()
