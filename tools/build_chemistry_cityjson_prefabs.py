"""Build original, schematic chemistry-laboratory CityJSON equipment prefabs."""
from __future__ import annotations

import json
import math
from collections import Counter
from pathlib import Path

from build_smartgrid_cityjson_prefabs import Model, rect

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "assets/data/city/chemistry-lab"
REFERENCE = "Original illustrative laboratory equipment; no manufacturer or certified design"


def model(slug: str, title: str, description: str) -> Model:
    return Model(slug, title, description, REFERENCE, role_key="labRole")


def cylinder(m: Model, oid: str, x: float, y: float, z: float,
             radius: float, height: float, role: str, palette: str,
             segments: int = 12) -> None:
    lower = [(x + radius * math.cos(2 * math.pi * i / segments),
              y + radius * math.sin(2 * math.pi * i / segments), z)
             for i in range(segments)]
    upper = [(px, py, z + height) for px, py, _ in lower]
    faces = [list(reversed(lower))]
    faces += [[lower[i], lower[(i + 1) % segments], upper[(i + 1) % segments], upper[i]]
              for i in range(segments)]
    faces.append(upper)
    m.solid(oid, "GenericCityObject", faces,
            ["GroundSurface"] + ["WallSurface"] * segments + ["RoofSurface"],
            role, palette)


def frustum(m: Model, oid: str, x: float, y: float, z: float,
            bottom: float, top: float, height: float, role: str, palette: str,
            segments: int = 12) -> None:
    a = 2 * math.pi / segments
    lower = [(x + bottom * math.cos(i * a), y + bottom * math.sin(i * a), z)
             for i in range(segments)]
    upper = [(x + top * math.cos(i * a), y + top * math.sin(i * a), z + height)
             for i in range(segments)]
    faces = [list(reversed(lower))]
    faces += [[lower[i], lower[(i + 1) % segments], upper[(i + 1) % segments], upper[i]]
              for i in range(segments)]
    faces.append(upper)
    m.solid(oid, "GenericCityObject", faces,
            ["GroundSurface"] + ["WallSurface"] * segments + ["RoofSurface"],
            role, palette)


def lab_bench() -> Model:
    m = model("lab-bench", "Laboratory bench", "Free-standing workbench with drawers, sink and service taps.")
    m.box("worktop", "GenericCityObject", -1.4, -.55, 2.8, 1.1, .86, .09, "worktop", "bench-top")
    for x in (-1.28, 1.16):
        for y in (-.45, .33):
            m.box(f"leg-{x}-{y}", "CityFurniture", x, y, .12, .12, 0, .86, "bench-leg", "metal")
    m.box("drawer-bank", "CityFurniture", -.93, -.48, .65, .72, .18, .62, "bench-drawers", "cabinet")
    for z in (.28, .48, .68):
        m.surface(f"drawer-face-{z}", "GenericCityObject", [[(-.88, -.49, z), (-.33, -.49, z),
                  (-.33, -.49, z + .16), (-.88, -.49, z + .16)]], ["WallSurface"],
                  "drawer-front", "trim")
    m.box("sink", "CityFurniture", .45, -.34, .62, .48, .93, .04, "sink", "dark")
    m.box("tap", "CityFurniture", .7, .13, .06, .06, .95, .32, "tap", "metal")
    return m


def fume_hood() -> Model:
    m = model("fume-hood", "Chemical fume hood", "Ducted hood with a visible sash, work chamber and exhaust collar.")
    m.box("hood-base", "GenericCityObject", -1, -.48, 2, .96, 0, .86, "hood-cabinet", "cabinet")
    m.box("hood-back", "GenericCityObject", -1, .36, 2, .12, .86, 1.48, "hood-shell", "metal")
    for x in (-1, .88):
        m.box(f"side-{x}", "GenericCityObject", x, -.48, .12, .96, .86, 1.48, "hood-shell", "metal")
    m.box("hood-top", "GenericCityObject", -1, -.48, 2, .96, 2.34, .22, "hood-canopy", "metal")
    m.box("hood-worktop", "GenericCityObject", -.86, -.36, 1.72, .7, .86, .09, "hood-worktop", "dark")
    m.surface("sliding-sash", "GenericCityObject", [[(-.86, -.495, 1.62), (.86, -.495, 1.62),
              (.86, -.495, 2.27), (-.86, -.495, 2.27)]], ["Window"], "sash", "glass")
    cylinder(m, "exhaust-collar", 0, .05, 2.56, .24, .27, "exhaust", "dark")
    return m


def chemical_cabinet() -> Model:
    m = model("chemical-storage-cabinet", "Chemical storage cabinet", "Double-door ventilated storage cabinet with hazard-color doors.")
    m.box("cabinet", "GenericCityObject", -.65, -.35, 1.3, .7, 0, 1.95, "chemical-cabinet", "metal")
    for side in (-1, 1):
        x0 = -.59 if side < 0 else .01
        m.surface(f"door-{side}", "GenericCityObject", [[(x0, -.365, .12), (x0 + .58, -.365, .12),
                  (x0 + .58, -.365, 1.83), (x0, -.365, 1.83)]], ["Door"],
                  "cabinet-door", "safety-yellow")
        xh = -.12 if side < 0 else .08
        m.box(f"handle-{side}", "CityFurniture", xh, -.40, .025, .04, .91, .22,
              "cabinet-handle", "dark")
    m.surface("vent", "GenericCityObject", [[(-.35, -.39, .24), (.35, -.39, .24),
              (.35, -.39, .39), (-.35, -.39, .39)]], ["WallSurface"], "vent", "dark")
    return m


def balance() -> Model:
    m = model("analytical-balance", "Analytical balance", "Precision balance with enclosed weighing chamber and display.")
    m.box("balance-base", "GenericCityObject", -.32, -.27, .64, .54, 0, .14, "balance-base", "light-metal")
    m.box("draft-shield", "GenericCityObject", -.25, -.18, .5, .39, .14, .37, "draft-shield", "glass")
    cylinder(m, "weighing-pan", 0, .015, .17, .12, .025, "weighing-pan", "metal")
    m.surface("display", "GenericCityObject", [[(-.16, -.29, .035), (.16, -.29, .035),
              (.16, -.29, .105), (-.16, -.29, .105)]], ["WallSurface"], "display", "screen")
    return m


def hotplate() -> Model:
    m = model("hotplate-stirrer", "Hotplate stirrer", "Bench hotplate with control knob and a beaker.")
    m.box("hotplate-base", "GenericCityObject", -.26, -.23, .52, .46, 0, .12, "hotplate", "dark")
    cylinder(m, "heating-plate", 0, .015, .12, .17, .025, "heating-surface", "metal")
    cylinder(m, "control-knob", .17, -.20, .07, .035, .025, "control-knob", "accent")
    cylinder(m, "beaker", 0, .015, .15, .115, .26, "beaker", "glass")
    cylinder(m, "liquid", 0, .015, .151, .105, .10, "illustrative-liquid", "liquid")
    return m


def rotary_evaporator() -> Model:
    m = model("rotary-evaporator", "Rotary evaporator", "Stand-mounted evaporation assembly with glass flask and heated bath.")
    m.box("rotavap-base", "GenericCityObject", -.65, -.35, 1.3, .7, 0, .08, "instrument-base", "dark")
    cylinder(m, "stand", -.36, .12, .08, .035, 1.38, "support-stand", "metal")
    m.box("drive-head", "GenericCityObject", -.40, -.22, .36, .35, 1.12, .27, "rotary-drive", "light-metal")
    cylinder(m, "water-bath", .25, -.05, .08, .26, .24, "heated-bath", "accent")
    frustum(m, "evaporation-flask", .25, -.05, .36, .22, .10, .34, "glass-flask", "glass")
    cylinder(m, "flask-neck", .25, -.05, .70, .055, .28, "glass-neck", "glass")
    cylinder(m, "condenser", -.31, .13, .68, .09, .72, "condenser", "glass")
    cylinder(m, "receiving-flask", -.30, .13, .29, .15, .32, "receiver", "glass")
    return m


def hplc() -> Model:
    m = model("hplc-stack", "HPLC system", "Stacked chromatography modules with solvent bottles and status screens.")
    m.box("hplc-stack", "GenericCityObject", -.43, -.32, .86, .64, 0, 1.3, "chromatography-stack", "light-metal")
    for i in range(4):
        z = .1 + i * .3
        m.surface(f"module-seam-{i}", "GenericCityObject", [[(-.4, -.333, z), (.4, -.333, z),
                  (.4, -.333, z + .015), (-.4, -.333, z + .015)]], ["WallSurface"],
                  "module-seam", "dark")
        m.surface(f"screen-{i}", "GenericCityObject", [[(.12, -.341, z + .07), (.32, -.341, z + .07),
                  (.32, -.341, z + .18), (.12, -.341, z + .18)]], ["WallSurface"],
                  "instrument-screen", "screen")
    for i, x in enumerate((-.22, .08), 1):
        cylinder(m, f"solvent-bottle-{i}", x, 0, 1.31, .105, .32, "solvent-bottle", "glass")
    return m


def centrifuge() -> Model:
    m = model("bench-centrifuge", "Bench centrifuge", "Compact enclosed centrifuge with round lid and control panel.")
    m.box("centrifuge-housing", "GenericCityObject", -.43, -.35, .86, .7, 0, .43, "centrifuge", "light-metal")
    cylinder(m, "round-lid", 0, .03, .43, .29, .045, "rotor-lid", "dark")
    m.surface("controls", "GenericCityObject", [[(-.32, -.365, .13), (.30, -.365, .13),
              (.30, -.365, .31), (-.32, -.365, .31)]], ["WallSurface"],
              "instrument-controls", "screen")
    return m


def drying_oven() -> Model:
    m = model("drying-oven", "Laboratory drying oven", "Countertop heated oven with viewing window and vent.")
    m.box("oven-body", "GenericCityObject", -.48, -.39, .96, .78, 0, 1.05, "drying-oven", "light-metal")
    m.surface("oven-window", "GenericCityObject", [[(-.35, -.405, .25), (.28, -.405, .25),
              (.28, -.405, .79), (-.35, -.405, .79)]], ["Window"], "oven-window", "dark")
    m.box("oven-handle", "CityFurniture", .34, -.44, .035, .06, .28, .46, "door-handle", "metal")
    cylinder(m, "oven-vent", 0, .11, 1.05, .13, .07, "oven-vent", "dark")
    return m


def reactor() -> Model:
    m = model("jacketed-reactor", "Jacketed reactor", "Small pilot reactor with vessel, mixer, ports and support frame.")
    for x in (-.44, .38):
        for y in (-.33, .27):
            m.box(f"frame-{x}-{y}", "CityFurniture", x, y, .06, .06, 0, 1.75,
                  "reactor-frame", "metal")
    cylinder(m, "reactor-vessel", 0, 0, .48, .39, 1.03, "reactor-vessel", "steel")
    cylinder(m, "reactor-lid", 0, 0, 1.51, .43, .08, "reactor-lid", "dark")
    cylinder(m, "agitator-motor", 0, 0, 1.59, .19, .39, "agitator", "dark")
    cylinder(m, "feed-port", -.27, .08, 1.59, .05, .2, "feed-port", "metal")
    m.box("outlet", "CityFurniture", .35, -.05, .34, .10, .61, .1, "outlet-pipe", "metal")
    return m


def glassware_rack() -> Model:
    m = model("glassware-rack", "Glassware rack", "Two-shelf rack with flasks and reagent bottles.")
    for x in (-.78, .73):
        for y in (-.27, .22):
            m.box(f"upright-{x}-{y}", "CityFurniture", x, y, .05, .05, 0, 1.55,
                  "rack-upright", "metal")
    for z in (.35, 1.03):
        m.box(f"shelf-{z}", "CityFurniture", -.80, -.30, 1.6, .6, z, .045,
              "rack-shelf", "metal")
    for i, x in enumerate((-.53, -.17, .19, .55), 1):
        frustum(m, f"flask-{i}", x, -.04, .40, .11, .045, .28, "glass-flask", "glass")
        cylinder(m, f"flask-neck-{i}", x, -.04, .68, .045, .11, "glass-neck", "glass")
    for i, x in enumerate((-.45, 0, .45), 1):
        cylinder(m, f"bottle-{i}", x, -.03, 1.08, .09, .27, "reagent-bottle", "glass")
        cylinder(m, f"cap-{i}", x, -.03, 1.35, .055, .05, "bottle-cap", "accent")
    return m


def vacuum_pump() -> Model:
    m = model("vacuum-pump", "Vacuum pump", "Bench vacuum pump with motor, pump head and carrying handle.")
    m.box("pump-base", "GenericCityObject", -.42, -.22, .84, .44, 0, .08, "pump-base", "dark")
    m.box("motor", "GenericCityObject", -.34, -.16, .43, .32, .08, .34, "pump-motor", "metal")
    cylinder(m, "pump-head", .24, 0, .08, .17, .33, "pump-head", "steel")
    m.box("handle-left", "CityFurniture", -.15, -.03, .05, .06, .42, .18, "handle", "dark")
    m.box("handle-right", "CityFurniture", .17, -.03, .05, .06, .42, .18, "handle", "dark")
    m.box("handle-top", "CityFurniture", -.15, -.03, .37, .06, .56, .05, "handle", "dark")
    return m


FACTORIES = [lab_bench, fume_hood, chemical_cabinet, balance, hotplate,
             rotary_evaporator, hplc, centrifuge, drying_oven, reactor,
             glassware_rack, vacuum_pump]


def main() -> None:
    DEST.mkdir(parents=True, exist_ok=True)
    manifest = {"collection": "chemistry-lab-cityjson-prefabs", "version": 1,
                "illustrative": True, "prefabs": []}
    for factory in FACTORIES:
        m = factory()
        doc = m.finish()
        path = DEST / (m.slug + ".city.json")
        path.write_text(json.dumps(doc, separators=(",", ":"), ensure_ascii=False) + "\n",
                        encoding="utf-8")
        xs, ys, zs = ([p[i] for p in m.vertices] for i in range(3))
        manifest["prefabs"].append({
            "id": m.slug, "file": path.name, "title": m.title,
            "description": m.description, "referenceBasis": m.reference,
            "boundsMetres": [min(xs), min(ys), min(zs), max(xs), max(ys), max(zs)],
            "cityObjectCount": len(m.objects),
            "objectTypes": dict(sorted(Counter(o["type"] for o in m.objects.values()).items())),
        })
    (DEST / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {len(manifest['prefabs'])} CityJSON prefabs to {DEST}")


if __name__ == "__main__":
    main()
