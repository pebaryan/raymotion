"""Build the bundled map data (assets/data/geo) from Natural Earth 1:110m GeoJSON.

Natural Earth is public domain (https://www.naturalearthdata.com). This keeps a few
properties per feature and rounds coordinates to 0.001 degrees (about 100 m, far below
the 1:110m scale's detail), so the files stay small.

    python tools/make_geo_data.py <folder with the ne_110m_*.geojson files> assets/data/geo

Inputs (natural-earth-vector v5.1.2, geojson/): ne_110m_admin_0_countries, ne_110m_land,
ne_110m_lakes, ne_110m_rivers_lake_centerlines, ne_110m_populated_places_simple.
"""
import json
import os
import sys


def rounded(coords):
    if isinstance(coords[0], (int, float)):
        return [round(coords[0], 3), round(coords[1], 3)]
    return [rounded(c) for c in coords]


def code(value):
    return "" if value in (None, "-99", -99) else value


def write(path, features):
    collection = {"type": "FeatureCollection", "features": features}
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(collection, f, ensure_ascii=False, separators=(",", ":"))
    print("%-40s %4d features  %7d bytes" % (path, len(features), os.path.getsize(path)))


def feature(geometry, properties, fid=None):
    out = {"type": "Feature"}
    if fid:
        out["id"] = fid
    out["properties"] = properties
    out["geometry"] = {"type": geometry["type"], "coordinates": rounded(geometry["coordinates"])}
    return out


def main(src, dst):
    def load(name):
        with open(os.path.join(src, name + ".geojson"), encoding="utf-8") as f:
            return json.load(f)["features"]

    os.makedirs(dst, exist_ok=True)

    countries = []
    for f in load("ne_110m_admin_0_countries"):
        p = f["properties"]
        a3 = code(p["ISO_A3_EH"]) or code(p["ISO_A3"]) or p["ADM0_A3"]
        props = {
            "name": p["NAME"],
            "name_long": p["NAME_LONG"],
            "iso_a2": code(p["ISO_A2_EH"]) or code(p["ISO_A2"]),
            "iso_a3": a3,
            "continent": p["CONTINENT"],
            "region": p["REGION_UN"],
            "subregion": p["SUBREGION"],
            "pop": p["POP_EST"],
            "gdp_md": p["GDP_MD"],
            "label": [round(p["LABEL_X"], 3), round(p["LABEL_Y"], 3)],
        }
        countries.append(feature(f["geometry"], props, a3))
    countries.sort(key=lambda f: f["properties"]["name"])
    write(os.path.join(dst, "countries-110m.geojson"), countries)

    write(os.path.join(dst, "land-110m.geojson"), [feature(f["geometry"], {}) for f in load("ne_110m_land")])
    write(os.path.join(dst, "lakes-110m.geojson"),
          [feature(f["geometry"], {"name": f["properties"]["name"]}) for f in load("ne_110m_lakes")])
    write(os.path.join(dst, "rivers-110m.geojson"),
          [feature(f["geometry"], {"name": f["properties"]["name"]}) for f in load("ne_110m_rivers_lake_centerlines")])

    cities = []
    for f in load("ne_110m_populated_places_simple"):
        p = f["properties"]
        cities.append(feature(f["geometry"], {
            "name": p["name"],
            "country": p["adm0name"],
            "iso_a2": code(p["iso_a2"]),
            "pop": p["pop_max"],
            "capital": p["adm0cap"] == 1,
        }))
    cities.sort(key=lambda f: -f["properties"]["pop"])
    write(os.path.join(dst, "cities-110m.geojson"), cities)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
