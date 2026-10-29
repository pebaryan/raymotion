// Native PBR furniture and equipment for civic, domestic and commercial interiors.
// Each builder returns a named Object3D group; origin is floor-level centre, +z front.
#pragma once

#include "scene3d.hpp"
#include <array>

namespace rmtn::interior3d {

enum class Kind {
    HospitalBed, BedsideCabinet, IvStand, ExamTable, Wheelchair, MedicalCart,
    StudentDesk, ClassroomChair, TeacherDesk, Whiteboard, Bookshelf, SchoolLockers,
    OfficeDesk, ErgonomicChair, MeetingTable, FilingCabinet, CubiclePartition, Printer,
    ServerRack, NetworkCabinet, UpsCabinet, Workstation, CoolingUnit, SecurityCamera,
    Sofa, DiningTable, Refrigerator, KitchenRange, WashingMachine, FloorLamp,
    CafeTable, RestaurantChair, ServiceCounter, MarketShelf, Checkout, RefrigeratedCase,
};

constexpr std::array<Kind, 36> all() {
    return { Kind::HospitalBed, Kind::BedsideCabinet, Kind::IvStand, Kind::ExamTable,
             Kind::Wheelchair, Kind::MedicalCart, Kind::StudentDesk, Kind::ClassroomChair,
             Kind::TeacherDesk, Kind::Whiteboard, Kind::Bookshelf, Kind::SchoolLockers,
             Kind::OfficeDesk, Kind::ErgonomicChair, Kind::MeetingTable,
             Kind::FilingCabinet, Kind::CubiclePartition, Kind::Printer,
             Kind::ServerRack, Kind::NetworkCabinet, Kind::UpsCabinet,
             Kind::Workstation, Kind::CoolingUnit, Kind::SecurityCamera,
             Kind::Sofa, Kind::DiningTable, Kind::Refrigerator, Kind::KitchenRange,
             Kind::WashingMachine, Kind::FloorLamp, Kind::CafeTable,
             Kind::RestaurantChair, Kind::ServiceCounter, Kind::MarketShelf,
             Kind::Checkout, Kind::RefrigeratedCase };
}

const char* name(Kind kind);
g3d::Object3D* add(g3d::Object3D& parent, Kind kind);
g3d::Object3D* add(g3d::Scene& scene, Kind kind);

} // namespace rmtn::interior3d
