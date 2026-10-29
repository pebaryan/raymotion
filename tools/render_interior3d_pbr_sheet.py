"""Assemble raymotion PBR stills for native interior prefabs."""
from __future__ import annotations

import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "out/interior3d-previews"
ITEMS = [
    ("Hospital bed", "hospital-bed", "hospital"),
    ("Bedside cabinet", "hospital-bedside-cabinet", "hospital"),
    ("IV stand", "hospital-iv-stand", "hospital"),
    ("Exam table", "hospital-exam-table", "hospital"),
    ("Wheelchair", "hospital-wheelchair", "hospital"),
    ("Medical cart", "hospital-medical-cart", "hospital"),
    ("Student desk", "school-student-desk", "school"),
    ("Classroom chair", "school-classroom-chair", "school"),
    ("Teacher desk", "school-teacher-desk", "school"),
    ("Whiteboard", "school-whiteboard", "school"),
    ("Bookshelf", "school-bookshelf", "school"),
    ("Lockers", "school-lockers", "school"),
    ("Office desk", "office-desk", "office"),
    ("Ergonomic chair", "office-ergonomic-chair", "office"),
    ("Meeting table", "office-meeting-table", "office"),
    ("Filing cabinet", "office-filing-cabinet", "office"),
    ("Cubicle partition", "office-cubicle-partition", "office"),
    ("Printer/copier", "office-printer", "office"),
    ("Server rack", "it-server-rack", "it"),
    ("Network cabinet", "it-network-cabinet", "it"),
    ("UPS cabinet", "it-ups-cabinet", "it"),
    ("Workstation", "it-workstation", "it"),
    ("Cooling unit", "it-cooling-unit", "it"),
    ("Security camera", "it-security-camera", "it"),
    ("Sofa", "home-sofa", "home"),
    ("Dining table", "home-dining-table", "home"),
    ("Refrigerator", "home-refrigerator", "home"),
    ("Kitchen range", "home-kitchen-range", "home"),
    ("Washing machine", "home-washing-machine", "home"),
    ("Floor lamp", "home-floor-lamp", "home"),
    ("Cafe table", "retail-cafe-table", "retail"),
    ("Restaurant chair", "retail-restaurant-chair", "retail"),
    ("Service counter", "retail-service-counter", "retail"),
    ("Market shelf", "retail-market-shelf", "retail"),
    ("Checkout", "retail-checkout", "retail"),
    ("Refrigerated case", "retail-refrigerated-case", "retail"),
]


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    path = Path("C:/Windows/Fonts/segoeuib.ttf" if bold else "C:/Windows/Fonts/segoeui.ttf")
    if not path.exists():
        path = Path("C:/Windows/Fonts/arialbd.ttf" if bold else "C:/Windows/Fonts/arial.ttf")
    return ImageFont.truetype(str(path), size)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--group", choices=("hospital", "school", "office", "it", "home", "retail"))
    args = parser.parse_args()
    entries = [item for item in ITEMS if args.group is None or item[2] == args.group]
    output = ROOT / (f"out/interior3d-{args.group}-pbr-sheet.png" if args.group else
                     "out/interior3d-pbr-prefabs-sheet.png")
    cols, tile_w, tile_h = 3, 480, 460
    margin, header = 32, 115
    rows = (len(entries) + cols - 1) // cols
    sheet = Image.new("RGB", (margin * 2 + tile_w * cols, header + margin + tile_h * rows), "#0b1422")
    pen = ImageDraw.Draw(sheet)
    title = (f"{args.group.upper()} / NATIVE PBR PREFABS" if args.group else
             "INTERIOR FURNITURE & EQUIPMENT / NATIVE PBR")
    pen.text((margin, 22), title, font=font(35, True), fill="#eaf2f7")
    pen.text((margin, 70), f"{len(entries)} raymotion g3d object builders • per-part Standard PBR materials",
             font=font(17), fill="#9db2c0")
    for i, (title, slug, _) in enumerate(entries):
        x, y = margin + (i % cols) * tile_w, header + (i // cols) * tile_h
        path = SOURCE / f"{slug}.png"
        if not path.exists():
            raise FileNotFoundError(f"Missing raymotion render: {path}")
        sheet.paste(Image.open(path).convert("RGB"), (x, y + 48))
        pen.text((x + 12, y + 7), title, font=font(20, True), fill="#eaf2f7")
        pen.text((x + 12, y + 32), slug, font=font(13), fill="#6bc9bd")
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    print(output)


if __name__ == "__main__":
    main()
