"""Builds assets/data/bundaran-hi/bundaran-hi.city.json (CityJSON 2.0, LoD1) for the bundaran-hi-banner film from OpenStreetMap.

    python tools/osm_bundaran_hi.py                 # fetch from the Overpass API
    python tools/osm_bundaran_hi.py --from hi.json  # or convert a saved Overpass response

Map data (c) OpenStreetMap contributors, available under the Open Database Licence (ODbL).

Coordinates are metres from the centre of the roundabout in the film's scene axes: x east, z south.
Buildings keep their footprint (outer ring) and a height: `height`, else `building:levels` x 3.6 m,
else a guess from the footprint area (marked "guessed": true). Roads keep their centre line, class,
lane count (OSM `lanes`, else a default by class) and one-way flag. The roundabout itself is not
kept: the film draws it as a ring from the radii noted in `ring`.
"""
import argparse
import json
import math
import os
import sys
import urllib.parse
import urllib.request

CENTRE = (-6.19502, 106.82303)          # Bundaran HI, the middle of the pool
RADIUS = 650                            # metres of buildings kept round it
QUERY = """[out:json][timeout:60];
(way["building"](around:{r},{lat},{lon});
 way["highway"](around:{rr},{lat},{lon});
 way["natural"="water"](around:150,{lat},{lon});
 way["junction"="roundabout"](around:200,{lat},{lon}););
out body;>;out skel qt;"""
DEFAULT_LANES = {"trunk": 3, "primary": 3, "secondary": 2, "tertiary": 2, "busway": 1, "residential": 1,
                 "unclassified": 1, "living_street": 1, "service": 1, "primary_link": 1, "secondary_link": 1,
                 "tertiary_link": 1, "trunk_link": 1}
KEEP_ROADS = set(DEFAULT_LANES)


def fetch():
    q = QUERY.format(r=RADIUS, rr=RADIUS - 50, lat=CENTRE[0], lon=CENTRE[1])
    req = urllib.request.Request("https://overpass-api.de/api/interpreter",
                                 data=urllib.parse.urlencode({"data": q}).encode(),
                                 headers={"User-Agent": "raymotion-film/1.0"})
    with urllib.request.urlopen(req, timeout=120) as r:
        return json.load(r)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from", dest="src")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "assets", "data", "bundaran-hi", "bundaran-hi.city.json"))
    a = ap.parse_args()
    data = json.load(open(a.src, encoding="utf-8")) if a.src else fetch()

    lat0, lon0 = CENTRE
    kx, ky = 111320 * math.cos(math.radians(lat0)), 110574
    nodes = {e["id"]: e for e in data["elements"] if e["type"] == "node"}

    def pt(n):
        e = nodes[n]
        return [round((e["lon"] - lon0) * kx, 2), round(-(e["lat"] - lat0) * ky, 2)]   # x east, z south

    buildings, roads, ring = [], [], []
    for w in data["elements"]:
        if w["type"] != "way" or "tags" not in w:
            continue
        t = w["tags"]
        pts = [pt(n) for n in w["nodes"] if n in nodes]
        if len(pts) < 2:
            continue
        if "building" in t:
            if len(pts) < 4 or pts[0] != pts[-1]:
                continue
            ring_ = pts[:-1]
            area = 0.5 * abs(sum(p[0] * q[1] - q[0] * p[1] for p, q in zip(ring_, ring_[1:] + ring_[:1])))
            cx = sum(p[0] for p in ring_) / len(ring_)
            cz = sum(p[1] for p in ring_) / len(ring_)
            if math.hypot(cx, cz) > RADIUS or area < 12:
                continue
            h, guessed = None, False
            try:
                h = float(str(t.get("height", "")).replace("m", "").strip())
            except ValueError:
                h = None
            if not h and t.get("building:levels"):
                try:
                    h = float(t["building:levels"]) * 3.6
                except ValueError:
                    h = None
            if not h:
                guessed = True
                h = 7 if area < 150 else 10 if area < 600 else 16 if area < 2500 else 24
            b = {"id": w["id"], "h": round(max(3.0, h), 1), "ring": ring_}
            if guessed:
                b["guessed"] = True
            if t.get("name"):
                b["name"] = t["name"]
            buildings.append(b)
        elif t.get("junction") == "roundabout":
            ring.extend(math.hypot(*p) for p in pts)
        elif t.get("highway") in KEEP_ROADS:
            hw = t["highway"]
            try:
                lanes = int(str(t.get("lanes", "")).split(";")[0])
            except ValueError:
                lanes = DEFAULT_LANES[hw]
            roads.append({"id": w["id"], "class": hw, "name": t.get("name", ""), "lanes": max(1, lanes),
                          "oneway": t.get("oneway") == "yes", "line": pts})
        elif t.get("natural") == "water":
            ring.extend(-math.hypot(*p) for p in pts)          # negative: the pool's edge

    buildings = resolve_overlaps(buildings)
    pool = [-r for r in ring if r < 0]
    road = [r for r in ring if r > 0]
    city = to_cityjson(buildings, roads, data.get("osm3s", {}).get("timestamp_osm_base", ""))
    city["metadata"]["ring"] = {"pool_min": round(min(pool), 1) if pool else None, "pool_max": round(max(pool), 1) if pool else None,
                                "road_min": round(min(road), 1) if road else None, "road_max": round(max(road), 1) if road else None}
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        json.dump(city, f, ensure_ascii=False, separators=(",", ":"))
    print("buildings %d (%d with a height from OSM), roads %d, ring %s -> %s" % (
        len(buildings), sum(1 for b in buildings if not b.get("guessed")), len(roads), city["metadata"]["ring"], a.out), file=sys.stderr)


def inside(p, poly):
    x, y, c = p[0], p[1], False
    for (x0, y0), (x1, y1) in zip(poly, poly[1:] + poly[:1]):
        if (y0 > y) != (y1 > y) and x < x0 + (y - y0) * (x1 - x0) / (y1 - y0):
            c = not c
    return c


def crosses(a, b):
    def side(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
    for p, q in zip(a, a[1:] + a[:1]):
        for r, s in zip(b, b[1:] + b[:1]):
            if side(p, q, r) * side(p, q, s) < 0 and side(r, s, p) * side(r, s, q) < 0:
                return True
    return False


def shares_wall(a, b, tol=0.25, least=1.0):
    """True when an edge of `a` lies along an edge of `b` for more than `least` metres: outlines that
    only touch, e.g. a podium traced round a tower, whose walls would share a plane."""
    for p, q in zip(a, a[1:] + a[:1]):
        dx, dy = q[0] - p[0], q[1] - p[1]
        n = math.hypot(dx, dy)
        if n < least:
            continue
        ux, uy = dx / n, dy / n
        for r, s in zip(b, b[1:] + b[:1]):
            if abs((r[0] - p[0]) * uy - (r[1] - p[1]) * ux) > tol or abs((s[0] - p[0]) * uy - (s[1] - p[1]) * ux) > tol:
                continue
            t0 = (r[0] - p[0]) * ux + (r[1] - p[1]) * uy
            t1 = (s[0] - p[0]) * ux + (s[1] - p[1]) * uy
            if min(n, max(t0, t1)) - max(0, min(t0, t1)) > least:
                return True
    return False


def inset(poly, d):
    """The ring moved `d` metres inwards (mitred, the miter capped), whatever its winding."""
    area2 = sum(p[0] * q[1] - q[0] * p[1] for p, q in zip(poly, poly[1:] + poly[:1]))
    sgn = 1 if area2 > 0 else -1
    out, n = [], len(poly)
    for i in range(n):
        a, b, c = poly[i - 1], poly[i], poly[(i + 1) % n]
        normals = []
        for p, q in ((a, b), (b, c)):
            dx, dy = q[0] - p[0], q[1] - p[1]
            l = math.hypot(dx, dy) or 1
            normals.append((-dy / l * sgn, dx / l * sgn))           # pointing into the ring
        nx, ny = normals[0][0] + normals[1][0], normals[0][1] + normals[1][1]
        l = math.hypot(nx, ny)
        if l < 1e-6:
            out.append(list(b))
            continue
        nx, ny = nx / l, ny / l
        k = d / max(0.5, nx * normals[0][0] + ny * normals[0][1])
        out.append([round(b[0] + nx * k, 2), round(b[1] + ny * k, 2)])
    return out


def resolve_overlaps(buildings):
    """OSM often has overlapping outlines (a tower and its podium, a mall and the blocks inside it).
    Extruded as they are, their walls and roofs share planes and z-fight. A building lying wholly
    inside a taller one is dropped (it cannot be seen); otherwise, where two overlap or share a wall, the lower one's
    walls are pulled in and its roof dropped a little, so the taller one always wins."""
    box = [(min(p[0] for p in b["ring"]), min(p[1] for p in b["ring"]),
            max(p[0] for p in b["ring"]), max(p[1] for p in b["ring"])) for b in buildings]
    order = sorted(range(len(buildings)), key=lambda i: (-buildings[i]["h"], buildings[i]["id"]))
    rank = {i: r for r, i in enumerate(order)}
    drop, above = set(), {}                 # above[lo]: the taller buildings lo overlaps
    for i in range(len(buildings)):
        for j in range(i + 1, len(buildings)):
            a, b = box[i], box[j]
            if a[0] > b[2] or b[0] > a[2] or a[1] > b[3] or b[1] > a[3]:
                continue
            hi, lo = (i, j) if rank[i] < rank[j] else (j, i)
            H, L = buildings[hi]["ring"], buildings[lo]["ring"]
            lo_in = all(inside(p, H) for p in L)
            if lo_in and not crosses(H, L):
                drop.add(lo)
            elif (lo_in or any(inside(p, H) for p in L) or any(inside(p, L) for p in H) or crosses(H, L)
                  or shares_wall(H, L)):
                above.setdefault(lo, []).append(hi)
    # Steps, tallest first: one step below every taller building a building overlaps. Both may have
    # been pulled in (a tower inside a mall's outline moves the same way as the mall), so each step
    # pulls in further than the one above it.
    step = {}
    for i in order:
        step[i] = min(4, max((step[h] + 1 for h in above.get(i, []) if h not in drop), default=0))   # capped: rows of shophouses chain
    out = []
    for i, b in enumerate(buildings):
        if i in drop:
            continue
        if step[i]:
            b = dict(b, ring=inset(b["ring"], 0.3 * step[i]), h=round(b["h"] - 0.3 * step[i], 2))
        out.append(b)
    print("overlaps: dropped %d buildings inside taller ones, pulled in %d" % (len(drop), sum(1 for i in step if step[i] and i not in drop)),
          file=sys.stderr)
    return out


ROAD_WIDTH = {"busway": 3.8, "service": 4.0, "residential": 5.0, "living_street": 4.5, "unclassified": 5.0}


def to_cityjson(buildings, roads, stamp):
    """LoD1 buildings (extruded footprints, semantic roof/wall/ground) and roads (a ribbon of
    TrafficArea quads along the centre line) as CityJSON 2.0, in CityJSON axes: x east, y north,
    z up, millimetre integers."""
    verts, index = [], {}

    def v(x, y, z):
        key = (round(x * 1000), round(y * 1000), round(z * 1000))
        if key not in index:
            index[key] = len(verts)
            verts.append(list(key))
        return index[key]

    objects = {}
    for b in buildings:
        ring = [(p[0], -p[1]) for p in b["ring"]]                  # scene z south -> CityJSON y north
        area2 = sum(p[0] * q[1] - q[0] * p[1] for p, q in zip(ring, ring[1:] + ring[:1]))
        if area2 < 0:
            ring.reverse()                                         # counter-clockwise seen from above
        h = b["h"]
        roof = [v(x, y, h) for x, y in ring]
        ground = [v(x, y, 0) for x, y in reversed(ring)]
        walls = []
        for (x0, y0), (x1, y1) in zip(ring, ring[1:] + ring[:1]):
            walls.append([[v(x0, y0, 0), v(x1, y1, 0), v(x1, y1, h), v(x0, y0, h)]])
        attrs = {"height": h, "source": "guessed from footprint" if b.get("guessed") else "OSM"}
        if b.get("name"):
            attrs["name"] = b["name"]
        objects["osm-way-%d" % b["id"]] = {
            "type": "Building", "attributes": attrs,
            "geometry": [{"type": "Solid", "lod": "1",
                          "boundaries": [[[roof], [ground]] + walls],
                          "semantics": {"surfaces": [{"type": "RoofSurface"}, {"type": "GroundSurface"}, {"type": "WallSurface"}],
                                        "values": [[0, 1] + [2] * len(walls)]}}]}

    for r in roads:
        line = [(p[0], -p[1]) for p in r["line"]]
        width = ROAD_WIDTH.get(r["class"], 3.3 * r["lanes"])
        lift = 0.05 if r["class"] == "busway" else 0.03
        quads = []
        for (x0, y0), (x1, y1) in zip(line, line[1:]):
            dx, dy = x1 - x0, y1 - y0
            n = math.hypot(dx, dy)
            if n < 0.05:
                continue
            ox, oy = -dy / n * width / 2, dx / n * width / 2         # left of the direction of travel
            quads.append([[v(x0 - ox, y0 - oy, lift), v(x1 - ox, y1 - oy, lift), v(x1 + ox, y1 + oy, lift), v(x0 + ox, y0 + oy, lift)]])
        if not quads:
            continue
        objects["osm-way-%d" % r["id"]] = {
            "type": "Road",
            "attributes": {"class": r["class"], "name": r["name"], "lanes": r["lanes"], "oneway": r["oneway"],
                           "width": width, "line": [[round(x, 2), round(y, 2)] for x, y in line]},
            "geometry": [{"type": "MultiSurface", "lod": "1", "boundaries": quads,
                          "semantics": {"surfaces": [{"type": "TrafficArea"}], "values": [0] * len(quads)}}]}

    return {"type": "CityJSON", "version": "2.0",
            "transform": {"scale": [0.001, 0.001, 0.001], "translate": [0, 0, 0]},
            "metadata": {"title": "Bundaran HI, Jakarta (LoD1 from OpenStreetMap)",
                         "referenceSystem": "local metres from the roundabout's centre (-6.19502, 106.82303)",
                         "source": "OpenStreetMap via the Overpass API " + stamp,
                         "licence": "(c) OpenStreetMap contributors, ODbL 1.0"},
            "CityObjects": objects, "vertices": verts}


if __name__ == "__main__":
    main()
