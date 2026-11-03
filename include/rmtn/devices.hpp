// rmtn/devices.hpp -- device prefabs for 2D in 3D (g3d::): a phone, tablet, laptop, monitor or
// browser window built round a g3d::Surface, so an app or a page drawn with the ordinary 2D
// calls has a body to sit in.
//
//     auto ui = std::make_shared<g3d::Surface>();
//     ui->pixels = { 390, 844 };
//     ui->draw = [](const Ctx& c, int t) { drawApp(c, t); };
//     g3d::Device phone = g3d::addDevice(*scene, g3d::DeviceKind::Phone, ui);
//     phone.root->rotation = { -20, 0, 0 };                        // place and turn the root
//     scene->camera = g3d::fillCamera(scene->camera, *scene, *phone.screen, 0, 16.f / 9);
//
// The screen's size comes from the kind (a phone 71.5 mm wide, a laptop 30 cm, ...; or
// DeviceStyle::width) and its height from the surface's pixels, so the picture is never
// stretched; `ui->size` is set for you. The screen's rounded corners are painted into the
// surface's own drawing (in the bezel colour), so `ui->draw` through g3d::fillCtx draws the
// same corners and the cut from the 3D screen to the 2D frame stays invisible.
#pragma once
#include "scene3d.hpp"
#include <memory>
#include <string>

namespace rmtn {
namespace g3d {

enum class DeviceKind {
    Phone,      // a slab, screen on +z, origin at its centre
    Tablet,     // likewise
    Laptop,     // base on y = 0 (origin at its centre), lid hinged at the back edge
    Monitor,    // on a stand, foot on y = 0
    Browser,    // a floating window: a title bar with an address over the page, origin at its centre
};

struct DeviceStyle {
    Color body{ 30, 32, 37, 255 };      // the case (a browser's frame)
    Color bezel{ 8, 9, 11, 255 };       // the glass round the screen, and the screen's corners
    float metalness = 0.4f, roughness = 0.35f;
    float width = 0;                    // the screen's width in metres (0 = the kind's own)
    float lidOpen = 110;                // laptop: degrees between lid and base
    std::string url = "raymotion.dev";  // browser: the address bar
    Color chrome{ 233, 236, 241, 255 }; // browser: the title bar
};

struct Device {
    Object3D* root = nullptr;           // place and turn this
    Object3D* screen = nullptr;         // the Surface object (for fillCamera)
    Object3D* lid = nullptr;            // laptop: rotation.x = 90 - degrees open (0 = upright)
    std::shared_ptr<Surface> ui;
    Vector2 screenSize{ 0, 0 };         // metres
};

Device addDevice(Object3D& parent, DeviceKind kind, std::shared_ptr<Surface> ui, const DeviceStyle& style = {});
Device addDevice(Scene& scene, DeviceKind kind, std::shared_ptr<Surface> ui, const DeviceStyle& style = {});

} // namespace g3d
} // namespace rmtn
