// Native PBR computer-component prefabs for g3d scenes.
// Local origin is footprint centre at floor level, +y up, +z front.
#pragma once

#include "scene3d.hpp"
#include <array>

namespace rmtn::computer3d {

enum class Kind {
    Cpu, RamModule, GraphicsCard, PowerSupply, PcCase, CpuAirCooler,
    Motherboard, NvmeSsd, SataSsd, HardDrive, CaseFan, AioCooler,
};

constexpr std::array<Kind, 12> all() {
    return { Kind::Cpu, Kind::RamModule, Kind::GraphicsCard, Kind::PowerSupply,
             Kind::PcCase, Kind::CpuAirCooler, Kind::Motherboard, Kind::NvmeSsd,
             Kind::SataSsd, Kind::HardDrive, Kind::CaseFan, Kind::AioCooler };
}

const char* name(Kind kind);
g3d::Object3D* add(g3d::Object3D& parent, Kind kind);
g3d::Object3D* add(g3d::Scene& scene, Kind kind);

} // namespace rmtn::computer3d
