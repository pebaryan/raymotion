#include "rmtn/lab3d.hpp"
#include <stdexcept>
#include <string>

namespace rmtn::lab3d {
namespace {

g3d::Material pbr(Color color, float roughness, float metalness = 0, float opacity = 1) {
    g3d::Material m;
    m.color = color;
    m.roughness = roughness;
    m.metalness = metalness;
    m.opacity = opacity;
    m.shading = g3d::Shading::Standard;
    return m;
}

const g3d::Material enamel = pbr({ 201, 213, 216, 255 }, .36f, .30f);
const g3d::Material steel = pbr({ 151, 167, 176, 255 }, .23f, .88f);
const g3d::Material brushed = pbr({ 177, 191, 195, 255 }, .40f, .80f);
const g3d::Material charcoal = pbr({ 37, 48, 57, 255 }, .53f, .12f);
const g3d::Material rubber = pbr({ 34, 40, 44, 255 }, .83f);
const g3d::Material ceramic = pbr({ 229, 234, 228, 255 }, .29f, .03f);
const g3d::Material worktop = pbr({ 97, 107, 110, 255 }, .48f, .08f);
const g3d::Material glass = pbr({ 177, 225, 236, 255 }, .10f, .02f, .34f);
const g3d::Material liquid = pbr({ 66, 164, 180, 255 }, .17f, .02f, .78f);
const g3d::Material screen = pbr({ 47, 136, 160, 255 }, .28f, .02f);
const g3d::Material yellow = pbr({ 227, 180, 70, 255 }, .34f, .22f);
const g3d::Material blue = pbr({ 70, 117, 142, 255 }, .38f, .34f);

g3d::Object3D* piece(g3d::Object3D& parent, const char* name,
                     g3d::GeometryPtr geometry, g3d::Material material, Vector3 centre) {
    auto* node = parent.add(std::move(geometry), material);
    node->name = name;
    node->position = centre;
    return node;
}

g3d::Object3D* box(g3d::Object3D& parent, const char* name,
                   float w, float h, float d, g3d::Material material, Vector3 centre) {
    return piece(parent, name, g3d::Geometry::box(w, h, d), material, centre);
}

g3d::Object3D* cyl(g3d::Object3D& parent, const char* name,
                   float r, float h, g3d::Material material, Vector3 base, int slices = 32) {
    return piece(parent, name, g3d::Geometry::cylinder(r, h, slices), material,
                 { base.x, base.y + h / 2, base.z });
}

g3d::Object3D* ball(g3d::Object3D& parent, const char* name,
                    float radius, g3d::Material material, Vector3 centre) {
    return piece(parent, name, g3d::Geometry::sphere(radius, 20, 32), material, centre);
}

void bench(g3d::Object3D& g) {
    box(g, "worktop", 2.8f, .09f, 1.1f, worktop, { 0, .88f, 0 });
    for (int sx : { -1, 1 }) for (int sz : { -1, 1 }) {
        auto* leg = box(g, "steel-leg", .09f, .84f, .09f, steel,
                        { sx * 1.27f, .42f, sz * .46f });
        leg->name += std::string(sx < 0 ? "-left" : "-right") + (sz < 0 ? "-back" : "-front");
    }
    box(g, "drawer-cabinet", .68f, .60f, .72f, enamel, { -.80f, .54f, -.02f });
    for (int i = 0; i < 3; ++i) {
        box(g, ("drawer-" + std::to_string(i + 1)).c_str(), .59f, .15f, .018f,
            blue, { -.80f, .35f + i * .19f, .35f });
        box(g, ("drawer-pull-" + std::to_string(i + 1)).c_str(), .18f, .025f, .035f, steel,
            { -.80f, .35f + i * .19f, .37f });
    }
    box(g, "sink-basin", .57f, .025f, .40f, charcoal, { .76f, .94f, 0 });
    cyl(g, "tap-post", .023f, .29f, steel, { 1.12f, .93f, -.17f }, 16);
    box(g, "tap-spout", .30f, .025f, .025f, steel, { .98f, 1.21f, -.17f });
}

void hood(g3d::Object3D& g) {
    box(g, "lower-cabinet", 2.0f, .85f, .95f, enamel, { 0, .425f, 0 });
    box(g, "dark-work-surface", 1.85f, .08f, .82f, worktop, { 0, .89f, .01f });
    box(g, "back-panel", 2.0f, 1.43f, .10f, enamel, { 0, 1.635f, -.43f });
    for (int sx : { -1, 1 })
        box(g, sx < 0 ? "left-panel" : "right-panel", .10f, 1.43f, .95f,
            enamel, { sx * .95f, 1.635f, 0 });
    box(g, "hood-canopy", 2.0f, .22f, .95f, enamel, { 0, 2.46f, 0 });
    box(g, "sliding-glass-sash", 1.77f, .68f, .02f, glass, { 0, 1.94f, .475f });
    box(g, "sash-handle", .65f, .025f, .03f, steel, { 0, 1.59f, .50f });
    cyl(g, "exhaust-collar", .22f, .27f, steel, { 0, 2.57f, -.10f });
}

void cabinet(g3d::Object3D& g) {
    box(g, "cabinet-shell", 1.30f, 1.95f, .70f, enamel, { 0, .975f, 0 });
    for (int sx : { -1, 1 }) {
        box(g, sx < 0 ? "left-yellow-door" : "right-yellow-door",
            .59f, 1.70f, .025f, yellow, { sx * .31f, .97f, .365f });
        box(g, sx < 0 ? "left-handle" : "right-handle", .025f, .24f, .045f,
            charcoal, { sx * .07f, 1.02f, .405f });
    }
    for (int i = 0; i < 4; ++i)
        box(g, ("vent-slot-" + std::to_string(i + 1)).c_str(), .68f, .018f, .015f, charcoal,
            { 0, .24f + i * .055f, .388f });
}

void balance(g3d::Object3D& g) {
    box(g, "balance-base", .64f, .14f, .54f, ceramic, { 0, .07f, 0 });
    box(g, "draft-shield", .49f, .38f, .39f, glass, { 0, .33f, -.025f });
    cyl(g, "weighing-pan", .12f, .022f, steel, { 0, .15f, -.025f });
    box(g, "display", .31f, .055f, .011f, screen, { 0, .065f, .278f });
    for (int sx : { -1, 1 })
        cyl(g, sx < 0 ? "level-foot-left" : "level-foot-right", .035f, .025f,
            rubber, { sx * .25f, -.01f, .20f }, 16);
}

void hotplate(g3d::Object3D& g) {
    box(g, "hotplate-base", .52f, .12f, .46f, charcoal, { 0, .06f, 0 });
    cyl(g, "ceramic-heater", .18f, .018f, ceramic, { 0, .12f, -.025f });
    cyl(g, "beaker-liquid", .105f, .10f, liquid, { 0, .147f, -.025f });
    cyl(g, "glass-beaker", .12f, .27f, glass, { 0, .14f, -.025f });
    cyl(g, "control-knob", .038f, .025f, steel, { .18f, .12f, .15f }, 16);
}

void rotavap(g3d::Object3D& g) {
    box(g, "instrument-base", 1.30f, .08f, .70f, charcoal, { 0, .04f, 0 });
    cyl(g, "support-stand", .035f, 1.40f, steel, { -.38f, .08f, -.13f }, 16);
    box(g, "rotary-drive", .39f, .28f, .34f, enamel, { -.19f, 1.28f, .03f });
    cyl(g, "heated-bath", .28f, .25f, yellow, { .26f, .08f, .09f });
    cyl(g, "bath-rim", .29f, .028f, brushed, { .26f, .31f, .09f });
    ball(g, "evaporation-flask", .22f, glass, { .26f, .60f, .09f });
    cyl(g, "flask-neck", .055f, .24f, glass, { .26f, .76f, .09f }, 20);
    cyl(g, "condenser", .09f, .69f, glass, { -.34f, .67f, -.10f }, 20);
    ball(g, "receiving-flask", .14f, glass, { -.34f, .49f, -.10f });
    box(g, "vapour-tube", .48f, .045f, .045f, glass, { -.04f, 1.09f, -.01f });
}

void hplc(g3d::Object3D& g) {
    box(g, "module-stack", .86f, 1.25f, .64f, enamel, { 0, .625f, 0 });
    for (int i = 0; i < 4; ++i) {
        const float y = .16f + i * .30f;
        box(g, ("module-seam-" + std::to_string(i + 1)).c_str(), .80f, .013f, .018f,
            charcoal, { 0, y, .33f });
        box(g, ("status-screen-" + std::to_string(i + 1)).c_str(), .20f, .105f, .012f,
            screen, { .22f, y + .12f, .335f });
        cyl(g, ("status-light-" + std::to_string(i + 1)).c_str(), .015f, .012f,
            yellow, { -.26f, y + .11f, .34f }, 12);
    }
    for (int sx : { -1, 1 }) {
        cyl(g, sx < 0 ? "solvent-bottle-left" : "solvent-bottle-right", .10f, .30f,
            glass, { sx * .18f, 1.25f, -.02f }, 20);
        cyl(g, sx < 0 ? "bottle-cap-left" : "bottle-cap-right", .06f, .055f,
            blue, { sx * .18f, 1.55f, -.02f }, 20);
    }
}

void centrifuge(g3d::Object3D& g) {
    box(g, "centrifuge-housing", .86f, .43f, .70f, enamel, { 0, .215f, 0 });
    cyl(g, "round-rotor-lid", .30f, .045f, charcoal, { 0, .43f, -.04f });
    box(g, "control-display", .57f, .12f, .012f, screen, { 0, .22f, .357f });
    for (int sx : { -1, 1 })
        cyl(g, sx < 0 ? "rubber-foot-left" : "rubber-foot-right", .04f, .035f,
            rubber, { sx * .32f, -.02f, .22f }, 16);
}

void oven(g3d::Object3D& g) {
    box(g, "oven-body", .96f, 1.05f, .78f, enamel, { 0, .525f, 0 });
    box(g, "viewing-window", .64f, .54f, .018f, glass, { -.03f, .53f, .405f });
    box(g, "oven-handle", .035f, .48f, .05f, steel, { .38f, .53f, .445f });
    cyl(g, "top-vent", .13f, .07f, charcoal, { 0, 1.05f, -.08f }, 24);
    box(g, "control-strip", .58f, .06f, .014f, screen, { -.07f, .94f, .408f });
}

void reactor(g3d::Object3D& g) {
    for (int sx : { -1, 1 }) for (int sz : { -1, 1 })
        box(g, (std::string("support-frame-") + (sx < 0 ? "left" : "right") +
                (sz < 0 ? "-back" : "-front")).c_str(),
            .055f, 1.70f, .055f, steel, { sx * .43f, .85f, sz * .32f });
    cyl(g, "jacketed-vessel", .39f, 1.02f, brushed, { 0, .49f, 0 });
    cyl(g, "vessel-lid", .43f, .08f, charcoal, { 0, 1.51f, 0 });
    cyl(g, "agitator-motor", .19f, .39f, blue, { 0, 1.59f, 0 });
    cyl(g, "feed-port", .05f, .20f, steel, { -.27f, 1.59f, .08f }, 16);
    auto* outlet = cyl(g, "outlet-pipe", .05f, .34f, steel, { .49f, .69f, 0 }, 16);
    outlet->rotation.z = 90;
    cyl(g, "pressure-gauge", .07f, .04f, ceramic, { .32f, 1.62f, .16f }, 20);
}

void rack(g3d::Object3D& g) {
    for (int sx : { -1, 1 }) for (int sz : { -1, 1 })
        box(g, (std::string("rack-upright-") + (sx < 0 ? "left" : "right") +
                (sz < 0 ? "-back" : "-front")).c_str(),
            .045f, 1.55f, .045f, steel, { sx * .77f, .775f, sz * .27f });
    for (int i = 0; i < 2; ++i)
        box(g, i == 0 ? "rack-shelf-lower" : "rack-shelf-upper", 1.60f, .045f, .60f,
            brushed, { 0, i == 0 ? .37f : 1.05f, 0 });
    for (int i = 0; i < 4; ++i) {
        float x = -.53f + i * .35f;
        auto* flask = piece(g, ("flask-" + std::to_string(i + 1)).c_str(),
                            g3d::Geometry::cone(.12f, .28f, 24), glass,
                            { x, .54f, .02f });
        flask->rotation.z = 180;
        cyl(g, ("flask-neck-" + std::to_string(i + 1)).c_str(), .04f, .10f,
            glass, { x, .68f, .02f }, 16);
    }
    for (int i = 0; i < 3; ++i) {
        float x = -.45f + i * .45f;
        cyl(g, ("reagent-bottle-" + std::to_string(i + 1)).c_str(), .09f, .27f,
            glass, { x, 1.08f, 0 }, 20);
        cyl(g, ("bottle-cap-" + std::to_string(i + 1)).c_str(), .055f, .05f,
            yellow, { x, 1.35f, 0 }, 16);
    }
}

void pump(g3d::Object3D& g) {
    box(g, "pump-base", .84f, .08f, .44f, charcoal, { 0, .04f, 0 });
    box(g, "motor-housing", .43f, .34f, .32f, enamel, { -.13f, .25f, 0 });
    cyl(g, "pump-head", .17f, .33f, steel, { .24f, .08f, 0 });
    for (int i = 0; i < 2; ++i)
        box(g, i == 0 ? "carry-handle-post-left" : "carry-handle-post-right",
            .05f, .18f, .05f, charcoal, { i == 0 ? -.15f : .19f, .50f, 0 });
    box(g, "carry-handle", .39f, .05f, .06f, charcoal, { .02f, .61f, 0 });
    cyl(g, "intake-port", .045f, .11f, brushed, { .24f, .41f, 0 }, 16);
}

void build(g3d::Object3D& group, Kind kind) {
    switch (kind) {
    case Kind::Bench: bench(group); break;
    case Kind::FumeHood: hood(group); break;
    case Kind::ChemicalCabinet: cabinet(group); break;
    case Kind::AnalyticalBalance: balance(group); break;
    case Kind::HotplateStirrer: hotplate(group); break;
    case Kind::RotaryEvaporator: rotavap(group); break;
    case Kind::Hplc: hplc(group); break;
    case Kind::Centrifuge: centrifuge(group); break;
    case Kind::DryingOven: oven(group); break;
    case Kind::JacketedReactor: reactor(group); break;
    case Kind::GlasswareRack: rack(group); break;
    case Kind::VacuumPump: pump(group); break;
    default: throw std::invalid_argument("unknown lab3d::Kind");
    }
}

} // namespace

const char* name(Kind kind) {
    switch (kind) {
    case Kind::Bench: return "lab-bench";
    case Kind::FumeHood: return "fume-hood";
    case Kind::ChemicalCabinet: return "chemical-storage-cabinet";
    case Kind::AnalyticalBalance: return "analytical-balance";
    case Kind::HotplateStirrer: return "hotplate-stirrer";
    case Kind::RotaryEvaporator: return "rotary-evaporator";
    case Kind::Hplc: return "hplc-stack";
    case Kind::Centrifuge: return "bench-centrifuge";
    case Kind::DryingOven: return "drying-oven";
    case Kind::JacketedReactor: return "jacketed-reactor";
    case Kind::GlasswareRack: return "glassware-rack";
    case Kind::VacuumPump: return "vacuum-pump";
    default: throw std::invalid_argument("unknown lab3d::Kind");
    }
}

g3d::Object3D* add(g3d::Object3D& parent, Kind kind) {
    auto* group = parent.add();
    group->name = name(kind);
    build(*group, kind);
    return group;
}

g3d::Object3D* add(g3d::Scene& scene, Kind kind) {
    auto* group = scene.add();
    group->name = name(kind);
    build(*group, kind);
    return group;
}

city::Look cityLook() {
    city::Look look;
    look.color = [](const city::Object&, const city::Face&) { return WHITE; };
    look.material = [](const city::Object& object, g3d::Material& m) {
        const std::string role = object.attributes["paletteRole"].asString();
        m.shading = g3d::Shading::Standard;
        if (role == "glass") {
            m.color = { 132, 210, 229, 255 }; m.roughness = .10f; m.metalness = .02f; m.opacity = .43f;
        } else if (role == "metal" || role == "steel") {
            m.color = { 158, 179, 188, 255 }; m.roughness = .28f; m.metalness = .83f;
        } else if (role == "light-metal") {
            m.color = { 202, 215, 217, 255 }; m.roughness = .38f; m.metalness = .32f;
        } else if (role == "dark") {
            m.color = { 45, 54, 62, 255 }; m.roughness = .56f; m.metalness = .10f;
        } else if (role == "safety-yellow" || role == "accent") {
            m.color = { 226, 178, 69, 255 }; m.roughness = .37f; m.metalness = .18f;
        } else if (role == "screen") {
            m.color = { 56, 150, 178, 255 }; m.roughness = .28f; m.metalness = .02f;
        } else if (role == "liquid") {
            m.color = { 59, 159, 179, 255 }; m.roughness = .18f; m.metalness = .02f;
            m.opacity = .78f;
        } else if (role == "bench-top") {
            m.color = { 91, 103, 107, 255 }; m.roughness = .5f; m.metalness = .08f;
        } else {
            m.color = { 198, 213, 216, 255 }; m.roughness = .43f; m.metalness = .2f;
        }
    };
    return look;
}

} // namespace rmtn::lab3d
