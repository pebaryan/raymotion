"""Assemble raymotion-rendered smart-grid CityJSON PBR prefab stills."""
from __future__ import annotations

import json
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets/data/city/smartgrid"
SOURCE = ROOT / "out/smartgrid-pbr-previews"


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    path = Path("C:/Windows/Fonts/segoeuib.ttf" if bold else "C:/Windows/Fonts/segoeui.ttf")
    if not path.exists():
        path = Path("C:/Windows/Fonts/arialbd.ttf" if bold else "C:/Windows/Fonts/arial.ttf")
    return ImageFont.truetype(str(path), size)


def main() -> None:
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--foliage", action="store_true", help="render only foliage prefabs")
    group.add_argument("--street", action="store_true", help="render only street infrastructure and furniture")
    args = parser.parse_args()
    manifest = json.loads((ASSETS / "manifest.json").read_text(encoding="utf-8"))
    street_ids = {"street-traffic-light", "street-light", "solar-street-light",
                  "traffic-sign-stop", "traffic-sign-warning", "traffic-sign-direction",
                  "street-pedestrian-crossing", "bus-stop-shelter", "park-bench",
                  "public-waste-bin", "street-bollard", "bicycle-rack"}
    entries = ([item for item in manifest["prefabs"] if item["id"].startswith("foliage-")]
               if args.foliage else
               [item for item in manifest["prefabs"] if item["id"] in street_ids]
               if args.street else manifest["prefabs"])
    cols = 3 if args.foliage else 4
    output = ROOT / ("out/smartgrid-foliage-pbr-sheet.png" if args.foliage else
                     "out/smartgrid-street-furniture-pbr-sheet.png" if args.street else
                     "out/smartgrid-cityjson-pbr-sheet.png")
    tile_w, tile_h = 480, 460
    margin, header = 32, 115
    rows = (len(entries) + cols - 1) // cols
    sheet = Image.new("RGB", (margin * 2 + tile_w * cols, header + margin + tile_h * rows), "#0b1422")
    pen = ImageDraw.Draw(sheet)
    pen.text((margin, 22), "SETTLEMENT FOLIAGE / CITYJSON PBR" if args.foliage else
             "STREET INFRASTRUCTURE / CITYJSON PBR" if args.street else
             "SMART GRID / CITYJSON PBR PREFABS",
             font=font(35, True), fill="#eaf2f7")
    pen.text((margin, 70),
             f"{len(entries)} raymotion-rendered models • per-object Standard PBR materials",
             font=font(17), fill="#9db2c0")
    for i, item in enumerate(entries):
        x = margin + (i % cols) * tile_w
        y = header + (i // cols) * tile_h
        path = SOURCE / (item["id"] + ".png")
        if not path.exists():
            raise FileNotFoundError(f"Missing raymotion render: {path}")
        sheet.paste(Image.open(path).convert("RGB"), (x, y + 48))
        pen.text((x + 12, y + 7), item["title"], font=font(20, True), fill="#eaf2f7")
        pen.text((x + 12, y + 32), item["id"], font=font(13), fill="#6bc9bd")
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    print(output)


if __name__ == "__main__":
    main()
