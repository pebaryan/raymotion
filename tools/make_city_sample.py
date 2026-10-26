"""Build the bundled sample city (assets/data/city/town.city.json): a made-up town in
CityJSON 2.0, so the city:: tests and showcase have data with no licence strings attached.

    python tools/make_city_sample.py assets/data/city/town.city.json

It covers what real files use: LoD 1.2 blocks and LoD 2.2 buildings with semantic
surfaces (gable roofs, flat roofs, a courtyard block whose roof and ground have holes),
a Building made of two BuildingParts, roads with traffic areas, a canal, a park, trees and
street lamps placed from geometry templates (GeometryInstance), and a bus route as a line
geometry with its stops as points. Vertices are integers with a transform,
far from the origin, as in projected data. The layout is seeded, so runs are identical.
"""
import json
import math
import random
import sys

SCALE = 0.001
TRANSLATE = [500000.0, 5000000.0, 0.0]

vertices = []
index = {}


def v(x, y, z):
    key = (round(x / SCALE), round(y / SCALE), round(z / SCALE))
    if key not in index:
        index[key] = len(vertices)
        vertices.append(list(key))
    return index[key]


def ring(points):
    return [v(*p) for p in points]


def solid(faces):
    """faces: [(kind, [ring, hole, ...]), ...] with rings as point lists."""
    kinds = []
    boundaries = []
    for kind, rings in faces:
        if kind not in kinds:
            kinds.append(kind)
        boundaries.append(([ring(r) for r in rings], kinds.index(kind)))
    return {
        "type": "Solid",
        "boundaries": [[b for b, _ in boundaries]],
        "semantics": {"surfaces": [{"type": k} for k in kinds], "values": [[s for _, s in boundaries]]},
    }


def walls(loop, z0, z1):
    """Walls along a loop (anticlockwise from above: facing out; clockwise: facing in)."""
    out = []
    for i in range(len(loop)):
        (ax, ay), (bx, by) = loop[i], loop[(i + 1) % len(loop)]
        out.append(("WallSurface", [[(ax, ay, z0), (bx, by, z0), (bx, by, z1), (ax, ay, z1)]]))
    return out


def prism(loop, z0, z1, holes=(), roof="RoofSurface"):
    """A flat-roofed block over an anticlockwise footprint, with optional courtyards
    (clockwise loops). Every face is anticlockwise seen from outside, holes the other way."""
    faces = [("GroundSurface", [[(x, y, z0) for x, y in reversed(loop)]] + [[(x, y, z0) for x, y in reversed(h)] for h in holes])]
    faces += walls(loop, z0, z1)
    for h in holes:
        faces += walls(h, z0, z1)          # clockwise: the walls face into the courtyard
    faces.append((roof, [[(x, y, z1) for x, y in loop]] + [[(x, y, z1) for x, y in h] for h in holes]))
    return faces


def rect(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def gable(x0, y0, x1, y1, eaves, ridge):
    """A gable-roofed house; the ridge runs along the longer side."""
    along_x = (x1 - x0) >= (y1 - y0)
    if along_x:
        ym = (y0 + y1) / 2
        p0, p1, p2, p3 = (x0, y0), (x1, y0), (x1, y1), (x0, y1)
        r0, r1 = (x0, ym, ridge), (x1, ym, ridge)
        return [
            ("GroundSurface", [[(*p0, 0), (*p3, 0), (*p2, 0), (*p1, 0)]]),
            ("WallSurface", [[(*p0, 0), (*p1, 0), (*p1, eaves), (*p0, eaves)]]),
            ("WallSurface", [[(*p1, 0), (*p2, 0), (*p2, eaves), r1, (*p1, eaves)]]),
            ("WallSurface", [[(*p2, 0), (*p3, 0), (*p3, eaves), (*p2, eaves)]]),
            ("WallSurface", [[(*p3, 0), (*p0, 0), (*p0, eaves), r0, (*p3, eaves)]]),
            ("RoofSurface", [[(*p0, eaves), (*p1, eaves), r1, r0]]),
            ("RoofSurface", [[(*p2, eaves), (*p3, eaves), r0, r1]]),
        ]
    xm = (x0 + x1) / 2
    p0, p1, p2, p3 = (x0, y0), (x1, y0), (x1, y1), (x0, y1)
    r0, r1 = (xm, y0, ridge), (xm, y1, ridge)
    return [
        ("GroundSurface", [[(*p0, 0), (*p3, 0), (*p2, 0), (*p1, 0)]]),
        ("WallSurface", [[(*p0, 0), (*p1, 0), (*p1, eaves), r0, (*p0, eaves)]]),
        ("WallSurface", [[(*p1, 0), (*p2, 0), (*p2, eaves), (*p1, eaves)]]),
        ("WallSurface", [[(*p2, 0), (*p3, 0), (*p3, eaves), r1, (*p2, eaves)]]),
        ("WallSurface", [[(*p3, 0), (*p0, 0), (*p0, eaves), (*p3, eaves)]]),
        ("RoofSurface", [[(*p1, eaves), (*p2, eaves), r1, r0]]),
        ("RoofSurface", [[(*p3, eaves), (*p0, eaves), r0, r1]]),
    ]


def plate(loop, z, kind):
    return {"type": "MultiSurface", "lod": "1", "boundaries": [[ring([(x, y, z) for x, y in loop])]],
            "semantics": {"surfaces": [{"type": kind}], "values": [0]} if kind else None}


def main(out):
    rnd = random.Random(7)
    objects = {}

    def building(oid, faces2, height, footprint, holes=(), attrs=None):
        lod1 = solid(prism(footprint, 0, height, holes))
        lod1["lod"] = "1.2"
        lod2 = solid(faces2)
        lod2["lod"] = "2.2"
        a = {"measuredHeight": round(height, 2)}
        a.update(attrs or {})
        objects[oid] = {"type": "Building", "attributes": a, "geometry": [lod1, lod2]}

    BW, BD, ST = 60.0, 50.0, 12.0          # block width, depth, street width
    COLS, ROWS = 5, 4
    x_start, y_start = -(COLS * BW + (COLS - 1) * ST) / 2, -(ROWS * BD + (ROWS - 1) * ST) / 2
    n_house = 0
    for r in range(ROWS):
        for c in range(COLS):
            x0, y0 = x_start + c * (BW + ST), y_start + r * (BD + ST)
            x1, y1 = x0 + BW, y0 + BD
            block = f"block-{r}-{c}"
            if (r, c) == (1, 2):        # the tower: a podium and a shaft as BuildingParts
                objects["tower"] = {"type": "Building", "attributes": {"name": "Tower", "measuredHeight": 82.0, "function": "office"},
                                    "children": ["tower-podium", "tower-shaft"]}
                pod = rect(x0 + 6, y0 + 6, x1 - 6, y1 - 6)
                shaft = rect(x0 + 18, y0 + 14, x1 - 18, y1 - 14)
                objects["tower-podium"] = {"type": "BuildingPart", "parents": ["tower"], "attributes": {"measuredHeight": 12.0},
                                           "geometry": [dict(solid(prism(pod, 0, 12)), lod="2.2")]}
                objects["tower-shaft"] = {"type": "BuildingPart", "parents": ["tower"], "attributes": {"measuredHeight": 82.0},
                                          "geometry": [dict(solid(prism(shaft, 12, 82)), lod="2.2")]}
            elif (r, c) == (2, 1):      # a courtyard block: roof and ground with a hole
                outer = rect(x0 + 4, y0 + 4, x1 - 4, y1 - 4)
                inner = list(reversed(rect(x0 + 16, y0 + 14, x1 - 16, y1 - 14)))   # clockwise
                building("courtyard", prism(outer, 0, 18, [inner]), 18, outer, [inner], {"function": "housing", "yearOfConstruction": 1931})
            elif (r, c) == (0, 4):      # the park, with trees
                objects["park"] = {"type": "PlantCover", "attributes": {"name": "Park"},
                                   "geometry": [plate(rect(x0, y0, x1, y1), 0.05, None)]}
                for t in range(9):
                    tx, ty = x0 + 8 + rnd.random() * (BW - 16), y0 + 8 + rnd.random() * (BD - 16)
                    s = 0.8 + rnd.random() * 0.5
                    objects[f"tree-{t}"] = {"type": "SolitaryVegetationObject", "attributes": {"height": round(9 * s, 2)},
                                            "geometry": [{"type": "GeometryInstance", "template": 0, "boundaries": [v(tx, ty, 0)],
                                                          "transformationMatrix": [s, 0, 0, 0, 0, s, 0, 0, 0, 0, s, 0, 0, 0, 0, 1]}]}
            elif (r + c) % 3 == 0:      # flat-roofed apartment blocks
                for k, (bx0, bx1) in enumerate(((x0 + 4, x0 + 27), (x0 + 33, x1 - 4))):
                    h = 12 + rnd.randrange(0, 5) * 3
                    fp = rect(bx0, y0 + 6, bx1, y1 - 6)
                    building(f"{block}-flats-{k}", prism(fp, 0, h), h, fp, attrs={"function": "housing", "yearOfConstruction": 1960 + rnd.randrange(0, 50)})
            else:                       # rows of gable-roofed houses on two sides
                for side in (0, 1):
                    x = x0 + 2
                    while x < x1 - 8:
                        w = 7 + rnd.random() * 3
                        if x + w > x1 - 2:
                            break
                        eaves = 5.5 + rnd.random() * 3.5
                        ridge = eaves + 3 + rnd.random() * 2.5
                        ya, yb = (y0 + 2, y0 + 13) if side == 0 else (y1 - 13, y1 - 2)
                        building(f"house-{n_house}", gable(x, ya, x + w, yb, eaves, ridge), ridge, rect(x, ya, x + w, yb),
                                 attrs={"function": "housing", "yearOfConstruction": 1880 + rnd.randrange(0, 140)})
                        n_house += 1
                        x += w + 0.4
            if (r, c) not in ((0, 4),):
                objects[f"{block}-ground"] = {"type": "LandUse", "attributes": {"class": "yard"},
                                              "geometry": [plate(rect(x0, y0, x1, y1), 0.0, None)]}

    # Streets between the blocks (east-west and north-south), split where they cross.
    w_total, d_total = COLS * BW + (COLS - 1) * ST, ROWS * BD + (ROWS - 1) * ST
    for r in range(ROWS - 1):
        y0 = y_start + (r + 1) * BD + r * ST
        objects[f"street-ew-{r}"] = {"type": "Road", "attributes": {"name": f"Street {r + 1}"},
                                     "geometry": [plate(rect(x_start, y0, x_start + w_total, y0 + ST), 0.02, "TrafficArea")]}
    for c in range(COLS - 1):
        x0 = x_start + (c + 1) * BW + c * ST
        pieces = []
        for r in range(ROWS):
            ya = y_start + r * (BD + ST)
            pieces.append([ring([(x, y, 0.02) for x, y in rect(x0, ya, x0 + ST, ya + BD)])])
        objects[f"avenue-{c}"] = {"type": "Road", "attributes": {"name": f"Avenue {c + 1}"},
                                  "geometry": [{"type": "MultiSurface", "lod": "1", "boundaries": pieces,
                                                "semantics": {"surfaces": [{"type": "TrafficArea"}], "values": [0] * len(pieces)}}]}
    # The canal along the south edge, and its quay.
    cy0 = y_start - 26
    objects["canal"] = {"type": "WaterBody", "attributes": {"name": "Canal"},
                        "geometry": [plate(rect(x_start - 20, cy0, x_start + w_total + 20, cy0 + 16), -0.6, "WaterSurface")]}
    objects["quay"] = {"type": "Road", "attributes": {"name": "Quay"},
                       "geometry": [plate(rect(x_start - 20, cy0 + 16, x_start + w_total + 20, y_start), 0.02, "TrafficArea")]}

    # A tree: a trunk and a crown, in template coordinates (metres about its foot).
    tv = []
    def t(x, y, z):
        tv.append([x, y, z])
        return len(tv) - 1
    trunk = [t(-0.3, -0.3, 0), t(0.3, -0.3, 0), t(0.3, 0.3, 0), t(-0.3, 0.3, 0),
             t(-0.3, -0.3, 3), t(0.3, -0.3, 3), t(0.3, 0.3, 3), t(-0.3, 0.3, 3)]
    crown = [t(-2.5, -2.5, 3), t(2.5, -2.5, 3), t(2.5, 2.5, 3), t(-2.5, 2.5, 3), t(0, 0, 9)]
    a, b, c_, d, e, f, g, h = trunk
    p, q, r_, s, top = crown
    tree = {"type": "MultiSurface", "lod": "2",
            "boundaries": [[[a, b, f, e]], [[b, c_, g, f]], [[c_, d, h, g]], [[d, a, e, h]],
                           [[p, s, r_, q]], [[p, q, top]], [[q, r_, top]], [[r_, s, top]], [[s, p, top]]],
            "semantics": {"surfaces": [{"type": "+Trunk"}, {"type": "+Crown"}], "values": [0, 0, 0, 0, 1, 1, 1, 1, 1]}}

    # A street lamp: a pole, an arm reaching along +x and a lamp head under its end.
    def box(x0, y0, z0, x1, y1, z1):
        c = [t(x0, y0, z0), t(x1, y0, z0), t(x1, y1, z0), t(x0, y1, z0), t(x0, y0, z1), t(x1, y0, z1), t(x1, y1, z1), t(x0, y1, z1)]
        a, b, c2, d, e, f, g, h = c
        return [[[a, d, c2, b]], [[e, f, g, h]], [[a, b, f, e]], [[b, c2, g, f]], [[c2, d, h, g]], [[d, a, e, h]]]
    pole = box(-0.15, -0.15, 0, 0.15, 0.15, 7.2) + box(0, -0.1, 7.0, 1.8, 0.1, 7.2)
    head = box(1.3, -0.3, 6.7, 2.1, 0.3, 7.0)
    lamp = {"type": "MultiSurface", "lod": "2", "boundaries": pole + head,
            "semantics": {"surfaces": [{"type": "+Pole"}, {"type": "+Lamp"}], "values": [0] * len(pole) + [1] * len(head)}}

    def rot_z(deg):
        a = math.radians(deg)
        return [math.cos(a), -math.sin(a), 0, 0, math.sin(a), math.cos(a), 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]

    # Lamps along Street 2 and Avenue 2, on alternate sides, arms over the road.
    n_lamp = 0
    st_y = y_start + 2 * BD + ST + ST / 2           # centre line of street-ew-1
    av_x = x_start + 2 * BW + ST + ST / 2           # centre line of avenue-1
    x = x_start + 12
    k = 0
    while x < x_start + w_total - 6:
        side = 1 if k % 2 else -1
        objects[f"lamp-{n_lamp}"] = {"type": "CityFurniture", "attributes": {"function": "street lamp", "street": "street-ew-1"},
                                     "geometry": [{"type": "GeometryInstance", "template": 1, "boundaries": [v(x, st_y + 5.2 * side, 0)],
                                                   "transformationMatrix": rot_z(-90 if side > 0 else 90)}]}
        n_lamp += 1
        x += 24
        k += 1
    y = y_start + 12
    k = 0
    while y < y_start + d_total - 6:
        side = 1 if k % 2 else -1
        objects[f"lamp-{n_lamp}"] = {"type": "CityFurniture", "attributes": {"function": "street lamp", "street": "avenue-1"},
                                     "geometry": [{"type": "GeometryInstance", "template": 1, "boundaries": [v(av_x + 5.2 * side, y, 0)],
                                                   "transformationMatrix": rot_z(180 if side > 0 else 0)}]}
        n_lamp += 1
        y += 24
        k += 1

    # A bus route along the street centre lines (a line geometry), and its stops.
    quay_y = (cy0 + 16 + y_start) / 2
    av = [x_start + (c + 1) * BW + c * ST + ST / 2 for c in range(COLS - 1)]
    st = [y_start + (r + 1) * BD + r * ST + ST / 2 for r in range(ROWS - 1)]
    route = [(x_start - 15, quay_y), (av[0], quay_y), (av[0], st[1]), (av[2], st[1]), (av[2], st[2]), (x_start + w_total - 4, st[2])]
    objects["bus-route"] = {"type": "GenericCityObject", "attributes": {"name": "Line 7", "function": "bus route"},
                            "geometry": [{"type": "MultiLineString", "lod": "0", "boundaries": [ring([(px, py, 0.3) for px, py in route])]}]}
    stops = [route[1], route[3], ((av[2] + route[5][0]) / 2, st[2])]
    objects["bus-stops"] = {"type": "GenericCityObject", "attributes": {"name": "Line 7 stops"},
                            "geometry": [{"type": "MultiPoint", "lod": "0", "boundaries": [v(px, py, 0.3) for px, py in stops]}]}

    for o in objects.values():
        for gm in o.get("geometry", []):
            if gm.get("semantics", 0) is None:
                del gm["semantics"]
    doc = {
        "type": "CityJSON",
        "version": "2.0",
        "transform": {"scale": [SCALE] * 3, "translate": TRANSLATE},
        "metadata": {"title": "raymotion sample town (synthetic)"},
        "CityObjects": objects,
        "geometry-templates": {"templates": [tree, lamp], "vertices-templates": tv},
        "vertices": vertices,   # integers; the file's coordinates are these * scale + translate
    }
    with open(out, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(doc, fh, separators=(",", ":"))
    kinds = {}
    for o in objects.values():
        kinds[o["type"]] = kinds.get(o["type"], 0) + 1
    print(out, len(objects), "objects", kinds, len(vertices), "vertices")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
