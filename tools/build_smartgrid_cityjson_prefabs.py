"""Author and validate the illustrative smart-grid CityJSON asset library.

These are original illustration models informed by Aryan et al. (2020), Figure 1,
and Siemens' public BIFROST descriptions. They are not BIFROST geometry exports.
"""
from __future__ import annotations

import json
import math
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "assets/data/city/smartgrid"


class Model:
    def __init__(self, slug: str, title: str, description: str, reference: str,
                 role_key: str = "smartGridRole"):
        self.slug, self.title, self.description, self.reference = slug, title, description, reference
        self.role_key = role_key
        self.vertices: list[list[float]] = []
        self.objects: dict[str, dict] = {}

    def _vertex(self, p) -> int:
        self.vertices.append([round(float(v), 3) for v in p])
        return len(self.vertices) - 1

    def _object(self, oid: str, typ: str, geom: dict, role: str, palette: str,
                parent: str | None = None, extra: dict | None = None) -> None:
        if oid in self.objects:
            raise ValueError(f"duplicate CityObject: {self.slug}/{oid}")
        attrs = {self.role_key: role, "paletteRole": palette}
        if extra:
            attrs.update(extra)
        obj = {"type": typ, "attributes": attrs, "geometry": [geom]}
        if parent:
            if parent not in self.objects:
                raise ValueError(f"parent must be authored first: {parent}")
            obj["parents"] = [parent]
            self.objects[parent].setdefault("children", []).append(oid)
        self.objects[oid] = obj

    def solid(self, oid: str, typ: str, faces: list[list[tuple]], kinds: list[str],
              role: str, palette: str, parent: str | None = None,
              extra: dict | None = None) -> None:
        assert len(faces) == len(kinds)
        boundaries = [[[self._vertex(p) for p in face]] for face in faces]
        geom = {"type": "Solid", "lod": "2.2", "boundaries": [boundaries]}
        if typ == "Building":
            names = list(dict.fromkeys(kinds))
            geom["semantics"] = {
                "surfaces": [{"type": k} for k in names],
                "values": [[names.index(k) for k in kinds]],
            }
        self._object(oid, typ, geom, role, palette, parent, extra)

    def box(self, oid: str, typ: str, x: float, y: float, w: float, d: float,
            z: float, h: float, role: str, palette: str,
            parent: str | None = None, extra: dict | None = None) -> None:
        a=(x,y,z); b=(x+w,y,z); c=(x+w,y+d,z); e=(x,y+d,z)
        A=(x,y,z+h); B=(x+w,y,z+h); C=(x+w,y+d,z+h); E=(x,y+d,z+h)
        self.solid(oid,typ,[[a,e,c,b],[a,b,B,A],[b,c,C,B],[c,e,E,C],[e,a,A,E],[A,B,C,E]],
                   ["GroundSurface","WallSurface","WallSurface","WallSurface","WallSurface","RoofSurface"],
                   role,palette,parent,extra)

    def gable(self, oid: str, x: float, y: float, w: float, d: float,
              wall: float, ridge: float, role: str, palette: str,
              extra: dict | None = None) -> None:
        a=(x,y,0); b=(x+w,y,0); c=(x+w,y+d,0); e=(x,y+d,0)
        A=(x,y,wall); B=(x+w,y,wall); C=(x+w,y+d,wall); E=(x,y+d,wall)
        r=(x+w/2,y,wall+ridge); R=(x+w/2,y+d,wall+ridge)
        self.solid(oid,"Building",[[a,e,c,b],[a,b,B,r,A],[b,c,C,B],[c,e,E,R,C],[e,a,A,E],
                                   [A,r,R,E],[r,B,C,R]],
                   ["GroundSurface"]+["WallSurface"]*4+["RoofSurface"]*2,
                   role,palette,extra=extra)

    def surface(self, oid: str, typ: str, polygons: list[list[tuple]], kinds: list[str],
                role: str, palette: str, parent: str | None = None,
                extra: dict | None = None) -> None:
        assert len(polygons) == len(kinds)
        boundaries = [[[self._vertex(p) for p in poly]] for poly in polygons]
        geom = {"type": "MultiSurface", "lod": "2.2", "boundaries": boundaries}
        if typ in ("Road", "TransportSquare", "Building"):
            names = list(dict.fromkeys(kinds))
            geom["semantics"] = {
                "surfaces": [{"type": k} for k in names],
                "values": [names.index(k) for k in kinds],
            }
        self._object(oid, typ, geom, role, palette, parent, extra)

    def finish(self) -> dict:
        if not self.objects or not self.vertices:
            raise ValueError(f"empty prefab {self.slug}")
        for oid, obj in self.objects.items():
            for parent in obj.get("parents", []):
                if oid not in self.objects[parent].get("children", []):
                    raise ValueError(f"unreciprocated parent link: {oid}")
            def check_ids(v):
                if isinstance(v, int) and not (0 <= v < len(self.vertices)):
                    raise ValueError(f"invalid vertex reference: {oid}/{v}")
                if isinstance(v, list):
                    for value in v: check_ids(value)
            check_ids(obj["geometry"][0]["boundaries"])
        return {
            "type": "CityJSON", "version": "2.0",
            "metadata": {
                "title": self.title, "description": self.description,
                "referenceBasis": self.reference, "illustrative": True,
                "units": "metres", "localOrigin": "footprint centre at ground level",
            },
            "CityObjects": self.objects, "vertices": self.vertices,
        }


def rect(x0,y0,x1,y1,z=0):
    return [(x0,y0,z),(x1,y0,z),(x1,y1,z),(x0,y1,z)]


def windows(m: Model, parent: str, w: float, d: float, floors: int,
            columns: int, floor_step: float, width: float = 1.6) -> None:
    polys=[]
    for floor in range(floors):
        z=1.35+floor*floor_step
        for col in range(columns):
            cx=-w/2+(col+1)*w/(columns+1)
            polys.append([(cx-width/2,-d/2-.04,z),(cx+width/2,-d/2-.04,z),
                          (cx+width/2,-d/2-.04,z+1.8),(cx-width/2,-d/2-.04,z+1.8)])
    m.surface(parent+"-windows","BuildingInstallation",polys,["Window"]*len(polys),
              "glazing","glass",parent)


def detached() -> Model:
    m=Model("residential-detached","Detached home","Pitched-roof single-household illustration.","2020 paper: residential buildings")
    m.gable("home",-6,-5,12,10,5.8,3.4,"residential-single","house-wall",{"use":"residential"})
    windows(m,"home",12,10,2,2,2.7,1.45)
    m.surface("home-door","BuildingInstallation",[[(-.65,-5.08,0),(.65,-5.08,0),
                                                       (.65,-5.08,2.25),(-.65,-5.08,2.25)]],
              ["Door"],"entrance","door","home")
    m.box("home-chimney","BuildingInstallation",2,1,1.1,1.1,6.8,2.3,"chimney","roof","home")
    return m


def terrace() -> Model:
    m=Model("residential-terrace","Terraced homes","Three attached residential bays with separate entrances.","2020 paper: different residential types")
    m.gable("terrace",-12,-5,24,10,6.5,2.6,"residential-terrace","house-wall",{"use":"residential"})
    windows(m,"terrace",24,10,2,6,2.8,1.25)
    doors=[]
    for x in (-8,0,8): doors.append([(x-.65,-5.08,0),(x+.65,-5.08,0),(x+.65,-5.08,2.35),(x-.65,-5.08,2.35)])
    m.surface("terrace-doors","BuildingInstallation",doors,["Door"]*3,"entrance","door","terrace")
    return m


def apartment() -> Model:
    m=Model("residential-apartment","Apartment block","Four-storey flat-roof residential block.","2020 paper: different residential types; current BIFROST: multi-residential structures")
    m.box("apartment","Building",-10,-7,20,14,0,15.5,"residential-multi","apartment-wall",extra={"use":"residential"})
    windows(m,"apartment",20,14,4,5,3.5,1.65)
    bands=[]
    for z in (3.8,7.3,10.8,14.3):
        bands.append([(-10.08,-7.08,z),(10.08,-7.08,z),(10.08,-7.08,z+.23),(-10.08,-7.08,z+.23)])
    m.surface("apartment-bands","BuildingInstallation",bands,["WallSurface"]*4,
              "floor-band","trim","apartment")
    m.box("apartment-lift","BuildingInstallation",-2,0,4,4,15.5,2.2,"roof-equipment","metal","apartment")
    return m


def office() -> Model:
    m=Model("commercial-office","Office building","Commercial office block with glazed front and rooftop plant.","2020 paper: commercial profiles")
    m.box("office","Building",-12,-8,24,16,0,17,"commercial-office","office-wall",extra={"use":"commercial"})
    windows(m,"office",24,16,4,8,3.5,1.85)
    m.box("office-plant","BuildingInstallation",2,0,6,5,17,2.2,"hvac","metal","office")
    m.surface("office-sign","BuildingInstallation",[[(-4,-8.12,14),(4,-8.12,14),(4,-8.12,15.2),(-4,-8.12,15.2)]],
              ["WallSurface"],"sign-band","accent","office")
    return m


def office_tower() -> Model:
    m=Model("commercial-office-tower","Office tower",
            "Ten-storey office tower with a low entrance podium and vertical glazing.",
            "User-requested office-building extension; illustrative, not a Figure 1 structure")
    m.box("office-tower","Building",-8,-7,16,14,0,35,
          "commercial-office","office-wall",extra={"use":"commercial","illustrativeStoreys":10})
    windows(m,"office-tower",16,14,10,4,3.25,2.0)
    m.box("tower-podium","BuildingPart",-12,-10,24,5,0,5.2,
          "office-podium","shop-wall","office-tower")
    m.surface("podium-glazing","BuildingInstallation",[[(-9,-10.08,.8),(9,-10.08,.8),
              (9,-10.08,4.5),(-9,-10.08,4.5)]],["Window"],
              "entrance-glazing","glass","office-tower")
    m.box("tower-roof-plant","BuildingInstallation",-2,-1.5,4,3,35,2.2,
          "hvac","metal","office-tower")
    return m


def city_hall() -> Model:
    m=Model("city-hall","City hall",
            "Symmetrical civic building with a public entrance and clock tower.",
            "User-requested civic illustration extension; not present in the 2020 paper's Figure 1")
    m.box("city-hall","Building",-13,-7,26,14,0,10,
          "civic-city-hall","civic-wall",extra={"use":"civic"})
    for side in (-1,1):
        x=-20 if side<0 else 13
        m.box(f"hall-wing-{side}","BuildingPart",x,-5,7,10,0,7,
              "civic-wing","civic-wall","city-hall")
    m.box("clock-tower","BuildingPart",-3,-2.5,6,5,10,10,
          "civic-clock-tower","civic-wall","city-hall")
    m.box("tower-cap","BuildingInstallation",-3.6,-3.1,7.2,6.2,20,1.1,
          "tower-cap","civic-roof","city-hall")
    m.surface("clock-face","BuildingInstallation",[[(-1.2,-2.58,15.4),(1.2,-2.58,15.4),
              (1.2,-2.58,18),(-1.2,-2.58,18)]],["WallSurface"],
              "clock-face","accent","city-hall")
    windows(m,"city-hall",26,14,2,6,3.6,1.25)
    m.surface("hall-entrance","BuildingInstallation",[[(-2.3,-7.11,0),(2.3,-7.11,0),
              (2.3,-7.11,4),(-2.3,-7.11,4)]],["Door"],
              "public-entrance","glass","city-hall")
    m.box("hall-portico","BuildingInstallation",-5,-9.5,10,2.5,4.1,.45,
          "portico","civic-roof","city-hall")
    return m


def event_hall() -> Model:
    m=Model("exhibition-event-hall","Exhibition and event hall",
            "Wide-span hall with a faceted arched roof, glass foyer and multiple public doors.",
            "User-requested exhibition/event illustration extension; not present in the 2020 paper's Figure 1")
    x0,x1=-23,23
    profile=[(-14,9),(-10,11),(-5,13),(0,14),(5,13),(10,11),(14,9)]
    front=[(x0,-14,0),(x1,-14,0),(x1,-14,9),(x0,-14,9)]
    back=[(x1,14,0),(x0,14,0),(x0,14,9),(x1,14,9)]
    faces=[[(x0,-14,0),(x0,14,0),(x1,14,0),(x1,-14,0)],front,back,
           [(x0,-14,0),(x0,-14,9),(x0,-10,11),(x0,-5,13),(x0,0,14),
            (x0,5,13),(x0,10,11),(x0,14,9),(x0,14,0)],
           [(x1,14,0),(x1,14,9),(x1,10,11),(x1,5,13),(x1,0,14),
            (x1,-5,13),(x1,-10,11),(x1,-14,9),(x1,-14,0)]]
    kinds=["GroundSurface"]+["WallSurface"]*4
    for (y0,z0),(y1,z1) in zip(profile,profile[1:]):
        faces.append([(x0,y0,z0),(x1,y0,z0),(x1,y1,z1),(x0,y1,z1)])
        kinds.append("RoofSurface")
    m.solid("event-hall","Building",faces,kinds,"public-event-hall","event-wall",
            extra={"use":"assembly"})
    m.surface("foyer-glazing","BuildingInstallation",[[(-19,-14.09,1),(19,-14.09,1),
              (19,-14.09,7.5),(-19,-14.09,7.5)]],["Window"],
              "public-foyer","glass","event-hall")
    doors=[]
    for x in (-12,-4,4,12):
        doors.append([(x-1.2,-14.14,0),(x+1.2,-14.14,0),
                      (x+1.2,-14.14,3.2),(x-1.2,-14.14,3.2)])
    m.surface("event-doors","BuildingInstallation",doors,["Door"]*4,
              "event-entrance","dark","event-hall")
    m.box("event-canopy","BuildingInstallation",-20,-16,40,2.5,7.3,.35,
          "entrance-canopy","accent","event-hall")
    return m


def shop() -> Model:
    m=Model("commercial-shop","Neighbourhood shop","Single-storey storefront with deep awning.","2020 paper: commercial profiles")
    m.box("shop","Building",-9,-6,18,12,0,7.2,"commercial-retail","shop-wall",extra={"use":"commercial"})
    m.surface("shop-front","BuildingInstallation",[[(-7.5,-6.08,1),(7.5,-6.08,1),(7.5,-6.08,5.2),(-7.5,-6.08,5.2)]],
              ["Window"],"storefront","glass","shop")
    m.box("shop-awning","BuildingInstallation",-8,-7,16,1.4,4.7,.28,"awning","accent","shop")
    return m


def factory_pv() -> Model:
    m=Model("industrial-factory-pv","PV factory","Wide white factory hall with rooftop photovoltaic array.","2020 paper Figure 1: PV on white factory roof")
    m.box("factory","Building",-17,-10,34,20,0,10,"industrial-factory-pv","factory-wall",extra={"use":"industrial"})
    m.box("factory-loader","BuildingInstallation",-10,-10.14,7,0.22,0,5.8,"loading-door","door","factory")
    panels=[]
    for row in range(3):
        for col in range(5):
            x=-14+col*5.4; y=-7+row*5.4
            panels.append([(x,y,10.16),(x+4.4,y,10.16),(x+4.4,y+3.9,10.55),(x,y+3.9,10.55)])
    m.surface("factory-pv","BuildingInstallation",panels,["RoofSurface"]*len(panels),
              "photovoltaic-array","solar","factory",{"illustrativePanels":len(panels)})
    return m


def warehouse() -> Model:
    m=Model("industrial-warehouse","Industrial warehouse","Large low industrial hall with three loading bays.","2020 paper: industrial/commercial profiles")
    m.gable("warehouse",-16,-10,32,20,8.2,2.7,"industrial-warehouse","warehouse-wall",{"use":"industrial"})
    doors=[]
    for x in (-9,0,9): doors.append([(x-2.3,-10.1,0),(x+2.3,-10.1,0),(x+2.3,-10.1,5.8),(x-2.3,-10.1,5.8)])
    m.surface("warehouse-doors","BuildingInstallation",doors,["Door"]*3,"loading-bay","door","warehouse")
    return m


def assembly_factory() -> Model:
    m=Model("industrial-assembly-factory","Assembly factory",
            "Broad production hall with roof monitors, loading bays and process exhaust.",
            "User-requested factory extension; original illustrative building")
    m.box("assembly-factory","Building",-20,-12,40,24,0,9.5,
          "industrial-assembly","assembly-wall",extra={"use":"industrial"})
    for i,x in enumerate((-13,-1,11),1):
        m.box(f"roof-monitor-{i}","BuildingInstallation",x,-8,8,16,9.5,2.0,
              "roof-monitor","metal","assembly-factory")
        m.surface(f"monitor-glass-{i}","BuildingInstallation",[[
                  (x,-8.08,9.75),(x+8,-8.08,9.75),(x+8,-8.08,11.1),(x,-8.08,11.1)]],
                  ["Window"],"roof-monitor-glazing","glass","assembly-factory")
    doors=[]
    for x in (-13,-1,11):
        doors.append([(x-3,-12.12,0),(x+3,-12.12,0),(x+3,-12.12,6.3),(x-3,-12.12,6.3)])
    m.surface("assembly-loading-bays","BuildingInstallation",doors,["Door"]*3,
              "loading-bay","door","assembly-factory")
    m.box("process-exhaust","BuildingInstallation",16,5,1.8,1.8,9.5,4.2,
          "process-exhaust","metal","assembly-factory")
    return m


def data_center() -> Model:
    m=Model("data-center","Data center",
            "Window-light technical building with rooftop cooling units and a service entrance.",
            "User-requested data-center extension; original illustrative building")
    m.box("data-center","Building",-18,-11,36,22,0,10.5,
          "data-center","data-center-wall",extra={"use":"data-centre"})
    vents=[]
    for x in (-14,-9,-4,1,6,11):
        vents.append([(x,-11.12,3.5),(x+3.2,-11.12,3.5),
                      (x+3.2,-11.12,7.5),(x,-11.12,7.5)])
    m.surface("cooling-louvres","BuildingInstallation",vents,["WallSurface"]*len(vents),
              "cooling-louvre","dark","data-center")
    for i,x in enumerate((-12,-4,4,12),1):
        for j,y in enumerate((-5,3),1):
            m.box(f"roof-cooler-{i}-{j}","BuildingInstallation",x-2,y-2.3,4,4.6,10.5,2,
                  "roof-cooler","metal","data-center")
    m.surface("service-door","BuildingInstallation",[[(-2.2,-11.16,0),(2.2,-11.16,0),
              (2.2,-11.16,3.8),(-2.2,-11.16,3.8)]],["Door"],
              "service-entrance","accent","data-center")
    return m


def turbine_hall() -> Model:
    m=Model("power-station-turbine-hall","Power station turbine hall",
            "Large-span generator hall with clerestory, service doors and roof vents.",
            "User-requested power-station building; Siemens Energy turbine-hall category, schematic geometry")
    m.box("turbine-hall","Building",-22,-11,44,22,0,14,
          "power-station-hall","power-hall-wall",extra={"use":"power-generation"})
    m.box("roof-clerestory","BuildingInstallation",-18,-3,36,6,14,3,
          "roof-clerestory","metal","turbine-hall")
    m.surface("clerestory-glazing","BuildingInstallation",[[(-18,-3.08,14.4),(18,-3.08,14.4),
              (18,-3.08,16.6),(-18,-3.08,16.6)]],["Window"],
              "clerestory-glazing","glass","turbine-hall")
    doors=[]
    for x in (-14,0,14):
        doors.append([(x-3.7,-11.12,0),(x+3.7,-11.12,0),
                      (x+3.7,-11.12,8),(x-3.7,-11.12,8)])
    m.surface("equipment-doors","BuildingInstallation",doors,["Door"]*3,
              "equipment-access","door","turbine-hall")
    for x in (-14,0,14):
        m.box(f"roof-vent-{x}","BuildingInstallation",x-1.2,6,2.4,2.4,14,2.1,
              "roof-vent","dark","turbine-hall")
    return m


def station_control() -> Model:
    m=Model("power-station-control-building","Power station control building",
            "Two-storey control and operations building with a glazed front and antenna.",
            "User-requested power-station building; original illustrative geometry")
    m.box("control-building","Building",-12,-7,24,14,0,9,
          "power-station-control","control-wall",extra={"use":"power-station-control"})
    windows(m,"control-building",24,14,2,7,3.4,1.7)
    m.surface("control-entrance","BuildingInstallation",[[(-2,-7.1,0),(2,-7.1,0),
              (2,-7.1,3.3),(-2,-7.1,3.3)]],["Door"],
              "public-entrance","glass","control-building")
    m.box("entrance-canopy","BuildingInstallation",-4,-8.5,8,1.7,3.4,.35,
          "entrance-canopy","accent","control-building")
    m.box("communications-antenna","BuildingInstallation",6,2,.14,.14,9,5,
          "communications-antenna","metal","control-building")
    return m


def wind_turbine() -> Model:
    m=Model("wind-turbine","Wind turbine generator",
            "Tapered tower, nacelle, hub and three visible rotor blades.",
            "User-requested wind generator; Vestas tower/nacelle/hub/blade categories, illustrative scale")
    m.box("foundation","CityFurniture",-2.3,-2.3,4.6,4.6,0,.45,
          "turbine-foundation","pavement")
    lower=[(-1.1,-1.1,.45),(1.1,-1.1,.45),(1.1,1.1,.45),(-1.1,1.1,.45)]
    upper=[(-.57,-.57,22),(.57,-.57,22),(.57,.57,22),(-.57,.57,22)]
    faces=[list(reversed(lower))]+[[lower[i],lower[(i+1)%4],upper[(i+1)%4],upper[i]]
                                    for i in range(4)]+[upper]
    m.solid("tower","GenericCityObject",faces,
            ["GroundSurface"]+["WallSurface"]*4+["RoofSurface"],
            "wind-tower","tower-metal")
    m.box("nacelle","GenericCityObject",-1.25,-1.3,2.5,4.7,22,2.1,
          "wind-nacelle","metal")
    m.box("rotor-hub","GenericCityObject",-.58,-2.05,1.16,.82,22.47,1.16,
          "wind-rotor-hub","dark")
    hubz=23.05
    for i,deg in enumerate((90,210,330),1):
        theta=math.radians(deg)
        ux,uz=math.cos(theta),math.sin(theta)
        px,pz=-uz,ux
        r0,r1=1.15,12.2
        poly=[]
        for r,w,side in ((r0,1.1,1),(r1,.24,1),(r1,.24,-1),(r0,1.1,-1)):
            poly.append((ux*r+px*w*side,-2.13,hubz+uz*r+pz*w*side))
        m.surface(f"rotor-blade-{i}","GenericCityObject",[poly],["WallSurface"],
                  "wind-rotor-blade","turbine-blade")
    return m


def diesel_generator() -> Model:
    m=Model("diesel-generator","Diesel generator set",
            "Enclosed backup generator with louvres, service panel, radiator and exhaust.",
            "User-requested fossil-fuel generator; Caterpillar enclosure/radiator/exhaust categories, schematic")
    m.box("generator-skid","CityFurniture",-6,-2.5,12,5,0,.35,
          "generator-skid","dark")
    m.box("diesel-enclosure","GenericCityObject",-5.5,-2,11,4,.35,3.5,
          "diesel-generator","generator-enclosure")
    vents=[]
    for x in (-4,-2,0,2):
        vents.append([(x,-2.08,1.1),(x+1.3,-2.08,1.1),
                      (x+1.3,-2.08,2.8),(x,-2.08,2.8)])
    m.surface("air-intake-louvres","GenericCityObject",vents,["WallSurface"]*4,
              "air-intake","dark")
    m.surface("radiator-grille","GenericCityObject",[[(5.58,-1.55,.8),(5.58,1.55,.8),
              (5.58,1.55,3.4),(5.58,-1.55,3.4)]],["WallSurface"],
              "radiator-grille","metal")
    m.box("exhaust-stack","CityFurniture",-3.8,.75,.75,.75,3.85,3,
          "generator-exhaust","exhaust")
    m.surface("control-panel","GenericCityObject",[[(3.15,-2.12,1.4),(4.75,-2.12,1.4),
              (4.75,-2.12,2.8),(3.15,-2.12,2.8)]],["WallSurface"],
              "generator-control-panel","glass")
    return m


def gas_turbine_generator() -> Model:
    m=Model("gas-turbine-generator","Gas turbine generator package",
            "Long skid with filter house, turbine enclosure, generator housing and exhaust stack.",
            "User-requested fossil-fuel generator; Siemens Energy packaged gas-turbine layout, schematic")
    m.box("turbine-skid","CityFurniture",-10,-3.5,20,7,0,.45,
          "generator-skid","dark")
    m.box("air-filter-house","GenericCityObject",-9,-2.7,4,5.4,.45,4.9,
          "air-filter-house","metal")
    m.box("gas-turbine-enclosure","GenericCityObject",-4.6,-2.5,8.3,5,.45,3.8,
          "gas-turbine","generator-enclosure")
    m.box("generator-housing","GenericCityObject",4,-2.15,5.2,4.3,.45,3.45,
          "electric-generator","storage")
    louvres=[]
    for x in (-8.5,-7.4,-6.3):
        louvres.append([(x,-2.78,1.1),(x+.7,-2.78,1.1),
                        (x+.7,-2.78,4.6),(x,-2.78,4.6)])
    m.surface("filter-louvres","GenericCityObject",louvres,["WallSurface"]*3,
              "air-intake","dark")
    m.box("exhaust-stack","CityFurniture",-.9,.2,2.1,2.1,4.25,6.1,
          "turbine-exhaust","exhaust")
    m.box("control-cabinet","CityFurniture",5.2,-2.65,1.3,.7,.45,2.5,
          "generator-controls","accent")
    return m


def hv_transformer() -> Model:
    m=Model("hv-transformer","HV power transformer",
            "Large transformer tank with radiator banks, top bushings and conservator on a utility pad.",
            "User-requested HV transformer; Siemens Energy tank/cooling categories, illustrative equipment")
    m.box("transformer-pad","CityFurniture",-8,-5.5,16,11,0,.35,
          "transformer-pad","pavement")
    m.box("hv-tank","GenericCityObject",-4.8,-2.4,9.6,4.8,.35,4.2,
          "hv-transformer-tank","hv-transformer")
    for side in (-1,1):
        y=-3.9 if side<0 else 3.15
        for i,x in enumerate((-3.8,-2.6,-1.4,-.2,1,2.2,3.4),1):
            m.box(f"radiator-{side}-{i}","CityFurniture",x,y,.38,.75,.85,3.3,
                  "cooling-radiator","metal")
    for i,x in enumerate((-2.7,0,2.7),1):
        m.box(f"hv-bushing-{i}","CityFurniture",x-.36,-.4,.72,.8,4.55,3.2,
              "hv-bushing","insulator")
        m.box(f"bushing-terminal-{i}","CityFurniture",x-.11,-.15,.22,.3,7.75,.55,
              "bushing-terminal","metal")
    m.box("conservator","CityFurniture",-3.1,2.75,6.2,1.0,5.4,1.25,
          "oil-conservator","metal")
    m.box("control-box","CityFurniture",5,-2.1,1.4,1.6,.35,2.3,
          "transformer-control-box","dark")
    return m


def charging_pole() -> Model:
    m=Model("ev-charging-pole","EV charging pole","Freestanding charging post with display, connector and plinth.","2020 paper Figure 1: private/public charging stations")
    m.box("pole-pad","CityFurniture",-1,-1,2,2,0,.16,"charging-pad","pavement")
    m.box("charging-pole","CityFurniture",-.37,-.27,.74,.54,.16,2.05,"ev-charger","charger",extra={"ports":1})
    m.surface("pole-screen","GenericCityObject",[[(-.25,-.315,1.35),(.25,-.315,1.35),(.25,-.315,1.75),(-.25,-.315,1.75)]],
              ["WallSurface"],"charger-screen","glass")
    m.box("pole-hook","CityFurniture",.36,-.17,.18,.32,.8,.4,"connector-holster","dark")
    return m


def car_parts(m: Model, prefix: str, x: float, y: float) -> None:
    m.box(prefix,"GenericCityObject",x-2.1,y-.92,4.2,1.84,.45,.72,"e-car","car-body")
    m.box(prefix+"-cabin","GenericCityObject",x-.95,y-.79,2.15,1.58,1.17,.78,"vehicle-cabin","glass")
    for side in (-1,1):
        for axle in (-1.3,1.25):
            m.box(f"{prefix}-wheel-{side}-{axle}","CityFurniture",x+axle-.34,y+side*.89-.13,.68,.26,.12,.7,
                  "wheel","dark")


def car() -> Model:
    m=Model("e-car","Electric car","Compact EV visual cue; no vehicle brand.","2020 paper: residents' e-cars")
    car_parts(m,"e-car",0,0)
    return m


def charging_hub() -> Model:
    m=Model("ev-charging-hub-five-bay","Five-bay EV charging hub",
            "Five separate posts on a marked row of parking bays, with two illustrative cars.",
            "2020 paper §II.B and Figure 1: five charging stations")
    m.surface("parking-court","TransportSquare",[rect(-19,-8,19,8,.01)],
              ["TrafficArea"],"parking-court","pavement")
    marks=[]
    for x in (-18.5,-11.5,-4.5,2.5,9.5,16.5):
        marks.append([(x-.06,-7,.035),(x+.06,-7,.035),(x+.06,4,.035),(x-.06,4,.035)])
    m.surface("bay-marks","Road",marks,["Marking"]*len(marks),"parking-marking","marking")
    for i in range(5):
        x=-15+7*i
        m.box(f"charger-{i+1}","CityFurniture",x-.34,4.85,.68,.58,.12,2.1,
              "ev-charger","charger",extra={"bay":i+1,"ports":1})
        m.surface(f"charger-{i+1}-screen","GenericCityObject",[[
                  (x-.22,4.79,1.25),(x+.22,4.79,1.25),(x+.22,4.79,1.66),(x-.22,4.79,1.66)]],
                  ["WallSurface"],"charger-screen","glass")
    car_parts(m,"car-a",-15,-1.4)
    car_parts(m,"car-b",6,-1.4)
    return m


def substation() -> Model:
    m=Model("distribution-substation","Distribution substation",
            "Dark transformer enclosure on its own fenced utility parcel, distinct from the road.",
            "2020 paper §II.B: 630 kVA distribution transformer in Figure 1")
    m.surface("substation-parcel","LandUse",[rect(-10,-8,10,8,.01)],
              ["GroundSurface"],"utility-parcel","pavement")
    m.box("transformer","Building",-6,-4,12,8,.12,5.8,"distribution-transformer","transformer",
          extra={"scenarioRatingKVA":630,"ratingIsIllustrativeLabel":True})
    m.box("transformer-vent-a","BuildingInstallation",-4,-4.12,2.2,.18,2.2,2.1,"vent","metal","transformer")
    m.box("transformer-vent-b","BuildingInstallation",1.8,-4.12,2.2,.18,2.2,2.1,"vent","metal","transformer")
    for oid,x,y,w,d in (("fence-back",-9,-7.2,18,.18),("fence-left",-9,-7.2,.18,14.4),
                        ("fence-right",8.82,-7.2,.18,14.4),("fence-front-left",-9,7.02,6.6,.18),
                        ("fence-front-right",2.4,7.02,6.6,.18)):
        m.box(oid,"CityFurniture",x,y,w,d,.05,1.3,"utility-fence","metal")
    return m


def cabinet() -> Model:
    m=Model("lv-distribution-cabinet","LV distribution cabinet",
            "Street-side low-voltage feeder cabinet on a small pad.","BIFROST power-grid equipment concept")
    m.box("cabinet-pad","CityFurniture",-1.5,-1.1,3,2.2,0,.2,"utility-pad","pavement")
    m.box("cabinet","CityFurniture",-1,-.55,2,1.1,.2,2.45,"lv-distribution-cabinet","metal")
    m.surface("cabinet-doors","GenericCityObject",[[(-.9,-.59,.45),(-.06,-.59,.45),(-.06,-.59,2.35),(-.9,-.59,2.35)],
              [(.06,-.59,.45),(.9,-.59,.45),(.9,-.59,2.35),(.06,-.59,2.35)]],
              ["WallSurface"]*2,"cabinet-door","accent")
    return m


def battery() -> Model:
    m=Model("battery-storage","Battery storage container",
            "Ventilated modular battery container with adjacent inverter cabinet.",
            "BIFROST smart-energy and storage scenario category")
    m.box("battery-container","GenericCityObject",-4,-1.8,7,3.6,0,3.1,"battery-storage","storage")
    m.box("battery-inverter","CityFurniture",3.25,-1.15,1.2,2.3,0,2.65,"battery-inverter","metal")
    vents=[]
    for x in (-3,-1.7,-.4,.9):vents.append([(x,-1.86,1.2),(x+.85,-1.86,1.2),(x+.85,-1.86,2.5),(x,-1.86,2.5)])
    m.surface("battery-vents","GenericCityObject",vents,["WallSurface"]*4,"vent","dark")
    return m


def battery_substation() -> Model:
    m=Model("battery-substation","Battery substation",
            "Three battery modules, inverter, switchgear and transformer inside a utility parcel.",
            "User-requested smart-grid extension; illustrative, not a Figure 1 structure")
    m.surface("battery-yard","LandUse",[rect(-13,-9,13,9,.01)],
              ["GroundSurface"],"utility-parcel","pavement")
    for i,x in enumerate((-9,-2.5,4),1):
        m.box(f"battery-module-{i}","GenericCityObject",x,-5,5,8,.12,3.4,
              "battery-module","storage",extra={"module":i})
        m.surface(f"battery-vents-{i}","GenericCityObject",[[
                  (x+.6,-5.09,1.2),(x+4.4,-5.09,1.2),
                  (x+4.4,-5.09,2.8),(x+.6,-5.09,2.8)]],
                  ["WallSurface"],"battery-vent","dark")
    m.box("battery-inverter","CityFurniture",-8,4.1,5,2.7,.12,3,
          "battery-inverter","metal")
    m.box("battery-switchgear","CityFurniture",1,4.1,5,2.7,.12,3,
          "switchgear","transformer")
    m.box("battery-transformer","CityFurniture",7.2,4.1,3.8,2.7,.12,3.3,
          "distribution-transformer","dark")
    for oid,x,y,w,d in (("yard-fence-back",-12,-8,24,.15),
                        ("yard-fence-left",-12,-8,.15,16),
                        ("yard-fence-right",11.85,-8,.15,16),
                        ("yard-fence-front-left",-12,7.85,9,.15),
                        ("yard-fence-front-right",3,7.85,9,.15)):
        m.box(oid,"CityFurniture",x,y,w,d,.02,1.25,"utility-fence","metal")
    return m


def carport() -> Model:
    m=Model("solar-carport","Solar carport",
            "Four-column parking canopy with a photovoltaic roof plane.",
            "2020 paper: PV-based feeders; BIFROST renewable-energy settlement concepts")
    m.box("carport","Building",-8,-4,16,8,4.2,.35,"solar-carport","metal",extra={"use":"parking"})
    for x in (-7.4,7.0):
        for y in (-3.3,2.9):
            m.box(f"post-{x}-{y}","BuildingInstallation",x,y,.45,.45,0,4.5,
                  "carport-post","metal","carport")
    m.surface("carport-pv","BuildingInstallation",[rect(-7.2,-3.2,7.2,3.2,4.62)],
              ["RoofSurface"],"photovoltaic-array","solar","carport")
    return m


def grid_pole() -> Model:
    m=Model("grid-pole","Grid connection pole","Simple overhead-line support and insulators.",
            "BIFROST power-grid layer; illustrative equipment")
    m.box("pole","CityFurniture",-.28,-.28,.56,.56,0,8,"grid-pole","metal")
    m.box("crossarm","CityFurniture",-2.4,-.2,4.8,.4,7.2,.35,"crossarm","dark")
    for x in (-1.7,0,1.7):
        m.box(f"insulator-{x}","CityFurniture",x-.15,-.16,.3,.32,7.55,.5,"insulator","accent")
    return m


def road() -> Model:
    m=Model("road-straight","Road segment","Straight 24 m street tile with sidewalks and markings.",
            "2020 paper Figure 1: settlement roads; current BIFROST settlement builder")
    m.surface("road","Road",[rect(-12,-3.2,12,3.2,.02),rect(-12,-5.5,12,-3.2,.05),
                              rect(-12,3.2,12,5.5,.05)],
              ["TrafficArea","AuxiliaryTrafficArea","AuxiliaryTrafficArea"],
              "road","pavement")
    marks=[rect(x, -.11, x+2.2, .11, .07) for x in (-10,-4,2,8)]
    m.surface("centre-markings","Road",marks,["Marking"]*len(marks),"lane-marking","marking")
    return m


def tapered_plant(m: Model, oid: str, x: float, y: float, z: float,
                  bottom: float, top: float, height: float,
                  role: str, palette: str, segments: int = 12,
                  typ: str = "SolitaryVegetationObject") -> None:
    a=2*math.pi/segments
    lo=[(x+bottom*math.cos(i*a),y+bottom*math.sin(i*a),z) for i in range(segments)]
    hi=[(x+top*math.cos(i*a),y+top*math.sin(i*a),z+height) for i in range(segments)]
    faces=[list(reversed(lo))]+[[lo[i],lo[(i+1)%segments],hi[(i+1)%segments],hi[i]]
                                for i in range(segments)]+[hi]
    m.solid(oid,typ,faces,
            ["GroundSurface"]+["WallSurface"]*segments+["RoofSurface"],role,palette)


def canopy(m: Model, oid: str, x: float, y: float, z: float,
           rx: float, ry: float, rz: float, palette: str,
           role: str = "tree-canopy", segments: int = 12) -> None:
    levels=((-0.9,.32),(-.55,.78),(0,1),(.55,.78),(.9,.32))
    rings=[]
    for up,rad in levels:
        rings.append([(x+rx*rad*math.cos(i*2*math.pi/segments),
                       y+ry*rad*math.sin(i*2*math.pi/segments),z+rz*up)
                      for i in range(segments)])
    faces=[list(reversed(rings[0]))]
    for lower,upper in zip(rings,rings[1:]):
        faces.extend([[lower[i],lower[(i+1)%segments],upper[(i+1)%segments],upper[i]]
                      for i in range(segments)])
    faces.append(rings[-1])
    m.solid(oid,"SolitaryVegetationObject",faces,
            ["GroundSurface"]+["WallSurface"]*(segments*4)+["RoofSurface"],role,palette)


def deciduous_tree() -> Model:
    m=Model("foliage-deciduous-tree","Deciduous tree",
            "Broad shade tree with a round, lobed crown and visible trunk.",
            "User-requested settlement foliage; original illustrative plant")
    tapered_plant(m,"tree-trunk",0,0,0,.48,.30,5.5,"tree-trunk","bark")
    canopy(m,"main-crown",0,0,7.1,3.2,3.0,3.0,"leaf-green")
    canopy(m,"crown-left",-2.35,.3,6.6,2.1,2.1,1.9,"leaf-light")
    canopy(m,"crown-right",2.4,-.3,6.8,2,2.1,1.9,"leaf-dark")
    return m


def conifer_tree() -> Model:
    m=Model("foliage-conifer-tree","Conifer tree",
            "Narrow evergreen with a trunk and three tapered branch tiers.",
            "User-requested settlement foliage; original illustrative plant")
    tapered_plant(m,"conifer-trunk",0,0,0,.38,.20,9.3,"tree-trunk","bark")
    for i,(z,r,h) in enumerate(((2.4,2.7,4.2),(4.7,2.2,3.7),(6.8,1.6,3.2)),1):
        tapered_plant(m,f"branch-tier-{i}",0,0,z,r,.12,h,
                      "conifer-foliage","leaf-dark" if i<3 else "leaf-green")
    return m


def street_tree() -> Model:
    m=Model("foliage-street-tree","Street tree",
            "Raised clear trunk, compact crown and square tree pit for sidewalks.",
            "User-requested urban foliage; original illustrative plant")
    m.surface("tree-pit","PlantCover",[rect(-1.3,-1.3,1.3,1.3,.02)],
              ["GroundSurface"],"tree-pit","soil")
    tapered_plant(m,"street-trunk",0,0,.02,.34,.22,4.7,"tree-trunk","bark")
    canopy(m,"street-crown",0,0,6.7,2.75,2.5,2.5,"leaf-green")
    return m


def orchard_tree() -> Model:
    m=Model("foliage-orchard-tree","Orchard tree",
            "Low spreading fruit tree with a short trunk and visible fruit accents.",
            "User-requested village foliage; original illustrative plant")
    tapered_plant(m,"orchard-trunk",0,0,0,.31,.24,2.7,"tree-trunk","bark")
    canopy(m,"orchard-crown",0,0,4.3,3.3,2.7,2.2,"leaf-light")
    for i,(x,y,z) in enumerate(((-2.6,-1.25,3.9),(-1.45,-2.2,4.2),(-.25,-2.6,3.7),
                                (1.2,-2.25,4.4),(2.45,-1.4,3.9),(-.4,1.9,4.1)),1):
        canopy(m,f"fruit-{i}",x,y,z,.23,.23,.23,"fruit",role="fruit",segments=8)
    return m


def shrub_cluster() -> Model:
    m=Model("foliage-shrub-cluster","Shrub cluster",
            "Five overlapping low bushes for garden and verge planting.",
            "User-requested settlement foliage; original illustrative plant")
    for i,(x,y,rx,ry,h) in enumerate(((-1.7,-.3,1.15,1,.8),(-.7,.45,1.3,1.1,1.1),
                                      (.65,-.2,1.25,1,1),(1.7,.4,1.1,.9,.75),
                                      (0,1.05,1.05,.9,.85)),1):
        canopy(m,f"shrub-{i}",x,y,h,rx,ry,h,
               "leaf-dark" if i%2 else "leaf-light",role="shrub-foliage")
    return m


def hedge() -> Model:
    m=Model("foliage-hedge-segment","Hedge segment",
            "Clipped eight-metre hedge with a softly uneven planted top.",
            "User-requested settlement foliage; original illustrative plant")
    m.box("hedge-body","PlantCover",-4,-.65,8,1.3,0,1.7,
          "hedge","leaf-dark")
    for i,x in enumerate((-3.2,-1.6,0,1.6,3.2),1):
        canopy(m,f"hedge-top-{i}",x,0,1.65,1,.75,.43,
               "leaf-green",role="hedge-top",segments=8)
    return m


def grass_patch() -> Model:
    m=Model("foliage-grass-patch","Grass patch",
            "Four-metre planting tile with visible grass tufts for ground cover.",
            "User-requested settlement foliage; original illustrative plant")
    m.surface("grass-ground","PlantCover",[rect(-2,-2,2,2,.01)],
              ["GroundSurface"],"grass-ground","grass")
    blades=[]
    for row in range(5):
        for col in range(5):
            x=-1.62+col*.78+(row%2)*.16
            y=-1.58+row*.76
            h=.22+.08*((row+2*col)%3)
            blades.append([(x-.12,y,.02),(x+.12,y,.02),(x,y,h)])
            blades.append([(x,y-.12,.02),(x,y+.12,.02),(x,y,h)])
    m.surface("grass-tufts","PlantCover",blades,["WallSurface"]*len(blades),
              "grass-tuft","leaf-light")
    return m


def flower_bed() -> Model:
    m=Model("foliage-flower-bed","Flower bed",
            "Small bordered bed with stems, foliage and colored blossoms.",
            "User-requested settlement foliage; original illustrative plant")
    m.box("bed-border","CityFurniture",-2.5,-1,5,2,0,.17,
          "flower-border","stone")
    m.surface("bed-soil","PlantCover",[rect(-2.3,-.8,2.3,.8,.18)],
              ["GroundSurface"],"flower-soil","soil")
    for i,(x,y) in enumerate(((-1.8,-.35),(-1.2,.35),(-.5,-.25),(.2,.4),
                               (.9,-.35),(1.6,.3),(2,-.3)),1):
        m.box(f"flower-stem-{i}","SolitaryVegetationObject",x-.035,y-.035,.07,.07,.18,.48,
              "flower-stem","leaf-dark")
        canopy(m,f"flower-head-{i}",x,y,.72,.22,.22,.19,
               "flower" if i%2 else "fruit",role="flower-head",segments=8)
    return m


def planter_tree() -> Model:
    m=Model("foliage-planter-tree","Planter tree",
            "Small ornamental tree in a square raised planter for plazas and courtyards.",
            "User-requested urban foliage; original illustrative plant")
    m.box("planter","CityFurniture",-1,-1,2,2,0,.75,"planter","stone")
    m.surface("planter-soil","PlantCover",[rect(-.82,-.82,.82,.82,.76)],
              ["GroundSurface"],"planter-soil","soil")
    tapered_plant(m,"planter-trunk",0,0,.76,.22,.15,2.65,"tree-trunk","bark")
    canopy(m,"planter-crown",0,0,4.15,1.8,1.7,1.55,"leaf-light")
    return m


def round_sign(m: Model, oid: str, x: float, y: float, z: float,
               radius: float, palette: str, sides: int = 8) -> None:
    poly=[(x+radius*math.cos(2*math.pi*i/sides+math.pi/8),y,
           z+radius*math.sin(2*math.pi*i/sides+math.pi/8)) for i in range(sides)]
    m.surface(oid,"CityFurniture",[poly],["WallSurface"],"traffic-sign-face",palette)


def traffic_light() -> Model:
    m=Model("street-traffic-light","Traffic light",
            "Roadside pole with a three-lamp signal head and small service box.",
            "User-requested public infrastructure; illustrative signal, no jurisdictional claim")
    tapered_plant(m,"signal-pole",0,.2,0,.18,.12,5.2,
                  "signal-pole","metal",typ="CityFurniture")
    m.box("signal-head","CityFurniture",-.43,-.55,.86,.58,3.35,1.78,
          "signal-head","signal-housing")
    for name,z,palette in (("red",4.76,"lamp-red"),("amber",4.22,"lamp-amber"),
                           ("green",3.68,"lamp-green")):
        round_sign(m,f"signal-{name}",0,-.59,z,.18,palette,12)
    m.box("signal-control-box","CityFurniture",-.34,-.04,.68,.48,.7,1.02,
          "signal-control","metal")
    return m


def street_light() -> Model:
    m=Model("street-light","Street light",
            "Tall pole with a cantilevered luminaire over the street.",
            "User-requested public infrastructure; original illustrative lamp")
    tapered_plant(m,"lamp-pole",0,0,0,.22,.13,7.3,
                  "street-light-pole","metal",typ="CityFurniture")
    m.box("lamp-arm","CityFurniture",0,-.18,2.7,.28,7.0,.22,
          "lamp-arm","metal")
    m.box("luminaire","CityFurniture",2.1,-.21,1.5,.52,6.94,.28,
          "luminaire","signal-housing")
    m.surface("lamp-lens","CityFurniture",[[(2.23,-.235,6.98),(3.45,-.235,6.98),
              (3.45,-.235,7.09),(2.23,-.235,7.09)]],
              ["WallSurface"],"lamp-lens","lamp-light")
    return m


def solar_street_light() -> Model:
    m=Model("solar-street-light","Solar street light",
            "Street lamp with a dedicated tilted PV panel and pole battery cabinet.",
            "User-requested public infrastructure; original illustrative solar lamp")
    tapered_plant(m,"solar-lamp-pole",0,0,0,.24,.14,7.6,
                  "street-light-pole","metal",typ="CityFurniture")
    m.box("solar-lamp-arm","CityFurniture",0,-.18,2.7,.28,7.05,.22,
          "lamp-arm","metal")
    m.box("solar-luminaire","CityFurniture",2.1,-.21,1.5,.52,6.99,.28,
          "luminaire","signal-housing")
    m.surface("solar-lens","CityFurniture",[[(2.23,-.235,7.03),(3.45,-.235,7.03),
              (3.45,-.235,7.14),(2.23,-.235,7.14)]],
              ["WallSurface"],"lamp-lens","lamp-light")
    m.surface("street-pv-panel","CityFurniture",[[(-1.0,-1.2,8.0),(1.3,-1.2,8.0),
              (1.3,1.1,8.55),(-1.0,1.1,8.55)]],["RoofSurface"],
              "photovoltaic-panel","solar")
    m.box("solar-battery","CityFurniture",-.35,.2,.7,.55,4.0,1.0,
          "pole-battery","storage")
    return m


def stop_sign() -> Model:
    m=Model("traffic-sign-stop","Stop sign",
            "Red octagonal sign on a slim roadside post; illustrative without lettering.",
            "User-requested traffic sign; not a jurisdiction-specific regulatory asset")
    tapered_plant(m,"stop-post",0,0,0,.10,.08,2.75,
                  "sign-post","metal",typ="CityFurniture")
    round_sign(m,"stop-face",0,-.15,2.55,.66,"sign-red",8)
    round_sign(m,"stop-inset",0,-.17,2.55,.47,"sign-white",8)
    round_sign(m,"stop-centre",0,-.19,2.55,.38,"sign-red",8)
    return m


def warning_sign() -> Model:
    m=Model("traffic-sign-warning","Warning sign",
            "Triangular amber warning marker with a dark exclamation cue.",
            "User-requested traffic sign; illustrative and jurisdiction-neutral")
    tapered_plant(m,"warning-post",0,0,0,.10,.08,2.7,
                  "sign-post","metal",typ="CityFurniture")
    m.surface("warning-face","CityFurniture",[[(-.78,-.16,2.04),(.78,-.16,2.04),
              (0,-.16,3.35)]],["WallSurface"],"warning-face","sign-yellow")
    m.box("warning-mark","CityFurniture",-.05,-.19,.1,.03,2.48,.48,
          "warning-mark","dark")
    m.box("warning-dot","CityFurniture",-.07,-.19,.14,.03,2.3,.12,
          "warning-mark","dark")
    return m


def direction_sign() -> Model:
    m=Model("traffic-sign-direction","Direction sign",
            "Blue wayfinding sign with a simple white arrow on two posts.",
            "User-requested street sign; illustrative without place names")
    for x in (-1.45,1.45):
        tapered_plant(m,f"direction-post-{x}",x,0,0,.10,.08,3.1,
                      "sign-post","metal",typ="CityFurniture")
    m.box("direction-board","CityFurniture",-2,-.19,4,.28,2.25,1.15,
          "direction-board","sign-blue")
    m.surface("direction-arrow","CityFurniture",[[(-1.35,-.34,2.7),(.55,-.34,2.7),
              (.55,-.34,2.95),(1.45,-.34,2.82),(.55,-.34,2.4),
              (.55,-.34,2.57),(-1.35,-.34,2.57)]],["WallSurface"],
              "direction-arrow","sign-white")
    return m


def pedestrian_crossing() -> Model:
    m=Model("street-pedestrian-crossing","Pedestrian crossing",
            "Twelve-metre street tile with zebra markings and sidewalks.",
            "User-requested street markings; illustrative geometry, not a road standard")
    m.surface("crossing-road","Road",[rect(-6,-3.7,6,3.7,.02),
              rect(-6,-5,6,-3.7,.05),rect(-6,3.7,6,5,.05)],
              ["TrafficArea","AuxiliaryTrafficArea","AuxiliaryTrafficArea"],
              "street-crossing","pavement")
    marks=[rect(x,-3.3,x+.62,3.3,.07) for x in (-4.8,-3.55,-2.3,-1.05,.2,1.45,2.7,3.95)]
    m.surface("zebra-stripes","Road",marks,["Marking"]*len(marks),
              "pedestrian-marking","sign-white")
    return m


def bus_stop() -> Model:
    m=Model("bus-stop-shelter","Bus stop shelter",
            "Three-sided transparent shelter with a roof, seat and stop marker.",
            "User-requested public infrastructure; original illustrative shelter")
    m.surface("bus-stop-pad","TransportSquare",[rect(-3.8,-1.8,3.8,1.8,.02)],
              ["AuxiliaryTrafficArea"],"bus-stop-pad","pavement")
    for x in (-3,3):
        for y in (-1.1,1.15):
            m.box(f"shelter-post-{x}-{y}","CityFurniture",x,y,.12,.12,.02,2.8,
                  "shelter-post","metal")
    m.box("shelter-roof","CityFurniture",-3.3,-1.4,6.6,2.8,2.82,.24,
          "shelter-roof","signal-housing")
    m.surface("shelter-back","CityFurniture",[[(-3,-1.15,.25),(3,-1.15,.25),
              (3,-1.15,2.65),(-3,-1.15,2.65)]],["Window"],
              "shelter-glazing","glass")
    m.surface("shelter-side","CityFurniture",[[(3.08,-1.1,.25),(3.08,1.15,.25),
              (3.08,1.15,2.65),(3.08,-1.1,2.65)]],["Window"],
              "shelter-glazing","glass")
    m.box("shelter-seat","CityFurniture",-2.25,-.72,3.6,.55,.55,.13,
          "shelter-bench","wood")
    for x in (-1.95,.95):
        m.box(f"seat-leg-{x}","CityFurniture",x,-.55,.12,.12,.02,.55,
              "bench-leg","metal")
    m.box("stop-marker","CityFurniture",-3.62,-.9,.27,.27,1.65,1.1,
          "stop-marker","sign-blue")
    return m


def park_bench() -> Model:
    m=Model("park-bench","Park bench",
            "Wood-slat seat with a backrest, arms and metal frame.",
            "User-requested public furniture; original illustrative bench")
    for i,z in enumerate((.53,.62,.71),1):
        m.box(f"seat-slat-{i}","CityFurniture",-1.35,-.38,2.7,.24,z,.07,
              "seat-slat","wood")
    for i,z in enumerate((.88,1.06,1.24),1):
        m.box(f"back-slat-{i}","CityFurniture",-1.35,.32,2.7,.07,z,.13,
              "back-slat","wood")
    for x in (-1.15,1.15):
        m.box(f"bench-leg-{x}","CityFurniture",x,-.23,.12,.6,0,.7,
              "bench-leg","metal")
        m.box(f"armrest-{x}","CityFurniture",x,-.42,.12,.88,.77,.09,
              "armrest","metal")
    return m


def public_bin() -> Model:
    m=Model("public-waste-bin","Public waste bin",
            "Freestanding street bin with a dark opening and colored body.",
            "User-requested public furniture; original illustrative bin")
    m.box("bin-body","CityFurniture",-.38,-.38,.76,.76,0,1.08,
          "waste-bin","bin")
    m.box("bin-rim","CityFurniture",-.42,-.42,.84,.84,1.08,.08,
          "bin-rim","metal")
    m.surface("bin-opening","CityFurniture",[rect(-.26,-.26,.26,.26,1.17)],
              ["RoofSurface"],"bin-opening","dark")
    return m


def bollard() -> Model:
    m=Model("street-bollard","Street bollard",
            "Short protective post with a reflective band and small base plate.",
            "User-requested public furniture; original illustrative bollard")
    m.box("bollard-base","CityFurniture",-.28,-.28,.56,.56,0,.08,
          "bollard-base","metal")
    tapered_plant(m,"bollard-post",0,0,.08,.16,.16,.96,
                  "street-bollard","dark",typ="CityFurniture")
    tapered_plant(m,"reflective-band",0,0,.72,.165,.165,.14,
                  "bollard-reflector","reflective",typ="CityFurniture")
    return m


def bicycle_rack() -> Model:
    m=Model("bicycle-rack","Bicycle rack",
            "Three inverted-U parking loops on a small paved strip.",
            "User-requested public furniture; original illustrative rack")
    m.surface("rack-pad","TransportSquare",[rect(-2.8,-1.4,2.8,1.4,.02)],
              ["AuxiliaryTrafficArea"],"bicycle-rack-pad","pavement")
    for i,x in enumerate((-1.8,0,1.8),1):
        m.box(f"rack-left-{i}","CityFurniture",x-.38,-.15,.1,.1,.02,1.06,
              "rack-loop","metal")
        m.box(f"rack-right-{i}","CityFurniture",x+.28,-.15,.1,.1,.02,1.06,
              "rack-loop","metal")
        m.box(f"rack-top-{i}","CityFurniture",x-.38,-.15,.76,.1,1.08,.11,
              "rack-loop","metal")
    return m


FACTORIES = [detached,terrace,apartment,office,office_tower,city_hall,event_hall,
             shop,factory_pv,warehouse,assembly_factory,data_center,turbine_hall,
             station_control,wind_turbine,diesel_generator,gas_turbine_generator,
             hv_transformer,charging_pole,charging_hub,car,substation,cabinet,battery,
             battery_substation,carport,grid_pole,road,deciduous_tree,conifer_tree,
             street_tree,orchard_tree,shrub_cluster,hedge,grass_patch,flower_bed,planter_tree,
             traffic_light,street_light,solar_street_light,stop_sign,warning_sign,
             direction_sign,pedestrian_crossing,bus_stop,park_bench,public_bin,
             bollard,bicycle_rack]


def main() -> None:
    DEST.mkdir(parents=True,exist_ok=True)
    manifest={"collection":"smartgrid-cityjson-prefabs","version":1,"illustrative":True,"prefabs":[]}
    for factory in FACTORIES:
        model=factory(); doc=model.finish()
        path=DEST/(model.slug+".city.json")
        path.write_text(json.dumps(doc,separators=(",",":"),ensure_ascii=False)+"\n",encoding="utf-8")
        xs=[p[0] for p in model.vertices]; ys=[p[1] for p in model.vertices]; zs=[p[2] for p in model.vertices]
        manifest["prefabs"].append({
            "id":model.slug,"file":path.name,"title":model.title,
            "description":model.description,"referenceBasis":model.reference,
            "boundsMetres":[min(xs),min(ys),min(zs),max(xs),max(ys),max(zs)],
            "cityObjectCount":len(model.objects),
            "objectTypes":dict(sorted(Counter(o["type"] for o in model.objects.values()).items())),
        })
    (DEST/"manifest.json").write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print(f"Wrote {len(manifest['prefabs'])} CityJSON prefabs to {DEST}")


if __name__ == "__main__":
    main()
