#include "rmtn/smartgrid3d.hpp"

namespace rmtn::smartgrid3d {
namespace {

struct Finish {
    Color color;
    float roughness = .7f, metalness = 0, opacity = 1;
};

Finish finish(const std::string& role) {
    if (role == "house-wall") return { { 216, 204, 185, 255 }, .86f };
    if (role == "apartment-wall") return { { 168, 185, 186, 255 }, .79f };
    if (role == "office-wall") return { { 113, 150, 167, 255 }, .52f, .18f };
    if (role == "shop-wall") return { { 206, 182, 162, 255 }, .76f };
    if (role == "factory-wall") return { { 227, 226, 220, 255 }, .70f, .08f };
    if (role == "warehouse-wall") return { { 182, 186, 192, 255 }, .54f, .36f };
    if (role == "assembly-wall") return { { 164, 182, 188, 255 }, .58f, .28f };
    if (role == "data-center-wall") return { { 113, 137, 151, 255 }, .52f, .28f };
    if (role == "power-hall-wall") return { { 168, 178, 183, 255 }, .64f, .27f };
    if (role == "control-wall") return { { 190, 197, 192, 255 }, .72f, .12f };
    if (role == "civic-wall") return { { 199, 192, 174, 255 }, .84f };
    if (role == "civic-roof") return { { 93, 117, 128, 255 }, .43f, .62f };
    if (role == "event-wall") return { { 200, 208, 209, 255 }, .56f, .24f };
    if (role == "roof") return { { 105, 120, 127, 255 }, .58f, .25f };
    if (role == "transformer") return { { 37, 44, 54, 255 }, .43f, .56f };
    if (role == "hv-transformer") return { { 69, 93, 111, 255 }, .36f, .68f };
    if (role == "insulator") return { { 179, 137, 105, 255 }, .24f, .03f };
    if (role == "tower-metal") return { { 215, 221, 218, 255 }, .44f, .62f };
    if (role == "turbine-blade") return { { 224, 231, 228, 255 }, .48f, .12f };
    if (role == "generator-enclosure") return { { 129, 151, 155, 255 }, .51f, .47f };
    if (role == "exhaust") return { { 111, 123, 130, 255 }, .42f, .70f };
    if (role == "solar") return { { 27, 77, 118, 255 }, .18f, .38f };
    if (role == "glass") return { { 92, 176, 210, 255 }, .10f, .02f, .45f };
    if (role == "door") return { { 102, 91, 85, 255 }, .54f, .16f };
    if (role == "dark") return { { 45, 53, 64, 255 }, .50f, .22f };
    if (role == "metal") return { { 129, 147, 152, 255 }, .33f, .79f };
    if (role == "trim") return { { 117, 133, 142, 255 }, .47f, .51f };
    if (role == "accent") return { { 237, 183, 95, 255 }, .38f, .22f };
    if (role == "charger") return { { 86, 200, 181, 255 }, .32f, .21f };
    if (role == "pavement") return { { 139, 150, 147, 255 }, .94f };
    if (role == "marking") return { { 238, 224, 167, 255 }, .85f };
    if (role == "car-body") return { { 225, 173, 94, 255 }, .25f, .44f };
    if (role == "storage") return { { 121, 147, 158, 255 }, .45f, .55f };
    if (role == "bark") return { { 100, 76, 59, 255 }, .93f };
    if (role == "leaf-green") return { { 76, 132, 81, 255 }, .84f };
    if (role == "leaf-light") return { { 114, 162, 92, 255 }, .88f };
    if (role == "leaf-dark") return { { 49, 101, 79, 255 }, .83f };
    if (role == "grass") return { { 93, 148, 79, 255 }, .96f };
    if (role == "soil") return { { 97, 79, 63, 255 }, .98f };
    if (role == "stone") return { { 169, 163, 148, 255 }, .87f };
    if (role == "fruit") return { { 201, 91, 68, 255 }, .50f };
    if (role == "flower") return { { 238, 177, 80, 255 }, .66f };
    if (role == "signal-housing") return { { 52, 65, 72, 255 }, .44f, .42f };
    if (role == "lamp-red") return { { 218, 57, 48, 255 }, .20f, .04f };
    if (role == "lamp-amber") return { { 222, 156, 49, 255 }, .23f, .04f };
    if (role == "lamp-green") return { { 68, 178, 105, 255 }, .23f, .04f };
    if (role == "lamp-light") return { { 250, 240, 201, 255 }, .14f, .02f };
    if (role == "sign-red") return { { 192, 54, 47, 255 }, .37f, .10f };
    if (role == "sign-yellow") return { { 232, 185, 73, 255 }, .42f, .08f };
    if (role == "sign-blue") return { { 49, 105, 154, 255 }, .40f, .15f };
    if (role == "sign-white") return { { 239, 240, 230, 255 }, .44f, .05f };
    if (role == "wood") return { { 143, 105, 70, 255 }, .78f };
    if (role == "bin") return { { 78, 107, 112, 255 }, .56f, .20f };
    if (role == "reflective") return { { 235, 232, 204, 255 }, .18f, .05f };
    return { { 170, 182, 187, 255 }, .7f };
}

Color faceColor(const city::Object& object, const city::Face& face) {
    const std::string role = object.attributes["paletteRole"].asString();
    if (face.kind == "RoofSurface") {
        if (role == "house-wall" || role == "shop-wall" || role == "warehouse-wall")
            return { 185, 103, 87, 255 };
        if (role == "apartment-wall") return { 100, 127, 141, 255 };
        if (role == "factory-wall") return { 241, 241, 233, 255 };
        if (role == "assembly-wall") return { 196, 207, 205, 255 };
        if (role == "data-center-wall") return { 101, 126, 141, 255 };
        if (role == "power-hall-wall") return { 105, 132, 148, 255 };
        if (role == "control-wall") return { 106, 127, 135, 255 };
        if (role == "civic-wall") return { 93, 117, 128, 255 };
        if (role == "event-wall") return { 127, 158, 169, 255 };
    }
    if (role == "pavement") {
        if (face.kind == "TrafficArea") return { 101, 113, 122, 255 };
        if (face.kind == "AuxiliaryTrafficArea") return { 155, 165, 161, 255 };
    }
    return finish(role).color;
}

} // namespace

city::Look cityLook() {
    city::Look look;
    look.color = [](const city::Object& object, const city::Face& face) {
        return faceColor(object, face);
    };
    look.material = [](const city::Object& object, g3d::Material& material) {
        const Finish f = finish(object.attributes["paletteRole"].asString());
        material.color = WHITE; // vertex colour supplies the object's base colour
        material.roughness = f.roughness;
        material.metalness = f.metalness;
        material.opacity = f.opacity;
        material.shading = g3d::Shading::Standard;
        const std::string role = object.attributes["paletteRole"].asString();
        if (role == "lamp-red") material.emissive = { 95, 13, 10, 255 };
        if (role == "lamp-light") material.emissive = { 155, 143, 100, 255 };
    };
    return look;
}

} // namespace rmtn::smartgrid3d
