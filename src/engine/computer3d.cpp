#include "rmtn/computer3d.hpp"
#include <raymath.h>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace rmtn::computer3d {
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

const g3d::Material pcb = pbr({ 32, 75, 67, 255 }, .64f, .07f);
const g3d::Material darkPcb = pbr({ 34, 45, 52, 255 }, .65f, .06f);
const g3d::Material black = pbr({ 28, 34, 39, 255 }, .67f, .05f);
const g3d::Material matte = pbr({ 49, 59, 67, 255 }, .78f, .09f);
const g3d::Material steel = pbr({ 137, 153, 161, 255 }, .32f, .82f);
const g3d::Material aluminum = pbr({ 187, 198, 201, 255 }, .27f, .89f);
const g3d::Material copper = pbr({ 185, 112, 68, 255 }, .28f, .84f);
const g3d::Material gold = pbr({ 221, 176, 89, 255 }, .30f, .79f);
const g3d::Material chip = pbr({ 39, 44, 49, 255 }, .39f, .12f);
const g3d::Material glass = pbr({ 161, 208, 220, 255 }, .12f, .02f, .27f);
const g3d::Material teal = pbr({ 62, 160, 171, 255 }, .37f, .13f);
const g3d::Material blue = pbr({ 60, 110, 155, 255 }, .42f, .12f);
const g3d::Material white = pbr({ 220, 225, 221, 255 }, .52f, .11f);

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
                   g3d::Material material, Vector3 base, int slices = 24) {
    return part(root, std::move(name), g3d::Geometry::cylinder(r, h, slices), material,
                { base.x, base.y + h / 2, base.z });
}

g3d::Object3D* ring(g3d::Object3D& root, std::string name, float radius,
                    float tube, g3d::Material material, Vector3 at) {
    return part(root, std::move(name), g3d::Geometry::torus(radius, tube, 36, 10), material, at);
}

void fanFace(g3d::Object3D& g, const std::string& prefix, Vector3 centre, float radius) {
    ring(g, prefix + "-rim", radius, radius * .085f, black, centre);
    cyl(g, prefix + "-hub", radius * .20f, .012f, matte,
        { centre.x, centre.y - .006f, centre.z + .004f }, 24)->rotation.x = 90;
    for (int i = 0; i < 7; ++i) {
        float a = i * 2.f * PI / 7.f;
        auto* blade = box(g, prefix + "-blade-" + std::to_string(i + 1),
                          radius * .64f, radius * .23f, .006f, matte,
                          { centre.x + std::cos(a) * radius * .43f,
                            centre.y + std::sin(a) * radius * .43f, centre.z + .002f });
        blade->rotation.z = a * RAD2DEG + 24.f;
    }
}

void cpu(g3d::Object3D& g) {
    box(g, "package-substrate", .043f, .0025f, .043f, pcb, { 0, .002f, 0 });
    box(g, "heat-spreader", .035f, .004f, .035f, aluminum, { 0, .0052f, 0 });
    box(g, "etched-centre", .020f, .0004f, .009f, steel, { 0, .0074f, 0 });
    for (int i = 0; i < 12; ++i) {
        float p = -.018f + i * .0033f;
        box(g, "contact-left-" + std::to_string(i + 1), .0013f, .0005f, .002f,
            gold, { p, .0005f, -.019f });
        box(g, "contact-right-" + std::to_string(i + 1), .0013f, .0005f, .002f,
            gold, { p, .0005f, .019f });
    }
}

void ram(g3d::Object3D& g) {
    box(g, "memory-pcb", .133f, .028f, .002f, pcb, { 0, .018f, 0 });
    for (int i = 0; i < 8; ++i) {
        float x = -.052f + i * .015f;
        box(g, "dram-chip-" + std::to_string(i + 1), .012f, .013f, .001f,
            chip, { x, .023f, .0016f });
        box(g, "gold-contact-" + std::to_string(i + 1), .007f, .004f, .0007f,
            gold, { x, .004f, .0017f });
    }
    box(g, "heat-spreader-front", .124f, .010f, .0013f, aluminum, { 0, .035f, .002f });
    box(g, "heat-spreader-back", .124f, .010f, .0013f, aluminum, { 0, .035f, -.002f });
    box(g, "top-accent", .116f, .002f, .003f, teal, { 0, .041f, 0 });
}

void gpu(g3d::Object3D& g) {
    box(g, "graphics-pcb", .303f, .108f, .004f, darkPcb, { 0, .063f, 0 });
    box(g, "front-shroud", .286f, .096f, .033f, matte, { .004f, .065f, .021f });
    for (int i = 0; i < 2; ++i)
        fanFace(g, "gpu-fan-" + std::to_string(i + 1),
                { -.071f + i * .141f, .067f, .041f }, .044f);
    box(g, "rear-bracket", .014f, .135f, .061f, steel, { -.153f, .067f, .013f });
    for (int i = 0; i < 3; ++i)
        box(g, "display-port-" + std::to_string(i + 1), .004f, .014f, .011f,
            black, { -.162f, .035f + i * .032f, .037f });
    for (int i = 0; i < 14; ++i)
        box(g, "pcie-contact-" + std::to_string(i + 1), .008f, .004f, .001f,
            gold, { -.105f + i * .014f, .005f, .003f });
    box(g, "power-socket", .038f, .018f, .025f, black, { .109f, .124f, 0 });
    box(g, "edge-accent", .221f, .006f, .008f, teal, { 0, .121f, .016f });
}

void psu(g3d::Object3D& g) {
    box(g, "psu-housing", .150f, .086f, .160f, steel, { 0, .043f, 0 });
    box(g, "psu-top-label", .108f, .001f, .085f, matte, { 0, .087f, 0 });
    box(g, "rear-switch", .025f, .015f, .005f, black, { -.042f, .062f, .083f });
    box(g, "power-inlet", .034f, .025f, .005f, black, { .034f, .061f, .083f });
    box(g, "modular-panel", .122f, .058f, .005f, black, { 0, .045f, -.083f });
    for (int i = 0; i < 4; ++i)
        box(g, "cable-socket-" + std::to_string(i + 1), .019f, .013f, .006f,
            copper, { -.042f + i * .028f, .045f, -.087f });
    ring(g, "psu-fan-grille", .045f, .004f, black, { 0, .089f, 0 })->rotation.x = 90;
    for (int i = 0; i < 5; ++i)
        box(g, "grille-bar-" + std::to_string(i + 1), .083f, .002f, .002f,
            black, { 0, .090f, -.032f + i * .016f });
}

void pcCase(g3d::Object3D& g) {
    box(g, "case-bottom", .232f, .012f, .455f, steel, { 0, .006f, 0 });
    box(g, "case-top", .232f, .014f, .455f, steel, { 0, .449f, 0 });
    box(g, "case-back", .232f, .440f, .012f, steel, { 0, .231f, -.222f });
    box(g, "case-front", .232f, .440f, .018f, matte, { 0, .231f, .220f });
    box(g, "side-glass", .005f, .423f, .424f, glass, { .117f, .231f, 0 });
    box(g, "motherboard-tray", .008f, .378f, .357f, black, { -.095f, .226f, -.019f });
    box(g, "psu-shroud", .184f, .076f, .238f, matte, { 0, .053f, -.097f });
    for (int i = 0; i < 3; ++i)
        fanFace(g, "front-fan-" + std::to_string(i + 1),
                { 0, .114f + i * .123f, .231f }, .047f);
    for (int sx : { -1, 1 }) for (int sz : { -1, 1 })
        box(g, std::string("foot-") + (sx < 0 ? "left" : "right") +
            (sz < 0 ? "-back" : "-front"), .028f, .015f, .033f,
            black, { sx * .082f, -.007f, sz * .176f });
    box(g, "power-button", .012f, .005f, .012f, teal, { -.075f, .459f, .173f });
}

void cpuAirCooler(g3d::Object3D& g) {
    box(g, "mounting-base", .060f, .008f, .060f, copper, { 0, .004f, 0 });
    for (int i = 0; i < 12; ++i)
        box(g, "heatsink-fin-" + std::to_string(i + 1), .109f, .0022f, .083f,
            aluminum, { 0, .034f + i * .008f, 0 });
    for (int sx : { -1, 1 })
        cyl(g, sx < 0 ? "left-heatpipe" : "right-heatpipe", .004f, .12f,
            copper, { sx * .037f, .004f, -.028f }, 12);
    fanFace(g, "cooler-fan", { 0, .079f, .048f }, .048f);
    box(g, "fan-frame-top", .110f, .008f, .008f, black, { 0, .133f, .048f });
    box(g, "fan-frame-bottom", .110f, .008f, .008f, black, { 0, .025f, .048f });
}

void motherboard(g3d::Object3D& g) {
    box(g, "atx-pcb", .305f, .244f, .007f, darkPcb, { 0, .122f, 0 });
    box(g, "cpu-socket", .062f, .062f, .008f, steel, { -.061f, .155f, .008f });
    box(g, "socket-centre", .047f, .047f, .004f, black, { -.061f, .155f, .015f });
    for (int i = 0; i < 4; ++i)
        box(g, "dimm-slot-" + std::to_string(i + 1), .004f, .110f, .008f,
            black, { .071f + i * .013f, .155f, .009f });
    for (int i = 0; i < 3; ++i)
        box(g, "pcie-slot-" + std::to_string(i + 1), .160f, .006f, .011f,
            i == 0 ? steel : black, { -.034f, .031f + i * .025f, .010f });
    box(g, "vrm-heatsink", .055f, .027f, .015f, aluminum, { -.120f, .196f, .012f });
    box(g, "chipset-heatsink", .056f, .048f, .015f, aluminum, { .072f, .070f, .012f });
    for (int i = 0; i < 5; ++i)
        box(g, "sata-port-" + std::to_string(i + 1), .014f, .010f, .011f,
            black, { .131f, .018f + i * .019f, .011f });
    box(g, "rear-io-block", .016f, .071f, .024f, steel, { -.146f, .193f, .006f });
    for (int i = 0; i < 7; ++i)
        box(g, "trace-" + std::to_string(i + 1), .074f, .0015f, .001f,
            copper, { -.044f + i * .009f, .106f + i * .006f, .004f });
}

void nvme(g3d::Object3D& g) {
    box(g, "m2-pcb", .080f, .003f, .022f, pcb, { 0, .002f, 0 });
    for (int i = 0; i < 3; ++i)
        box(g, "nand-package-" + std::to_string(i + 1), .015f, .002f, .016f,
            chip, { -.022f + i * .020f, .0045f, 0 });
    box(g, "controller", .010f, .002f, .012f, chip, { .031f, .0045f, 0 });
    for (int i = 0; i < 5; ++i)
        box(g, "m2-contact-" + std::to_string(i + 1), .0015f, .0005f, .003f,
            gold, { -.039f, .0037f, -.009f + i * .004f });
    cyl(g, "mount-hole", .0025f, .0007f, gold, { .037f, .003f, 0 }, 12);
}

void sataSsd(g3d::Object3D& g) {
    box(g, "ssd-shell", .070f, .007f, .100f, matte, { 0, .0035f, 0 });
    box(g, "top-label", .054f, .0008f, .069f, blue, { 0, .0075f, -.006f });
    box(g, "sata-data-connector", .023f, .003f, .009f, black, { -.017f, .003f, -.054f });
    box(g, "sata-power-connector", .035f, .003f, .009f, black, { .021f, .003f, -.054f });
    for (int i = 0; i < 4; ++i)
        box(g, "shell-screw-" + std::to_string(i + 1), .003f, .0005f, .003f,
            steel, { (i % 2 ? 1 : -1) * .029f, .008f,
                     (i / 2 ? 1 : -1) * .043f });
}

void hardDrive(g3d::Object3D& g) {
    box(g, "drive-body", .102f, .026f, .147f, matte, { 0, .013f, 0 });
    box(g, "top-cover", .098f, .002f, .143f, aluminum, { 0, .027f, 0 });
    cyl(g, "platter-boss", .032f, .001f, steel, { -.014f, .028f, -.017f }, 32);
    box(g, "cover-label", .050f, .0008f, .046f, white, { .010f, .029f, .035f });
    box(g, "sata-data-connector", .023f, .005f, .009f, black, { -.022f, .010f, -.078f });
    box(g, "sata-power-connector", .036f, .005f, .009f, black, { .023f, .010f, -.078f });
    for (int i = 0; i < 4; ++i)
        cyl(g, "cover-screw-" + std::to_string(i + 1), .002f, .001f,
            steel, { (i % 2 ? 1 : -1) * .041f, .028f,
                     (i / 2 ? 1 : -1) * .063f }, 12);
}

void caseFan(g3d::Object3D& g) {
    box(g, "square-frame", .122f, .122f, .024f, black, { 0, .061f, 0 });
    box(g, "inner-opening", .107f, .107f, .026f, matte, { 0, .061f, .001f });
    fanFace(g, "rotor", { 0, .061f, .016f }, .049f);
    for (int sx : { -1, 1 }) for (int sy : { -1, 1 })
        cyl(g, std::string("mount-hole-") + (sx < 0 ? "left" : "right") +
            (sy < 0 ? "-bottom" : "-top"), .004f, .003f, steel,
            { sx * .052f, .061f + sy * .052f, .013f }, 12)->rotation.x = 90;
    ring(g, "accent-ring", .054f, .0015f, teal, { 0, .061f, .019f });
}

void aioCooler(g3d::Object3D& g) {
    box(g, "radiator-core", .252f, .124f, .028f, black, { 0, .062f, -.025f });
    for (int i = 0; i < 14; ++i)
        box(g, "radiator-fin-" + std::to_string(i + 1), .003f, .113f, .002f,
            steel, { -.116f + i * .018f, .062f, -.009f });
    for (int i = 0; i < 2; ++i)
        fanFace(g, "radiator-fan-" + std::to_string(i + 1),
                { -.063f + i * .126f, .062f, .007f }, .049f);
    cyl(g, "pump-block", .039f, .025f, matte, { 0, 0, .174f }, 32);
    cyl(g, "pump-face", .031f, .002f, teal, { 0, .025f, .174f }, 32);
    for (int sx : { -1, 1 }) {
        const std::string side = sx < 0 ? "left" : "right";
        box(g, side + "-hose-run", .012f, .012f, .193f, black,
            { sx * .027f, .129f, .072f });
        box(g, side + "-hose-drop", .012f, .103f, .012f, black,
            { sx * .027f, .077f, .167f });
        cyl(g, side + "-radiator-fitting", .009f, .016f, copper,
            { sx * .027f, .117f, -.025f }, 12);
    }
}

void build(g3d::Object3D& group, Kind kind) {
    switch (kind) {
    case Kind::Cpu: cpu(group); break;
    case Kind::RamModule: ram(group); break;
    case Kind::GraphicsCard: gpu(group); break;
    case Kind::PowerSupply: psu(group); break;
    case Kind::PcCase: pcCase(group); break;
    case Kind::CpuAirCooler: cpuAirCooler(group); break;
    case Kind::Motherboard: motherboard(group); break;
    case Kind::NvmeSsd: nvme(group); break;
    case Kind::SataSsd: sataSsd(group); break;
    case Kind::HardDrive: hardDrive(group); break;
    case Kind::CaseFan: caseFan(group); break;
    case Kind::AioCooler: aioCooler(group); break;
    default: throw std::invalid_argument("unknown computer3d::Kind");
    }
}

} // namespace

const char* name(Kind kind) {
    switch (kind) {
    case Kind::Cpu: return "pc-cpu";
    case Kind::RamModule: return "pc-ram-module";
    case Kind::GraphicsCard: return "pc-graphics-card";
    case Kind::PowerSupply: return "pc-power-supply";
    case Kind::PcCase: return "pc-case";
    case Kind::CpuAirCooler: return "pc-cpu-air-cooler";
    case Kind::Motherboard: return "pc-motherboard";
    case Kind::NvmeSsd: return "pc-nvme-ssd";
    case Kind::SataSsd: return "pc-sata-ssd";
    case Kind::HardDrive: return "pc-hard-drive";
    case Kind::CaseFan: return "pc-case-fan";
    case Kind::AioCooler: return "pc-aio-cooler";
    default: throw std::invalid_argument("unknown computer3d::Kind");
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

} // namespace rmtn::computer3d
