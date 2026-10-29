"""Assemble raymotion-rendered native computer-component prefab stills."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "out/computer3d-previews"
OUTPUT = ROOT / "out/computer3d-pbr-prefabs-sheet.png"
ITEMS = [
    ("CPU", "pc-cpu"),
    ("RAM module", "pc-ram-module"),
    ("Graphics card", "pc-graphics-card"),
    ("Power supply", "pc-power-supply"),
    ("PC case", "pc-case"),
    ("CPU air cooler", "pc-cpu-air-cooler"),
    ("Motherboard", "pc-motherboard"),
    ("NVMe SSD", "pc-nvme-ssd"),
    ("SATA SSD", "pc-sata-ssd"),
    ("Hard drive", "pc-hard-drive"),
    ("Case fan", "pc-case-fan"),
    ("AIO liquid cooler", "pc-aio-cooler"),
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
    pen.text((margin, 22), "COMPUTER COMPONENTS / NATIVE PBR PREFABS",
             font=font(33, True), fill="#eaf2f7")
    pen.text((margin, 70), "12 raymotion g3d object builders • per-part Standard PBR materials",
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
