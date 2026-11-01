// rig:: skinned meshes -- a body bound to the humanoid skeleton by tools/skin_bake.py,
// posed by the same joints and clips as the primitive characters.
#include "rig_internal.hpp"
#include "rmtn/json.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace rmtn {
namespace rig {

namespace {
// The bones the baker writes: every humanoid joint before Floor, in Joint order.
constexpr int kSkinBones = (int)Joint::Floor;

Build buildFrom(const Json& j) {
    Build b = Build::adult();
    auto num = [&](const char* key, float& v) { v = (float)j.num(key, v); };
    num("head", b.head); num("neck", b.neck); num("hips", b.hips); num("hipWidth", b.hipWidth);
    num("upperArm", b.upperArm); num("forearm", b.forearm); num("armRadius", b.armRadius); num("hand", b.hand);
    num("thigh", b.thigh); num("shin", b.shin); num("legRadius", b.legRadius); num("foot", b.foot);
    num("ankle", b.ankle); num("shoulderZ", b.shoulderZ);
    if (j["torso"].size() == 3)
        b.torso = { (float)j["torso"][0].asNumber(), (float)j["torso"][1].asNumber(), (float)j["torso"][2].asNumber() };
    return b;
}
} // namespace

Skin loadSkin(const std::string& glb) {
    const std::string side = glb + ".rig.json";
    std::ifstream in(std::filesystem::u8path(side), std::ios::binary);
    if (!in) throw std::runtime_error("rig::loadSkin: no " + side + " (bake the mesh with tools/skin_bake.py)");
    std::stringstream ss;
    ss << in.rdbuf();
    const Json doc = Json::parse(ss.str());
    if (doc.str("skeleton", "") != "humanoid")
        throw std::runtime_error("rig::loadSkin: " + side + " is not bound to the humanoid skeleton");
    Skin s;
    s.build = buildFrom(doc["build"]);
    if (doc.has("bind"))
        for (const auto& [name, rot] : doc["bind"].asObject())
            s.bind[joint(s.build, name)] = { (float)rot[0].asNumber(), (float)rot[1].asNumber(), (float)rot[2].asNumber() };
    s.model = g3d::loadModel(glb);
    s.model->dualQuaternion = true;
    return s;
}

std::vector<Matrix> skinMatrices(const Build& b, const Pose& bind, const Pose& p) {
    const Frames f0 = solve(b, bind), f = solve(b, p);
    std::vector<Matrix> m((size_t)kSkinBones);
    for (int j = 0; j < kSkinBones; j++) m[(size_t)j] = MatrixMultiply(MatrixInvert(f0[(size_t)j]), f[(size_t)j]);
    return m;
}

void pose(const Skin& s, const Pose& p) {
    const std::vector<Matrix> m = skinMatrices(s.build, s.bind, p);
    s.model->poseBones(m.data(), (int)m.size());
}

g3d::Object3D* add(g3d::Scene& scene, CharacterPtr ch, const Skin& skin, int fps) {
    g3d::Object3D* root = scene.add();
    root->name = "character";
    g3d::Object3D* body = root->add(skin.model);
    body->name = "skin";
    body->up = [ch, skin, fps](int t, g3d::Object3D&) { pose(skin, ch->poseAt(t / (double)fps)); };
    return root;
}

} // namespace rig
} // namespace rmtn
