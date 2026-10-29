"""Render a static inspection sheet for the authored smart-grid CityJSON assets."""
from __future__ import annotations

import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import to_rgb
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/"assets/data/city/smartgrid"
OUTPUT=ROOT/"out/smartgrid-cityjson-prefabs-sheet.png"
HUB_OUTPUT=ROOT/"out/smartgrid-ev-charging-hub-detail.png"

PALETTE={
    "house-wall":"#d8ccb9","apartment-wall":"#a8b9ba","office-wall":"#7196a7",
    "shop-wall":"#ceb6a2","factory-wall":"#e3e2dc","warehouse-wall":"#b6bac0",
    "transformer":"#252c36","solar":"#194b75","glass":"#5bb0d2",
    "door":"#665b55","dark":"#2d3540","metal":"#819398","trim":"#75858e",
    "accent":"#edb75f","charger":"#56c8b5","pavement":"#8b9693",
    "marking":"#eee0a7","car-body":"#e1ad5e","storage":"#79939e",
    "civic-wall":"#c7c0ae","civic-roof":"#5d7580","event-wall":"#c8d0d1",
    "assembly-wall":"#a4b6bc","data-center-wall":"#718997",
    "power-hall-wall":"#a8b2b7","control-wall":"#bec5c0",
    "hv-transformer":"#455d6f","insulator":"#b38969",
    "tower-metal":"#d7ddda","turbine-blade":"#e0e7e4",
    "generator-enclosure":"#81979b","exhaust":"#6f7b82",
    "bark":"#644c3b","leaf-green":"#4c8451","leaf-light":"#72a25c",
    "leaf-dark":"#31654f","grass":"#5d944f","soil":"#614f3f",
    "stone":"#a9a394","fruit":"#c95b44","flower":"#eeb150",
    "signal-housing":"#344148","lamp-red":"#da3930","lamp-amber":"#de9c31",
    "lamp-green":"#44b269","lamp-light":"#faf0c9","sign-red":"#c0362f",
    "sign-yellow":"#e8b949","sign-blue":"#31699a","sign-white":"#eff0e6",
    "wood":"#8f6946","bin":"#4e6b70","reflective":"#ebe8cc",
}

def surfaces(doc,obj):
    geom=obj["geometry"][0]
    surfaces=geom["boundaries"] if geom["type"]=="MultiSurface" else geom["boundaries"][0]
    sem=geom.get("semantics",{})
    values=sem.get("values",[])
    if geom["type"]=="Solid" and values: values=values[0]
    for i,s in enumerate(surfaces):
        kind=""
        if i<len(values) and values[i] is not None: kind=sem["surfaces"][values[i]]["type"]
        yield [doc["vertices"][n] for n in s[0]],kind

def color(obj,kind):
    role=obj["attributes"]["paletteRole"]
    base=PALETTE.get(role,"#a2aeb1")
    if role in ("house-wall","shop-wall","warehouse-wall") and kind=="RoofSurface":
        base="#b96757"
    elif role=="apartment-wall" and kind=="RoofSurface": base="#647f8d"
    elif role=="factory-wall" and kind=="RoofSurface": base="#f1f1e9"
    elif role=="civic-wall" and kind=="RoofSurface": base="#5d7580"
    elif role=="event-wall" and kind=="RoofSurface": base="#7f9ea9"
    elif role=="assembly-wall" and kind=="RoofSurface": base="#c4cfcd"
    elif role=="data-center-wall" and kind=="RoofSurface": base="#657e8d"
    elif role=="power-hall-wall" and kind=="RoofSurface": base="#698494"
    elif role=="control-wall" and kind=="RoofSurface": base="#6a7f87"
    elif role=="pavement" and obj["type"] in ("Road","TransportSquare"):
        base="#65717a" if kind=="TrafficArea" else "#9ba5a1"
    return to_rgb(base)

def draw(ax,doc,min_span=3.0):
    verts=doc["vertices"]
    xs=[v[0] for v in verts];ys=[v[1] for v in verts];zs=[v[2] for v in verts]
    for obj in doc["CityObjects"].values():
        faces=[]; colors=[]
        for face,kind in surfaces(doc,obj):
            faces.append(face);colors.append(color(obj,kind))
        coll=Poly3DCollection(faces,facecolors=colors,edgecolors=(.12,.17,.2,.65),linewidths=.32)
        role=obj["attributes"].get("smartGridRole", "")
        coll.set_zorder(1 if obj["type"] in ("Road","TransportSquare","LandUse")
                        else 1.5 if role=="utility-fence" else 2)
        ax.add_collection3d(coll)
    dx=max(xs)-min(xs);dy=max(ys)-min(ys);dz=max(zs)-min(zs)
    span=max(dx,dy,min_span)
    cx=(min(xs)+max(xs))/2;cy=(min(ys)+max(ys))/2
    ax.set_xlim(cx-span*.59,cx+span*.59)
    ax.set_ylim(cy-span*.59,cy+span*.59)
    ax.set_zlim(0,max(dz*1.12,span*.28,min_span*.9))
    ax.set_box_aspect((1,1,.57),zoom=1.08)
    ax.view_init(elev=26,azim=-68)
    ax.set_axis_off()
    ax.set_facecolor("#152231")

def main():
    manifest=json.loads((ASSETS/"manifest.json").read_text(encoding="utf-8"))
    rows=(len(manifest["prefabs"])+3)//4
    plt.rcParams.update({"font.family":"DejaVu Sans","font.size":12})
    fig=plt.figure(figsize=(18,4.2*rows+1),facecolor="#0b1422")
    fig.text(.045,.972,"SMART GRID / CITYJSON PREFABS",color="#eaf2f7",fontsize=27,weight="bold")
    fig.text(.045,.949,f"{len(manifest['prefabs'])} original, illustrative models for raymotion  •  2020 ExpCPS use case / BIFROST-inspired roles",
             color="#9db2c0",fontsize=13)
    for i,item in enumerate(manifest["prefabs"]):
        ax=fig.add_subplot(rows,4,i+1,projection="3d",computed_zorder=False)
        doc=json.loads((ASSETS/item["file"]).read_text(encoding="utf-8"))
        draw(ax,doc)
        x0=(i%4)*.24+.045;y0=.902-(i//4)*(.86/rows)
        fig.text(x0,y0,item["title"],color="#eaf2f7",fontsize=13,weight="bold")
        fig.text(x0,y0-.022,item["id"],color="#6bc9bd",fontsize=10)
    fig.subplots_adjust(left=.035,right=.99,bottom=.045,top=.92,wspace=.02,hspace=.16)
    OUTPUT.parent.mkdir(parents=True,exist_ok=True)
    fig.savefig(OUTPUT,dpi=150,facecolor=fig.get_facecolor())
    plt.close(fig)
    hub=json.loads((ASSETS/"ev-charging-hub-five-bay.city.json").read_text(encoding="utf-8"))
    detail=plt.figure(figsize=(10,8),facecolor="#0b1422")
    detail.text(.045,.92,"FIVE-BAY EV CHARGING HUB",color="#eaf2f7",fontsize=24,weight="bold")
    detail.text(.045,.86,"Five individual posts, marked bays and two illustrative e-cars",color="#9db2c0",fontsize=13)
    ax=detail.add_axes([.01,.05,.98,.77],projection="3d",computed_zorder=False)
    draw(ax,hub)
    ax.set_box_aspect((2.4,1,.28),zoom=1.22)
    ax.set_zlim(0,4.5)
    ax.view_init(elev=39,azim=-66)
    detail.savefig(HUB_OUTPUT,dpi=150,facecolor=detail.get_facecolor())
    plt.close(detail)
    print(OUTPUT)
    print(HUB_OUTPUT)

if __name__=="__main__":main()
