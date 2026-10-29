"""Render a static inspection sheet for the chemistry lab CityJSON prefabs."""
from __future__ import annotations

import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from render_smartgrid_cityjson_sheet import PALETTE, draw

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets/data/city/chemistry-lab"
OUTPUT = ROOT / "out/chemistry-cityjson-prefabs-sheet.png"

PALETTE.update({
    "bench-top": "#d8ddd9", "cabinet": "#8999a0", "light-metal": "#b7c5c9",
    "steel": "#8b9da6", "safety-yellow": "#e8bd57", "screen": "#6fc8d4",
    "liquid": "#53b6bb", "glass": "#78c7d9", "accent": "#efb568",
})


def main() -> None:
    manifest = json.loads((ASSETS / "manifest.json").read_text(encoding="utf-8"))
    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 12})
    fig = plt.figure(figsize=(18, 14), facecolor="#0b1422")
    fig.text(.045, .969, "CHEMISTRY LAB / CITYJSON PREFABS",
             color="#eaf2f7", fontsize=27, weight="bold")
    fig.text(.045, .938,
             "12 original, illustrative room-scale models for raymotion",
             color="#9db2c0", fontsize=13)
    for i, item in enumerate(manifest["prefabs"]):
        ax = fig.add_subplot(3, 4, i + 1, projection="3d", computed_zorder=False)
        doc = json.loads((ASSETS / item["file"]).read_text(encoding="utf-8"))
        draw(ax, doc, min_span=.8)
        x0, y0 = (i % 4) * .24 + .045, .893 - (i // 4) * .278
        fig.text(x0, y0, item["title"], color="#eaf2f7", fontsize=13, weight="bold")
        fig.text(x0, y0 - .026, item["id"], color="#6bc9bd", fontsize=10)
    fig.subplots_adjust(left=.035, right=.99, bottom=.035, top=.91, wspace=.02, hspace=.16)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUTPUT, dpi=150, facecolor=fig.get_facecolor())
    plt.close(fig)
    print(OUTPUT)


if __name__ == "__main__":
    main()
