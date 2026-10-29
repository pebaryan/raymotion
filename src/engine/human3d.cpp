#include "rmtn/human3d.hpp"
#include <stdexcept>
#include <string>
#include <utility>

namespace rmtn::human3d {
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

const g3d::Material skinLight = pbr({ 222, 168, 133, 255 }, .73f);
const g3d::Material skinMedium = pbr({ 171, 112, 76, 255 }, .76f);
const g3d::Material skinDeep = pbr({ 103, 65, 49, 255 }, .78f);
const g3d::Material skinOlive = pbr({ 193, 142, 105, 255 }, .73f);
const g3d::Material hairDark = pbr({ 36, 39, 41, 255 }, .88f);
const g3d::Material hairBrown = pbr({ 87, 58, 45, 255 }, .86f);
const g3d::Material hairLight = pbr({ 141, 114, 74, 255 }, .84f);
const g3d::Material denim = pbr({ 49, 76, 109, 255 }, .91f);
const g3d::Material navy = pbr({ 43, 59, 78, 255 }, .87f);
const g3d::Material charcoal = pbr({ 45, 51, 57, 255 }, .89f);
const g3d::Material teal = pbr({ 62, 124, 128, 255 }, .87f);
const g3d::Material rust = pbr({ 166, 88, 60, 255 }, .89f);
const g3d::Material mint = pbr({ 117, 166, 151, 255 }, .89f);
const g3d::Material lilac = pbr({ 132, 117, 156, 255 }, .89f);
const g3d::Material blue = pbr({ 70, 103, 154, 255 }, .84f);
const g3d::Material white = pbr({ 222, 231, 225, 255 }, .83f);
const g3d::Material safety = pbr({ 229, 166, 52, 255 }, .66f);
const g3d::Material hiVis = pbr({ 219, 214, 72, 255 }, .72f);
const g3d::Material steel = pbr({ 153, 168, 175, 255 }, .31f, .78f);
const g3d::Material rubber = pbr({ 31, 38, 42, 255 }, .91f);
const g3d::Material paper = pbr({ 239, 234, 209, 255 }, .95f);
const g3d::Material screen = pbr({ 59, 146, 171, 255 }, .27f, .05f);
const g3d::Material leather = pbr({ 106, 71, 54, 255 }, .70f);

g3d::Object3D* part(g3d::Object3D& root, std::string name,
                    g3d::GeometryPtr geometry, g3d::Material material, Vector3 at) {
    auto* n = root.add(std::move(geometry), material);
    n->name = std::move(name);
    n->position = at;
    return n;
}

g3d::Object3D* box(g3d::Object3D& root, std::string name, float w, float h, float d,
                   g3d::Material material, Vector3 at) {
    return part(root, std::move(name), g3d::Geometry::box(w, h, d), material, at);
}

g3d::Object3D* cyl(g3d::Object3D& root, std::string name, float r, float h,
                   g3d::Material material, Vector3 base, int slices = 20) {
    return part(root, std::move(name), g3d::Geometry::cylinder(r, h, slices), material,
                { base.x, base.y + h / 2, base.z });
}

g3d::Object3D* ellipsoid(g3d::Object3D& root, std::string name,
                         Vector3 radii, g3d::Material material, Vector3 at) {
    auto* n = part(root, std::move(name), g3d::Geometry::sphere(1, 14, 22), material, at);
    n->scale = radii;
    return n;
}

struct Profile {
    g3d::Material skin, hair, top, trousers;
    float height;
};

Profile profile(Role role) {
    switch (role) {
    case Role::Resident: return { skinMedium, hairDark, rust, denim, 1.00f };
    case Role::Commuter: return { skinLight, hairBrown, teal, charcoal, .98f };
    case Role::EvDriver: return { skinOlive, hairDark, mint, navy, 1.03f };
    case Role::ChargingTechnician: return { skinDeep, hairDark, navy, charcoal, 1.02f };
    case Role::GridEngineer: return { skinMedium, hairBrown, teal, navy, 1.04f };
    case Role::ConstructionWorker: return { skinLight, hairLight, rust, charcoal, 1.06f };
    case Role::OfficeWorker: return { skinDeep, hairDark, navy, charcoal, 1.00f };
    case Role::HealthcareWorker: return { skinOlive, hairBrown, teal, navy, 1.01f };
    case Role::Student: return { skinMedium, hairDark, lilac, denim, .82f };
    case Role::Teacher: return { skinLight, hairLight, mint, charcoal, .99f };
    case Role::RetailWorker: return { skinDeep, hairDark, rust, navy, .97f };
    case Role::TransitOperator: return { skinOlive, hairDark, navy, charcoal, 1.02f };
    default: throw std::invalid_argument("unknown human3d::Role");
    }
}

void baseHuman(g3d::Object3D& g, const Profile& p) {
    // Approximately 1.78 m before the role's uniform height scale.
    for (int sx : { -1, 1 }) {
        const std::string side = sx < 0 ? "left" : "right";
        ellipsoid(g, side + "-thigh", { .102f, .272f, .105f }, p.trousers,
                  { sx * .115f, .69f, 0 });
        ellipsoid(g, side + "-shin", { .079f, .265f, .082f }, p.trousers,
                  { sx * .115f, .28f, .012f });
        ellipsoid(g, side + "-shoe", { .105f, .07f, .18f }, rubber,
                  { sx * .115f, .07f, .075f });
    }
    ellipsoid(g, "pelvis", { .222f, .16f, .147f }, p.trousers, { 0, .94f, 0 });
    ellipsoid(g, "torso", { .245f, .34f, .150f }, p.top, { 0, 1.19f, 0 });
    ellipsoid(g, "neck", { .068f, .085f, .065f }, p.skin, { 0, 1.505f, 0 });
    ellipsoid(g, "head", { .137f, .174f, .125f }, p.skin, { 0, 1.642f, 0 });
    ellipsoid(g, "hair-cap", { .143f, .074f, .129f }, p.hair, { 0, 1.769f, -.005f });
    ellipsoid(g, "hair-fringe", { .131f, .034f, .063f }, p.hair, { 0, 1.737f, .083f });
    for (int sx : { -1, 1 }) {
        const std::string side = sx < 0 ? "left" : "right";
        ellipsoid(g, side + "-ear", { .025f, .050f, .027f }, p.skin,
                  { sx * .140f, 1.64f, 0 });
        ellipsoid(g, side + "-eye", { .010f, .008f, .006f }, hairDark,
                  { sx * .052f, 1.664f, .120f });
        auto* arm = g.add();
        arm->name = side + "-arm";
        arm->position = { sx * .248f, 1.417f, 0 };
        arm->rotation.z = sx * 8.f;
        ellipsoid(*arm, side + "-sleeve", { .087f, .235f, .082f }, p.top,
                  { sx * .025f, -.198f, 0 });
        ellipsoid(*arm, side + "-hand", { .052f, .077f, .048f }, p.skin,
                  { sx * .049f, -.438f, .012f });
    }
    ellipsoid(g, "nose", { .021f, .025f, .024f }, p.skin, { 0, 1.615f, .128f });
}

void backpack(g3d::Object3D& g, g3d::Material material) {
    ellipsoid(g, "backpack-body", { .194f, .268f, .105f }, material, { 0, 1.175f, -.187f });
    for (int sx : { -1, 1 })
        box(g, sx < 0 ? "backpack-left-strap" : "backpack-right-strap",
            .038f, .41f, .022f, rubber, { sx * .150f, 1.255f, .121f });
    box(g, "backpack-pocket", .245f, .102f, .022f, material, { 0, 1.033f, -.286f });
}

void helmet(g3d::Object3D& g, g3d::Material material) {
    ellipsoid(g, "safety-helmet-crown", { .158f, .085f, .143f }, material,
              { 0, 1.798f, -.005f });
    box(g, "safety-helmet-brim", .346f, .028f, .285f, material,
        { 0, 1.755f, .020f });
    box(g, "safety-helmet-ridge", .030f, .014f, .22f, steel,
        { 0, 1.877f, -.006f });
}

void safetyVest(g3d::Object3D& g, g3d::Material material) {
    box(g, "safety-vest", .385f, .39f, .025f, material, { 0, 1.19f, .153f });
    for (int i = 0; i < 2; ++i)
        box(g, "reflective-band-" + std::to_string(i + 1), .393f, .034f, .027f,
            white, { 0, 1.075f + i * .17f, .170f });
}

void satchel(g3d::Object3D& g, g3d::Material material) {
    box(g, "satchel-body", .27f, .22f, .085f, material, { .33f, .80f, .07f });
    box(g, "satchel-flap", .275f, .08f, .09f, material, { .33f, .90f, .07f });
    auto* strap = box(g, "satchel-strap", .025f, .59f, .022f,
                      material, { .10f, 1.25f, .16f });
    strap->rotation.z = -19.f;
}

void book(g3d::Object3D& g, std::string prefix, g3d::Material cover, Vector3 at) {
    box(g, prefix + "-cover", .21f, .028f, .16f, cover, at);
    box(g, prefix + "-pages", .195f, .023f, .145f, paper,
        { at.x, at.y + .002f, at.z });
    box(g, prefix + "-spine", .015f, .031f, .16f, cover,
        { at.x - .105f, at.y, at.z });
}

void roleDetails(g3d::Object3D& g, Role role) {
    switch (role) {
    case Role::Resident:
        box(g, "scarf", .34f, .070f, .19f, mint, { 0, 1.45f, .02f });
        satchel(g, leather);
        break;
    case Role::Commuter:
        backpack(g, navy);
        box(g, "phone", .065f, .12f, .012f, charcoal, { -.34f, .99f, .065f });
        box(g, "phone-screen", .051f, .094f, .004f, screen,
            { -.34f, .99f, .074f });
        break;
    case Role::EvDriver:
        box(g, "charging-plug", .067f, .045f, .12f, charcoal,
            { .34f, .995f, .095f });
        cyl(g, "charging-handle", .030f, .14f, rubber,
            { .34f, .91f, .095f }, 16);
        box(g, "plug-accent", .067f, .021f, .015f, teal,
            { .34f, 1.025f, .158f });
        break;
    case Role::ChargingTechnician:
        helmet(g, safety);
        safetyVest(g, hiVis);
        box(g, "tool-case", .28f, .23f, .15f, charcoal,
            { .39f, .77f, .05f });
        box(g, "tool-case-handle", .12f, .035f, .025f, steel,
            { .39f, .905f, .05f });
        break;
    case Role::GridEngineer:
        helmet(g, white);
        safetyVest(g, teal);
        box(g, "tablet", .22f, .30f, .018f, charcoal,
            { .16f, 1.09f, .20f });
        box(g, "tablet-screen", .188f, .255f, .004f, screen,
            { .16f, 1.09f, .212f });
        break;
    case Role::ConstructionWorker:
        helmet(g, safety);
        safetyVest(g, safety);
        box(g, "tool-belt", .47f, .09f, .19f, leather, { 0, .935f, 0 });
        box(g, "belt-pouch", .13f, .16f, .075f, leather,
            { .23f, .84f, .02f });
        break;
    case Role::OfficeWorker:
        box(g, "shirt-front", .115f, .38f, .026f, white, { 0, 1.20f, .155f });
        box(g, "tie", .037f, .29f, .029f, rust, { 0, 1.23f, .173f });
        satchel(g, charcoal);
        break;
    case Role::HealthcareWorker:
        box(g, "coat-left", .175f, .60f, .045f, white,
            { -.105f, 1.075f, .157f });
        box(g, "coat-right", .175f, .60f, .045f, white,
            { .105f, 1.075f, .157f });
        box(g, "name-badge", .085f, .045f, .009f, screen,
            { .095f, 1.345f, .185f });
        cyl(g, "stethoscope-neck", .016f, .24f, rubber,
            { -.055f, 1.22f, .197f }, 12);
        cyl(g, "stethoscope-head", .032f, .015f, steel,
            { -.055f, 1.19f, .203f }, 20);
        break;
    case Role::Student:
        backpack(g, lilac);
        book(g, "school-book", blue, { .31f, .99f, .09f });
        break;
    case Role::Teacher:
        box(g, "cardigan-lapel-left", .074f, .34f, .025f, white,
            { -.11f, 1.27f, .152f });
        box(g, "cardigan-lapel-right", .074f, .34f, .025f, white,
            { .11f, 1.27f, .152f });
        book(g, "lesson-book", rust, { .34f, .99f, .09f });
        break;
    case Role::RetailWorker:
        box(g, "apron", .36f, .63f, .025f, navy, { 0, 1.02f, .159f });
        box(g, "apron-tie", .40f, .034f, .19f, navy, { 0, 1.19f, 0 });
        box(g, "apron-badge", .096f, .048f, .008f, white,
            { .098f, 1.302f, .177f });
        box(g, "parcel", .23f, .18f, .20f, paper,
            { .34f, .88f, .09f });
        break;
    case Role::TransitOperator:
        ellipsoid(g, "operator-cap", { .159f, .058f, .135f }, navy,
                  { 0, 1.790f, 0 });
        box(g, "operator-cap-visor", .22f, .018f, .12f, charcoal,
            { 0, 1.748f, .11f });
        box(g, "uniform-collar", .21f, .07f, .022f, white,
            { 0, 1.435f, .132f });
        box(g, "ticket-reader", .10f, .17f, .035f, charcoal,
            { .34f, .98f, .06f });
        box(g, "reader-screen", .075f, .079f, .006f, screen,
            { .34f, 1.009f, .083f });
        break;
    default: throw std::invalid_argument("unknown human3d::Role");
    }
}

void build(g3d::Object3D& group, Role role) {
    Profile p = profile(role);
    baseHuman(group, p);
    roleDetails(group, role);
    group.scale = { p.height, p.height, p.height };
}

} // namespace

const char* name(Role role) {
    switch (role) {
    case Role::Resident: return "human-resident";
    case Role::Commuter: return "human-commuter";
    case Role::EvDriver: return "human-ev-driver";
    case Role::ChargingTechnician: return "human-charging-technician";
    case Role::GridEngineer: return "human-grid-engineer";
    case Role::ConstructionWorker: return "human-construction-worker";
    case Role::OfficeWorker: return "human-office-worker";
    case Role::HealthcareWorker: return "human-healthcare-worker";
    case Role::Student: return "human-student";
    case Role::Teacher: return "human-teacher";
    case Role::RetailWorker: return "human-retail-worker";
    case Role::TransitOperator: return "human-transit-operator";
    default: throw std::invalid_argument("unknown human3d::Role");
    }
}

g3d::Object3D* add(g3d::Object3D& parent, Role role) {
    auto* root = parent.add();
    root->name = name(role);
    build(*root, role);
    return root;
}

g3d::Object3D* add(g3d::Scene& scene, Role role) {
    auto* root = scene.add();
    root->name = name(role);
    build(*root, role);
    return root;
}

} // namespace rmtn::human3d
