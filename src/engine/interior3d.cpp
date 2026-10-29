#include "rmtn/interior3d.hpp"
#include <raymath.h>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace rmtn::interior3d {
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

const g3d::Material enamel = pbr({ 221, 231, 231, 255 }, .36f, .23f);
const g3d::Material stainless = pbr({ 167, 184, 190, 255 }, .26f, .85f);
const g3d::Material steel = pbr({ 91, 111, 123, 255 }, .39f, .75f);
const g3d::Material polymer = pbr({ 49, 62, 71, 255 }, .63f, .04f);
const g3d::Material rubber = pbr({ 34, 39, 42, 255 }, .87f);
const g3d::Material clinical = pbr({ 106, 171, 172, 255 }, .72f);
const g3d::Material linen = pbr({ 231, 239, 231, 255 }, .90f);
const g3d::Material fabric = pbr({ 80, 107, 129, 255 }, .91f);
const g3d::Material wood = pbr({ 172, 126, 82, 255 }, .67f);
const g3d::Material woodLight = pbr({ 197, 157, 106, 255 }, .70f);
const g3d::Material laminate = pbr({ 194, 183, 161, 255 }, .53f, .04f);
const g3d::Material whiteboard = pbr({ 237, 244, 239, 255 }, .23f, .02f);
const g3d::Material glass = pbr({ 155, 214, 228, 255 }, .12f, .03f, .40f);
const g3d::Material screen = pbr({ 56, 142, 164, 255 }, .25f, .02f);
const g3d::Material blue = pbr({ 70, 117, 142, 255 }, .41f, .28f);
const g3d::Material red = pbr({ 200, 83, 74, 255 }, .46f, .06f);
const g3d::Material yellow = pbr({ 228, 184, 82, 255 }, .49f, .08f);
const g3d::Material green = pbr({ 81, 145, 111, 255 }, .55f, .06f);
const g3d::Material charcoal = pbr({ 39, 48, 56, 255 }, .42f, .22f);
const g3d::Material copper = pbr({ 174, 104, 67, 255 }, .31f, .78f);
const g3d::Material warmFabric = pbr({ 182, 128, 99, 255 }, .92f);
const g3d::Material paleTile = pbr({ 222, 220, 204, 255 }, .49f, .02f);

g3d::Object3D* part(g3d::Object3D& root, std::string name,
                    g3d::GeometryPtr geometry, g3d::Material material, Vector3 at) {
    auto* node = root.add(std::move(geometry), material);
    node->name = std::move(name);
    node->position = at;
    return node;
}

g3d::Object3D* box(g3d::Object3D& root, std::string name, float w, float h, float d,
                   g3d::Material material, Vector3 at) {
    return part(root, std::move(name), g3d::Geometry::box(w, h, d), material, at);
}

g3d::Object3D* cyl(g3d::Object3D& root, std::string name, float r, float h,
                   g3d::Material material, Vector3 base, int slices = 24) {
    return part(root, std::move(name), g3d::Geometry::cylinder(r, h, slices), material,
                { base.x, base.y + h / 2, base.z });
}

g3d::Object3D* wheel(g3d::Object3D& root, std::string name, float r, Vector3 centre) {
    auto* n = part(root, std::move(name), g3d::Geometry::torus(r, .035f, 32, 12), rubber, centre);
    n->rotation.y = 90;
    return n;
}

void hospitalBed(g3d::Object3D& g) {
    box(g, "bed-frame", 1.03f, .14f, 2.13f, stainless, { 0, .55f, 0 });
    box(g, "mattress", .94f, .19f, 1.91f, clinical, { 0, .72f, 0 });
    box(g, "sheet", .91f, .035f, 1.22f, linen, { 0, .835f, .30f });
    box(g, "pillow", .71f, .12f, .39f, linen, { 0, .87f, -.72f });
    box(g, "headboard", 1.08f, .69f, .08f, enamel, { 0, .83f, -1.07f });
    box(g, "footboard", 1.08f, .42f, .08f, enamel, { 0, .70f, 1.07f });
    for (int sx : { -1, 1 }) {
        box(g, sx < 0 ? "left-side-rail" : "right-side-rail", .045f, .23f, 1.25f,
            stainless, { sx * .53f, .92f, 0 });
        for (int sz : { -1, 1 }) {
            std::string tag = std::string(sx < 0 ? "left" : "right") + (sz < 0 ? "-head" : "-foot");
            box(g, "leg-" + tag, .07f, .48f, .07f, steel,
                { sx * .44f, .24f, sz * .87f });
            wheel(g, "caster-" + tag, .085f, { sx * .44f, .09f, sz * .87f });
        }
    }
}

void bedsideCabinet(g3d::Object3D& g) {
    box(g, "cabinet-body", .56f, .70f, .47f, enamel, { 0, .40f, 0 });
    box(g, "countertop", .61f, .045f, .50f, laminate, { 0, .77f, 0 });
    box(g, "drawer-front", .49f, .16f, .018f, clinical, { 0, .60f, .246f });
    box(g, "lower-door", .49f, .42f, .018f, enamel, { 0, .28f, .246f });
    box(g, "drawer-pull", .16f, .022f, .035f, stainless, { 0, .60f, .27f });
    box(g, "door-pull", .025f, .14f, .035f, stainless, { .19f, .31f, .27f });
    for (int sx : { -1, 1 }) for (int sz : { -1, 1 })
        wheel(g, std::string("caster-") + (sx < 0 ? "left" : "right") +
              (sz < 0 ? "-back" : "-front"), .045f,
              { sx * .22f, .045f, sz * .18f });
}

void ivStand(g3d::Object3D& g) {
    cyl(g, "centre-pole", .025f, 1.85f, stainless, { 0, .10f, 0 }, 16);
    cyl(g, "height-collar", .055f, .10f, polymer, { 0, 1.18f, 0 }, 16);
    for (int i = 0; i < 3; ++i) {
        const float a = i * 2.0943951f;
        auto* arm = box(g, "base-arm-" + std::to_string(i + 1), .48f, .035f, .035f,
                        stainless, { .24f * std::cos(a), .10f, .24f * std::sin(a) });
        arm->rotation.y = -i * 120.f;
        wheel(g, "base-caster-" + std::to_string(i + 1), .045f,
              { .47f * std::cos(a), .045f, .47f * std::sin(a) });
    }
    box(g, "top-crossbar", .72f, .035f, .035f, stainless, { 0, 1.96f, 0 });
    for (int sx : { -1, 1 }) {
        box(g, sx < 0 ? "hook-left" : "hook-right", .035f, .15f, .035f,
            stainless, { sx * .34f, 1.89f, 0 });
    }
    box(g, "fluid-bag", .17f, .30f, .055f, glass, { -.34f, 1.65f, 0 });
}

void examTable(g3d::Object3D& g) {
    box(g, "exam-base", .72f, .51f, 1.45f, enamel, { 0, .30f, 0 });
    box(g, "padded-top", .79f, .14f, 1.54f, clinical, { 0, .64f, 0 });
    auto* head = box(g, "adjustable-headrest", .79f, .10f, .51f,
                     clinical, { 0, .74f, -.54f });
    head->rotation.x = -10;
    box(g, "base-door", .64f, .40f, .018f, enamel, { 0, .33f, .735f });
    box(g, "base-handle", .16f, .025f, .03f, stainless, { 0, .38f, .76f });
    box(g, "step-stool", .41f, .22f, .35f, steel, { .65f, .11f, .43f });
}

void wheelchair(g3d::Object3D& g) {
    box(g, "seat", .59f, .10f, .55f, fabric, { 0, .54f, .02f });
    box(g, "backrest", .59f, .59f, .09f, fabric, { 0, .91f, -.28f });
    for (int sx : { -1, 1 }) {
        box(g, sx < 0 ? "armrest-left" : "armrest-right", .07f, .07f, .53f,
            polymer, { sx * .34f, .77f, 0 });
        box(g, sx < 0 ? "frame-left" : "frame-right", .05f, .44f, .05f,
            stainless, { sx * .34f, .37f, -.14f });
        wheel(g, sx < 0 ? "rear-wheel-left" : "rear-wheel-right", .33f,
              { sx * .43f, .34f, -.19f });
        wheel(g, sx < 0 ? "front-caster-left" : "front-caster-right", .09f,
              { sx * .30f, .10f, .34f });
        box(g, sx < 0 ? "push-handle-left" : "push-handle-right", .06f, .23f, .06f,
            rubber, { sx * .28f, 1.27f, -.32f });
        box(g, sx < 0 ? "footrest-left" : "footrest-right", .24f, .035f, .19f,
            stainless, { sx * .17f, .14f, .46f });
    }
}

void medicalCart(g3d::Object3D& g) {
    for (int sx : { -1, 1 }) for (int sz : { -1, 1 }) {
        const std::string tag = std::string(sx < 0 ? "left" : "right") +
                                (sz < 0 ? "-back" : "-front");
        box(g, "cart-post-" + tag, .045f, .92f, .045f, stainless,
            { sx * .37f, .53f, sz * .25f });
        wheel(g, "cart-caster-" + tag, .055f,
              { sx * .37f, .055f, sz * .25f });
    }
    for (float y : { .24f, .72f, 1.0f })
        box(g, "shelf-" + std::to_string(int(y * 100)), .86f, .055f, .61f,
            enamel, { 0, y, 0 });
    box(g, "drawer-block", .78f, .22f, .56f, clinical, { 0, .86f, 0 });
    box(g, "drawer-handle", .22f, .025f, .035f, stainless, { 0, .85f, .30f });
    box(g, "push-handle", .84f, .035f, .035f, stainless, { 0, 1.12f, -.29f });
}

void studentDesk(g3d::Object3D& g) {
    box(g, "desktop", 1.15f, .055f, .64f, woodLight, { 0, .75f, 0 });
    for (int sx : { -1, 1 }) for (int sz : { -1, 1 })
        box(g, std::string("leg-") + (sx < 0 ? "left" : "right") +
            (sz < 0 ? "-back" : "-front"), .045f, .73f, .045f, steel,
            { sx * .49f, .365f, sz * .25f });
    box(g, "under-desk-tray", .86f, .065f, .33f, steel, { 0, .64f, -.08f });
    box(g, "pencil-trough", .62f, .025f, .075f, wood, { 0, .79f, -.24f });
}

void classroomChair(g3d::Object3D& g) {
    box(g, "chair-seat", .48f, .065f, .45f, woodLight, { 0, .46f, 0 });
    box(g, "chair-back", .48f, .47f, .065f, woodLight, { 0, .78f, -.23f });
    for (int sx : { -1, 1 }) for (int sz : { -1, 1 })
        box(g, std::string("chair-leg-") + (sx < 0 ? "left" : "right") +
            (sz < 0 ? "-back" : "-front"), .035f, .45f, .035f, steel,
            { sx * .20f, .225f, sz * .18f });
}

void teacherDesk(g3d::Object3D& g) {
    box(g, "teacher-desktop", 1.60f, .07f, .75f, laminate, { 0, .77f, 0 });
    box(g, "drawer-pedestal", .44f, .64f, .61f, woodLight, { -.52f, .43f, 0 });
    for (int i = 0; i < 3; ++i) {
        box(g, "drawer-front-" + std::to_string(i + 1), .38f, .15f, .018f,
            wood, { -.52f, .26f + i * .18f, .316f });
        box(g, "drawer-pull-" + std::to_string(i + 1), .13f, .02f, .025f,
            stainless, { -.52f, .26f + i * .18f, .335f });
    }
    for (int sz : { -1, 1 })
        box(g, sz < 0 ? "right-leg-back" : "right-leg-front", .055f, .73f, .055f,
            steel, { .68f, .365f, sz * .30f });
    box(g, "front-modesty-panel", 1.40f, .34f, .025f, woodLight, { 0, .50f, -.37f });
}

void whiteboardStand(g3d::Object3D& g) {
    box(g, "board-surface", 2.35f, 1.16f, .035f, whiteboard, { 0, 1.34f, 0 });
    box(g, "top-frame", 2.48f, .045f, .065f, stainless, { 0, 1.94f, 0 });
    box(g, "bottom-frame", 2.48f, .045f, .065f, stainless, { 0, .74f, 0 });
    for (int sx : { -1, 1 }) {
        box(g, sx < 0 ? "left-frame" : "right-frame", .045f, 1.20f, .065f,
            stainless, { sx * 1.21f, 1.34f, 0 });
        box(g, sx < 0 ? "left-stand" : "right-stand", .055f, .74f, .055f,
            steel, { sx * 1.02f, .37f, -.03f });
    }
    box(g, "marker-tray", 1.25f, .035f, .15f, steel, { 0, .71f, .10f });
    for (int i = 0; i < 3; ++i)
        box(g, "marker-" + std::to_string(i + 1), .18f, .026f, .025f,
            i == 0 ? red : i == 1 ? green : polymer,
            { -.35f + i * .25f, .75f, .15f });
}

void bookshelf(g3d::Object3D& g) {
    for (int sx : { -1, 1 })
        box(g, sx < 0 ? "left-side" : "right-side", .06f, 1.88f, .42f,
            wood, { sx * .66f, .94f, 0 });
    box(g, "back-panel", 1.31f, 1.88f, .035f, wood, { 0, .94f, -.20f });
    for (int i = 0; i < 4; ++i) {
        float y = .16f + i * .54f;
        box(g, "shelf-" + std::to_string(i + 1), 1.32f, .055f, .44f,
            woodLight, { 0, y, 0 });
    }
    for (int shelf = 0; shelf < 3; ++shelf) for (int i = 0; i < 5; ++i) {
        float x = -.50f + i * .23f;
        const g3d::Material& cover = (i + shelf) % 3 == 0 ? red :
                                     (i + shelf) % 3 == 1 ? fabric : green;
        box(g, "book-" + std::to_string(shelf + 1) + "-" + std::to_string(i + 1),
            .17f, .38f, .31f, cover, { x, .38f + shelf * .54f, .01f });
    }
}

void lockers(g3d::Object3D& g) {
    box(g, "locker-body", 1.65f, 1.94f, .51f, enamel, { 0, .97f, 0 });
    for (int i = 0; i < 3; ++i) {
        float x = -.55f + i * .55f;
        box(g, "locker-door-" + std::to_string(i + 1), .52f, 1.80f, .018f,
            i == 1 ? blue : clinical, { x, .98f, .265f });
        box(g, "locker-pull-" + std::to_string(i + 1), .025f, .17f, .025f,
            stainless, { x + .17f, 1.02f, .286f });
        for (int slot = 0; slot < 3; ++slot)
            box(g, "vent-" + std::to_string(i + 1) + "-" + std::to_string(slot + 1),
                .22f, .015f, .008f, steel, { x, 1.62f + slot * .06f, .28f });
    }
}

void officeDesk(g3d::Object3D& g) {
    box(g, "office-desktop", 1.70f, .065f, .83f, laminate, { 0, .75f, 0 });
    for (int sx : { -1, 1 }) for (int sz : { -1, 1 })
        box(g, std::string("desk-leg-") + (sx < 0 ? "left" : "right") +
            (sz < 0 ? "-back" : "-front"), .055f, .72f, .055f, steel,
            { sx * .74f, .36f, sz * .34f });
    box(g, "modesty-panel", 1.52f, .28f, .025f, wood, { 0, .49f, -.39f });
    cyl(g, "cable-grommet", .055f, .005f, polymer, { .62f, .79f, -.29f }, 20);
}

void ergonomicChair(g3d::Object3D& g) {
    box(g, "seat-cushion", .57f, .12f, .53f, fabric, { 0, .55f, .02f });
    box(g, "backrest", .55f, .63f, .09f, fabric, { 0, .94f, -.29f });
    for (int sx : { -1, 1 })
        box(g, sx < 0 ? "armrest-left" : "armrest-right", .07f, .055f, .43f,
            polymer, { sx * .35f, .78f, .015f });
    cyl(g, "gas-lift", .055f, .38f, steel, { 0, .17f, 0 }, 16);
    for (int i = 0; i < 5; ++i) {
        const float a = i * 1.25663706f;
        auto* spoke = box(g, "base-spoke-" + std::to_string(i + 1), .42f, .045f, .045f,
                          polymer, { .21f * std::cos(a), .16f, .21f * std::sin(a) });
        spoke->rotation.y = -a * RAD2DEG;
        wheel(g, "caster-" + std::to_string(i + 1), .045f,
              { .43f * std::cos(a), .045f, .43f * std::sin(a) });
    }
}

void meetingTable(g3d::Object3D& g) {
    box(g, "meeting-top", 2.42f, .075f, 1.18f, wood, { 0, .76f, 0 });
    for (int sx : { -1, 1 })
        box(g, sx < 0 ? "pedestal-left" : "pedestal-right", .12f, .70f, .70f,
            stainless, { sx * .85f, .35f, 0 });
    box(g, "centre-power-port", .28f, .008f, .14f, polymer, { 0, .80f, 0 });
}

void filingCabinet(g3d::Object3D& g) {
    box(g, "filing-body", .55f, 1.24f, .66f, enamel, { 0, .62f, 0 });
    for (int i = 0; i < 4; ++i) {
        const float y = .18f + i * .29f;
        box(g, "file-drawer-" + std::to_string(i + 1), .49f, .26f, .018f,
            steel, { 0, y + .08f, .339f });
        box(g, "drawer-pull-" + std::to_string(i + 1), .20f, .025f, .035f,
            stainless, { 0, y + .09f, .36f });
        box(g, "label-slot-" + std::to_string(i + 1), .14f, .045f, .012f,
            whiteboard, { 0, y + .19f, .356f });
    }
}

void cubicle(g3d::Object3D& g) {
    box(g, "long-panel", 2.15f, 1.42f, .075f, fabric, { 0, .71f, -.55f });
    box(g, "side-panel", .075f, 1.42f, 1.18f, fabric, { -1.04f, .71f, 0 });
    box(g, "long-top-trim", 2.19f, .045f, .095f, steel, { 0, 1.44f, -.55f });
    box(g, "side-top-trim", .095f, .045f, 1.18f, steel, { -1.04f, 1.44f, 0 });
    for (float x : { -.97f, 1.02f })
        box(g, x < 0 ? "left-foot" : "right-foot", .25f, .055f, .55f,
            polymer, { x, .03f, -.55f });
}

void printer(g3d::Object3D& g) {
    box(g, "printer-body", .62f, .38f, .50f, enamel, { 0, .19f, 0 });
    box(g, "scanner-lid", .67f, .055f, .52f, polymer, { 0, .41f, -.015f });
    box(g, "paper-tray", .46f, .055f, .21f, steel, { 0, .14f, .34f });
    box(g, "output-slot", .39f, .035f, .014f, polymer, { 0, .29f, .26f });
    box(g, "control-panel", .16f, .07f, .012f, screen, { .18f, .36f, .26f });
}

void serverRack(g3d::Object3D& g) {
    box(g, "rack-shell", .72f, 2.06f, 1.05f, charcoal, { 0, 1.03f, 0 });
    box(g, "front-recess", .62f, 1.82f, .016f, polymer, { 0, 1.03f, .535f });
    for (int i = 0; i < 8; ++i) {
        float y = .23f + i * .22f;
        box(g, "server-blade-" + std::to_string(i + 1), .56f, .16f, .025f,
            i % 3 ? steel : enamel, { 0, y, .554f });
        box(g, "status-led-" + std::to_string(i + 1), .022f, .013f, .007f,
            i % 4 ? green : yellow, { -.22f, y, .573f });
    }
    box(g, "top-vent", .48f, .009f, .65f, steel, { 0, 2.065f, 0 });
    for (int sx : { -1, 1 })
        box(g, sx < 0 ? "left-upright" : "right-upright", .045f, 1.96f, .065f,
            stainless, { sx * .34f, 1.03f, .54f });
}

void networkCabinet(g3d::Object3D& g) {
    box(g, "cabinet-shell", .66f, 1.76f, .80f, steel, { 0, .88f, 0 });
    box(g, "glass-door", .55f, 1.61f, .012f, glass, { 0, .87f, .408f });
    box(g, "door-handle", .025f, .22f, .035f, stainless, { .25f, .87f, .44f });
    for (int i = 0; i < 6; ++i) {
        float y = .40f + i * .19f;
        box(g, "patch-panel-" + std::to_string(i + 1), .48f, .095f, .035f,
            charcoal, { 0, y, .38f });
        for (int j = 0; j < 4; ++j)
            box(g, "port-" + std::to_string(i + 1) + "-" + std::to_string(j + 1),
                .032f, .024f, .005f, j % 2 ? green : copper,
                { -.17f + j * .11f, y, .401f });
    }
}

void upsCabinet(g3d::Object3D& g) {
    box(g, "ups-body", .68f, 1.48f, .72f, enamel, { 0, .74f, 0 });
    box(g, "front-panel", .59f, 1.34f, .014f, charcoal, { 0, .74f, .369f });
    box(g, "control-screen", .29f, .15f, .008f, screen, { 0, 1.24f, .38f });
    for (int i = 0; i < 5; ++i)
        box(g, "battery-drawer-" + std::to_string(i + 1), .48f, .14f, .012f,
            steel, { 0, .24f + i * .17f, .38f });
    for (int sx : { -1, 1 })
        box(g, sx < 0 ? "left-plinth" : "right-plinth", .12f, .11f, .61f,
            polymer, { sx * .24f, .055f, 0 });
}

void workstation(g3d::Object3D& g) {
    box(g, "tower-case", .24f, .48f, .48f, charcoal, { -.31f, .24f, -.10f });
    box(g, "tower-front", .205f, .42f, .01f, steel, { -.31f, .25f, .145f });
    box(g, "power-button", .032f, .032f, .007f, screen, { -.31f, .41f, .155f });
    box(g, "monitor-panel", .70f, .43f, .035f, polymer, { .17f, .70f, -.18f });
    box(g, "monitor-screen", .65f, .38f, .006f, screen, { .17f, .70f, -.157f });
    box(g, "monitor-post", .045f, .25f, .045f, steel, { .17f, .365f, -.18f });
    box(g, "monitor-foot", .34f, .025f, .23f, steel, { .17f, .245f, -.18f });
    box(g, "keyboard", .52f, .025f, .16f, polymer, { .14f, .035f, .32f });
    box(g, "mouse", .065f, .035f, .10f, polymer, { .49f, .035f, .30f });
}

void coolingUnit(g3d::Object3D& g) {
    box(g, "cooling-cabinet", .81f, 1.97f, .84f, enamel, { 0, .985f, 0 });
    box(g, "intake-grille", .66f, 1.20f, .014f, steel, { 0, .78f, .429f });
    for (int i = 0; i < 9; ++i)
        box(g, "intake-slat-" + std::to_string(i + 1), .60f, .022f, .025f,
            charcoal, { 0, .25f + i * .13f, .444f });
    box(g, "controller", .31f, .15f, .018f, screen, { 0, 1.67f, .434f });
    box(g, "top-outlet", .62f, .012f, .60f, charcoal, { 0, 1.983f, 0 });
}

void securityCamera(g3d::Object3D& g) {
    cyl(g, "mount-pole", .045f, 1.86f, steel, { 0, 0, 0 }, 16);
    box(g, "mount-arm", .42f, .055f, .055f, stainless, { .21f, 1.77f, 0 });
    auto* housing = box(g, "camera-housing", .43f, .18f, .18f,
                        enamel, { .54f, 1.75f, 0 });
    housing->rotation.z = -10;
    cyl(g, "lens-barrel", .067f, .08f, charcoal, { .76f, 1.68f, 0 }, 24)->rotation.z = 90;
    box(g, "sun-shield", .48f, .025f, .24f, steel, { .56f, 1.87f, 0 });
    box(g, "base-plate", .26f, .035f, .26f, charcoal, { 0, .018f, 0 });
}

void sofa(g3d::Object3D& g) {
    box(g, "sofa-base", 2.18f, .35f, .88f, wood, { 0, .28f, 0 });
    box(g, "backrest", 2.18f, .68f, .24f, warmFabric, { 0, .77f, -.39f });
    for (int i = 0; i < 3; ++i)
        box(g, "seat-cushion-" + std::to_string(i + 1), .68f, .18f, .70f,
            warmFabric, { -.72f + i * .72f, .55f, .10f });
    for (int sx : { -1, 1 }) {
        box(g, sx < 0 ? "left-arm" : "right-arm", .20f, .48f, .83f,
            warmFabric, { sx * 1.0f, .60f, 0 });
        box(g, sx < 0 ? "left-foot" : "right-foot", .12f, .12f, .12f,
            wood, { sx * .89f, .06f, .27f });
    }
}

void diningTable(g3d::Object3D& g) {
    box(g, "tabletop", 1.58f, .065f, .92f, woodLight, { 0, .75f, 0 });
    box(g, "apron-front", 1.37f, .11f, .04f, wood, { 0, .665f, .39f });
    box(g, "apron-back", 1.37f, .11f, .04f, wood, { 0, .665f, -.39f });
    for (int sx : { -1, 1 }) for (int sz : { -1, 1 })
        box(g, std::string("leg-") + (sx < 0 ? "left" : "right") +
            (sz < 0 ? "-back" : "-front"), .075f, .72f, .075f,
            wood, { sx * .67f, .36f, sz * .35f });
}

void refrigerator(g3d::Object3D& g) {
    box(g, "fridge-body", .77f, 1.78f, .70f, enamel, { 0, .89f, 0 });
    box(g, "upper-door", .73f, .58f, .025f, stainless, { 0, 1.46f, .365f });
    box(g, "lower-door", .73f, 1.12f, .025f, stainless, { 0, .56f, .365f });
    box(g, "upper-handle", .028f, .32f, .045f, steel, { -.30f, 1.35f, .405f });
    box(g, "lower-handle", .028f, .37f, .045f, steel, { -.30f, .95f, .405f });
    box(g, "base-grille", .66f, .055f, .01f, charcoal, { 0, .065f, .36f });
}

void kitchenRange(g3d::Object3D& g) {
    box(g, "range-body", .76f, .87f, .68f, enamel, { 0, .435f, 0 });
    box(g, "cooktop", .75f, .035f, .67f, charcoal, { 0, .89f, 0 });
    box(g, "oven-door", .67f, .48f, .018f, steel, { 0, .37f, .35f });
    box(g, "oven-window", .50f, .30f, .008f, glass, { 0, .39f, .363f });
    box(g, "oven-handle", .53f, .025f, .045f, stainless, { 0, .67f, .39f });
    for (int sx : { -1, 1 }) for (int sz : { -1, 1 }) {
        std::string tag = std::string(sx < 0 ? "left" : "right") + (sz < 0 ? "-back" : "-front");
        auto* burner = part(g, "burner-" + tag, g3d::Geometry::torus(.105f, .012f, 28, 8),
                            steel, { sx * .20f, .913f, sz * .17f });
        burner->rotation.x = 90;
    }
    for (int i = 0; i < 4; ++i)
        cyl(g, "control-knob-" + std::to_string(i + 1), .025f, .026f,
            steel, { -.24f + i * .16f, .80f, .355f }, 16)->rotation.x = 90;
}

void washingMachine(g3d::Object3D& g) {
    box(g, "washer-body", .67f, .85f, .67f, enamel, { 0, .425f, 0 });
    box(g, "control-strip", .63f, .13f, .012f, steel, { 0, .77f, .342f });
    box(g, "display", .15f, .055f, .006f, screen, { .13f, .78f, .352f });
    cyl(g, "selector", .034f, .025f, polymer, { -.20f, .76f, .345f }, 20)->rotation.x = 90;
    auto* rim = part(g, "door-rim", g3d::Geometry::torus(.225f, .026f, 36, 10),
                     stainless, { 0, .39f, .355f });
    (void)rim;
    cyl(g, "drum-window", .195f, .012f, glass, { 0, .38f, .35f }, 36)->rotation.x = 90;
    box(g, "lower-kickplate", .60f, .07f, .012f, steel, { 0, .055f, .343f });
}

void floorLamp(g3d::Object3D& g) {
    cyl(g, "round-base", .23f, .035f, steel, { 0, 0, 0 }, 32);
    cyl(g, "lamp-stem", .018f, 1.45f, copper, { 0, .035f, 0 }, 16);
    cyl(g, "shade", .27f, .32f, paleTile, { 0, 1.44f, 0 }, 32);
    cyl(g, "shade-rim", .275f, .018f, copper, { 0, 1.44f, 0 }, 32);
    cyl(g, "finial", .035f, .065f, copper, { 0, 1.77f, 0 }, 16);
}

void cafeTable(g3d::Object3D& g) {
    cyl(g, "round-top", .48f, .05f, woodLight, { 0, .72f, 0 }, 40);
    cyl(g, "pedestal", .055f, .69f, steel, { 0, .05f, 0 }, 24);
    cyl(g, "disc-base", .32f, .05f, steel, { 0, 0, 0 }, 32);
    cyl(g, "top-collar", .11f, .045f, stainless, { 0, .69f, 0 }, 24);
}

void restaurantChair(g3d::Object3D& g) {
    box(g, "seat", .48f, .07f, .47f, woodLight, { 0, .46f, 0 });
    box(g, "back-panel", .47f, .44f, .065f, woodLight, { 0, .79f, -.21f });
    for (int sx : { -1, 1 }) for (int sz : { -1, 1 })
        box(g, std::string("leg-") + (sx < 0 ? "left" : "right") +
            (sz < 0 ? "-back" : "-front"), .035f, .44f, .035f,
            steel, { sx * .205f, .22f, sz * .19f });
    box(g, "rear-crossbar", .40f, .035f, .035f, steel, { 0, .29f, -.19f });
}

void serviceCounter(g3d::Object3D& g) {
    box(g, "counter-body", 2.05f, .93f, .72f, wood, { 0, .465f, 0 });
    box(g, "stone-top", 2.15f, .055f, .82f, paleTile, { 0, .96f, 0 });
    box(g, "front-inset", 1.76f, .68f, .018f, woodLight, { 0, .48f, .371f });
    box(g, "toe-kick", 1.87f, .10f, .045f, charcoal, { 0, .05f, .38f });
    box(g, "card-terminal", .12f, .19f, .11f, polymer, { .75f, 1.09f, -.20f });
    box(g, "terminal-display", .09f, .055f, .008f, screen, { .75f, 1.14f, -.137f });
}

void marketShelf(g3d::Object3D& g) {
    box(g, "shelf-back", 1.52f, 1.92f, .045f, steel, { 0, .96f, -.28f });
    for (int i = 0; i < 4; ++i) {
        float y = .18f + i * .44f;
        box(g, "shelf-tier-" + std::to_string(i + 1), 1.52f, .045f, .62f,
            enamel, { 0, y, .02f });
        box(g, "price-rail-" + std::to_string(i + 1), 1.48f, .075f, .018f,
            blue, { 0, y + .01f, .335f });
        for (int j = 0; j < 7; ++j)
            box(g, "stock-" + std::to_string(i + 1) + "-" + std::to_string(j + 1),
                .13f, .23f + (j % 3) * .025f, .22f,
                j % 3 == 0 ? red : (j % 3 == 1 ? green : yellow),
                { -.64f + j * .21f, y + .14f, -.035f });
    }
    for (int sx : { -1, 1 })
        box(g, sx < 0 ? "left-upright" : "right-upright", .055f, 1.94f, .64f,
            steel, { sx * .76f, .97f, .015f });
}

void checkout(g3d::Object3D& g) {
    box(g, "checkout-body", 1.84f, .76f, .71f, enamel, { 0, .38f, 0 });
    box(g, "conveyor-belt", 1.02f, .025f, .62f, charcoal, { -.38f, .78f, 0 });
    box(g, "scanner-pad", .38f, .02f, .51f, glass, { .53f, .78f, 0 });
    box(g, "cashier-screen", .34f, .25f, .035f, polymer, { .56f, 1.04f, -.23f });
    box(g, "cashier-display", .30f, .20f, .007f, screen, { .56f, 1.04f, -.208f });
    box(g, "receipt-printer", .24f, .17f, .23f, steel, { .55f, .87f, .29f });
    box(g, "bagging-platform", .42f, .055f, .60f, stainless, { 1.11f, .73f, 0 });
    box(g, "checkout-divider", .035f, .18f, .62f, yellow, { .18f, .87f, 0 });
}

void refrigeratedCase(g3d::Object3D& g) {
    box(g, "cabinet-back", 1.58f, 1.71f, .055f, enamel, { 0, .855f, -.37f });
    box(g, "cabinet-base", 1.58f, .22f, .79f, enamel, { 0, .11f, 0 });
    box(g, "cabinet-top", 1.58f, .18f, .79f, enamel, { 0, 1.62f, 0 });
    for (int sx : { -1, 1 })
        box(g, sx < 0 ? "left-side" : "right-side", .065f, 1.71f, .79f,
            enamel, { sx * .755f, .855f, 0 });
    box(g, "glass-front", 1.45f, 1.30f, .012f, glass, { 0, .94f, .405f });
    for (int i = 0; i < 3; ++i) {
        float y = .42f + i * .39f;
        box(g, "interior-shelf-" + std::to_string(i + 1), 1.38f, .025f, .58f,
            stainless, { 0, y, 0 });
        for (int j = 0; j < 5; ++j)
            box(g, "chilled-item-" + std::to_string(i + 1) + "-" + std::to_string(j + 1),
                .18f, .22f, .16f, j % 2 ? green : red,
                { -.54f + j * .27f, y + .12f, .08f });
    }
    box(g, "top-sign", 1.58f, .20f, .025f, blue, { 0, 1.61f, .414f });
    box(g, "bottom-vent", 1.37f, .10f, .014f, charcoal, { 0, .11f, .408f });
    for (int sx : { -1, 1 })
        box(g, sx < 0 ? "left-door-handle" : "right-door-handle", .025f, .25f, .035f,
            stainless, { sx * .10f, 1.00f, .445f });
}

void build(g3d::Object3D& group, Kind kind) {
    switch (kind) {
    case Kind::HospitalBed: hospitalBed(group); break;
    case Kind::BedsideCabinet: bedsideCabinet(group); break;
    case Kind::IvStand: ivStand(group); break;
    case Kind::ExamTable: examTable(group); break;
    case Kind::Wheelchair: wheelchair(group); break;
    case Kind::MedicalCart: medicalCart(group); break;
    case Kind::StudentDesk: studentDesk(group); break;
    case Kind::ClassroomChair: classroomChair(group); break;
    case Kind::TeacherDesk: teacherDesk(group); break;
    case Kind::Whiteboard: whiteboardStand(group); break;
    case Kind::Bookshelf: bookshelf(group); break;
    case Kind::SchoolLockers: lockers(group); break;
    case Kind::OfficeDesk: officeDesk(group); break;
    case Kind::ErgonomicChair: ergonomicChair(group); break;
    case Kind::MeetingTable: meetingTable(group); break;
    case Kind::FilingCabinet: filingCabinet(group); break;
    case Kind::CubiclePartition: cubicle(group); break;
    case Kind::Printer: printer(group); break;
    case Kind::ServerRack: serverRack(group); break;
    case Kind::NetworkCabinet: networkCabinet(group); break;
    case Kind::UpsCabinet: upsCabinet(group); break;
    case Kind::Workstation: workstation(group); break;
    case Kind::CoolingUnit: coolingUnit(group); break;
    case Kind::SecurityCamera: securityCamera(group); break;
    case Kind::Sofa: sofa(group); break;
    case Kind::DiningTable: diningTable(group); break;
    case Kind::Refrigerator: refrigerator(group); break;
    case Kind::KitchenRange: kitchenRange(group); break;
    case Kind::WashingMachine: washingMachine(group); break;
    case Kind::FloorLamp: floorLamp(group); break;
    case Kind::CafeTable: cafeTable(group); break;
    case Kind::RestaurantChair: restaurantChair(group); break;
    case Kind::ServiceCounter: serviceCounter(group); break;
    case Kind::MarketShelf: marketShelf(group); break;
    case Kind::Checkout: checkout(group); break;
    case Kind::RefrigeratedCase: refrigeratedCase(group); break;
    default: throw std::invalid_argument("unknown interior3d::Kind");
    }
}

} // namespace

const char* name(Kind kind) {
    switch (kind) {
    case Kind::HospitalBed: return "hospital-bed";
    case Kind::BedsideCabinet: return "hospital-bedside-cabinet";
    case Kind::IvStand: return "hospital-iv-stand";
    case Kind::ExamTable: return "hospital-exam-table";
    case Kind::Wheelchair: return "hospital-wheelchair";
    case Kind::MedicalCart: return "hospital-medical-cart";
    case Kind::StudentDesk: return "school-student-desk";
    case Kind::ClassroomChair: return "school-classroom-chair";
    case Kind::TeacherDesk: return "school-teacher-desk";
    case Kind::Whiteboard: return "school-whiteboard";
    case Kind::Bookshelf: return "school-bookshelf";
    case Kind::SchoolLockers: return "school-lockers";
    case Kind::OfficeDesk: return "office-desk";
    case Kind::ErgonomicChair: return "office-ergonomic-chair";
    case Kind::MeetingTable: return "office-meeting-table";
    case Kind::FilingCabinet: return "office-filing-cabinet";
    case Kind::CubiclePartition: return "office-cubicle-partition";
    case Kind::Printer: return "office-printer";
    case Kind::ServerRack: return "it-server-rack";
    case Kind::NetworkCabinet: return "it-network-cabinet";
    case Kind::UpsCabinet: return "it-ups-cabinet";
    case Kind::Workstation: return "it-workstation";
    case Kind::CoolingUnit: return "it-cooling-unit";
    case Kind::SecurityCamera: return "it-security-camera";
    case Kind::Sofa: return "home-sofa";
    case Kind::DiningTable: return "home-dining-table";
    case Kind::Refrigerator: return "home-refrigerator";
    case Kind::KitchenRange: return "home-kitchen-range";
    case Kind::WashingMachine: return "home-washing-machine";
    case Kind::FloorLamp: return "home-floor-lamp";
    case Kind::CafeTable: return "retail-cafe-table";
    case Kind::RestaurantChair: return "retail-restaurant-chair";
    case Kind::ServiceCounter: return "retail-service-counter";
    case Kind::MarketShelf: return "retail-market-shelf";
    case Kind::Checkout: return "retail-checkout";
    case Kind::RefrigeratedCase: return "retail-refrigerated-case";
    default: throw std::invalid_argument("unknown interior3d::Kind");
    }
}

g3d::Object3D* add(g3d::Object3D& parent, Kind kind) {
    auto* root = parent.add();
    root->name = name(kind);
    build(*root, kind);
    return root;
}

g3d::Object3D* add(g3d::Scene& scene, Kind kind) {
    auto* root = scene.add();
    root->name = name(kind);
    build(*root, kind);
    return root;
}

} // namespace rmtn::interior3d
