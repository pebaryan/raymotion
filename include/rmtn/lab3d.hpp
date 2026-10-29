// Native, reusable PBR laboratory equipment for g3d scenes.
// Each builder returns a named Object3D group with individually named, lit parts.
// Local origin is the footprint centre at floor level; +z is the front.
#pragma once

#include "city.hpp"
#include <array>

namespace rmtn::lab3d {

enum class Kind {
    Bench, FumeHood, ChemicalCabinet, AnalyticalBalance,
    HotplateStirrer, RotaryEvaporator, Hplc, Centrifuge,
    DryingOven, JacketedReactor, GlasswareRack, VacuumPump,
};

constexpr std::array<Kind, 12> all() {
    return { Kind::Bench, Kind::FumeHood, Kind::ChemicalCabinet, Kind::AnalyticalBalance,
             Kind::HotplateStirrer, Kind::RotaryEvaporator, Kind::Hplc, Kind::Centrifuge,
             Kind::DryingOven, Kind::JacketedReactor, Kind::GlasswareRack, Kind::VacuumPump };
}

const char* name(Kind kind);
g3d::Object3D* add(g3d::Object3D& parent, Kind kind);
g3d::Object3D* add(g3d::Scene& scene, Kind kind);
// Applies a matching per-object PBR palette to the optional chemistry CityJSON files.
city::Look cityLook();

} // namespace rmtn::lab3d
