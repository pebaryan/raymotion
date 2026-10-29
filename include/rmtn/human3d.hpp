// Native PBR city-scale human role prefabs for g3d scenes.
// Local origin is between the feet; +y up, +z front. Dimensions are metres.
#pragma once

#include "scene3d.hpp"
#include <array>

namespace rmtn::human3d {

enum class Role {
    Resident, Commuter, EvDriver, ChargingTechnician, GridEngineer, ConstructionWorker,
    OfficeWorker, HealthcareWorker, Student, Teacher, RetailWorker, TransitOperator,
};

constexpr std::array<Role, 12> all() {
    return { Role::Resident, Role::Commuter, Role::EvDriver,
             Role::ChargingTechnician, Role::GridEngineer, Role::ConstructionWorker,
             Role::OfficeWorker, Role::HealthcareWorker, Role::Student,
             Role::Teacher, Role::RetailWorker, Role::TransitOperator };
}

const char* name(Role role);
g3d::Object3D* add(g3d::Object3D& parent, Role role);
g3d::Object3D* add(g3d::Scene& scene, Role role);

} // namespace rmtn::human3d
