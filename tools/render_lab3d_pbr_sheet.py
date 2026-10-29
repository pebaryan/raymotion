"""Assemble the native raymotion PBR stills into a reviewable catalog sheet.

Build and run `raymotion_lab3d_tests --preview out/lab3d-previews` first.
"""
from __future__ import annotations

import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
ITEMS = [
    ("Laboratory bench", "lab-bench"),
    ("Chemical fume hood", "fume-hood"),
    ("Chemical cabinet", "chemical-storage-cabinet"),
    ("Analytical balance", "analytical-balance"),
    ("Hotplate stirrer", "hotplate-stirrer"),
    ("Rotary evaporator", "rotary-evaporator"),
    ("HPLC system", "hplc-stack"),
    ("Bench centrifuge", "bench-centrifuge"),
    ("Drying oven", "drying-oven"),
    ("Jacketed reactor", "jacketed-reactor"),
    ("Glassware rack", "glassware-rack"),
    ("Vacuum pump", "vacuum-pump"),
]


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    path = Path("C:/Windows/Fonts/segoeuib.ttf" if bold else "C:/Windows/Fonts/segoeui.ttf")
    if not path.exists():
        path = Path("C:/Windows/Fonts/arialbd.ttf" if bold else "C:/Windows/Fonts/arial.ttf")
    return ImageFont.truetype(str(path), size)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--city", action="store_true", help="assemble the CityJSON PBR renders")
    args = parser.parse_args()
    source = ROOT / ("out/city-pbr-previews" if args.city else "out/lab3d-previews")
    output = ROOT / ("out/chemistry-cityjson-pbr-sheet.png" if args.city else "out/lab3d-pbr-prefabs-sheet.png")
    tile_w, tile_h = 480, 460
    margin, header = 32, 115
    sheet = Image.new("RGB", (margin * 2 + tile_w * 4, header + margin + tile_h * 3), "#0b1422")
    pen = ImageDraw.Draw(sheet)
    title = "CHEMISTRY LAB / CITYJSON PBR" if args.city else "CHEMISTRY LAB / NATIVE PBR PREFABS"
    subtitle = ("12 CityJSON models rendered in raymotion with per-object PBR materials" if args.city else
                "12 raymotion g3d object builders • rendered with Standard PBR materials")
    pen.text((margin, 22), title, font=font(35, True), fill="#eaf2f7")
    pen.text((margin, 70), subtitle,
             font=font(17), fill="#9db2c0")
    for i, (title, slug) in enumerate(ITEMS):
        x = margin + (i % 4) * tile_w
        y = header + (i // 4) * tile_h
        path = source / f"{slug}.png"
        if not path.exists():
            raise FileNotFoundError(f"Missing raymotion render: {path}")
        still = Image.open(path).convert("RGB")
        sheet.paste(still, (x, y + 48))
        pen.text((x + 12, y + 7), title, font=font(20, True), fill="#eaf2f7")
        pen.text((x + 12, y + 32), slug, font=font(13), fill="#6bc9bd")
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    print(output)


if __name__ == "__main__":
    main()
