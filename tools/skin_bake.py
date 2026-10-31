#!/usr/bin/env python3
"""skin_bake.py -- bind a scanned or generated T-pose body to raymotion's humanoid skeleton.

    python tools/skin_bake.py body.glb out.glb [--height 1.7] [--crotch 0.455] [--flip]

The input is a GLB in a T-pose (arms straight out along x, facing +z, y up, under 65535
vertices after welding: decimate a raw generator mesh first). The script

  1. welds it, stands it on y = 0 and centres its hips on the y axis;
  2. finds the crotch and separates the legs below it (generated bodies often join the
     thighs with a web of skin that would stretch between them);
  3. fits a rig::Build to it: the shoulders, arms and hands are measured from the mesh; the
     hips, knees and ankles from the leg cross-sections and standard segment ratios;
  4. computes bone-heat skin weights (Baran & Popovic 2007, "Automatic rigging and
     animation of 3D characters"): each bone's weight diffuses over the surface from the
     vertices it is nearest to and can see;
  5. writes out.glb (one bone per rig::Joint up to AnkleR, a T-pose bind) and
     out.glb.rig.json (the fitted build and the bind pose) for rig::loadSkin.

Only numpy and scipy are needed.
"""
import argparse, json, struct, sys
import numpy as np
import scipy.sparse as sp
import scipy.sparse.linalg as sla

JOINTS = ["root", "pelvis", "chest", "neck", "head", "shoulder-L", "elbow-L", "wrist-L",
          "shoulder-R", "elbow-R", "wrist-R", "hip-L", "knee-L", "ankle-L", "hip-R", "knee-R", "ankle-R"]
PARENT = [0, 0, 1, 2, 3, 2, 5, 6, 2, 8, 9, 1, 11, 12, 1, 14, 15]
ROOT, PELVIS, CHEST, NECK, HEAD, SHL, ELL, WRL, SHR, ELR, WRR, HIPL, KNL, ANL, HIPR, KNR, ANR = range(17)

# Standard segment heights as fractions of stature (Drillis & Contini 1966, via Winter,
# "Biomechanics and Motor Control of Human Movement"): hip joint, knee, ankle.
HIP_H, KNEE_H, ANKLE_H = 0.530, 0.285, 0.039


# ------------------------------------------------------------------ GLB in
def read_glb(path):
    raw = open(path, "rb").read()
    if raw[:4] != b"glTF":
        sys.exit(f"{path}: not a GLB")
    jlen = struct.unpack_from("<I", raw, 12)[0]
    gl = json.loads(raw[20:20 + jlen])
    blen = struct.unpack_from("<I", raw, 20 + jlen)[0]
    binc = raw[28 + jlen:28 + jlen + blen]
    comp = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
    width = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}

    def accessor(i):
        a = gl["accessors"][i]
        v = gl["bufferViews"][a["bufferView"]]
        dt, n, w = np.dtype(comp[a["componentType"]]), a["count"], width[a["type"]]
        off = v.get("byteOffset", 0) + a.get("byteOffset", 0)
        stride = v.get("byteStride", dt.itemsize * w)
        rows = np.ndarray((n, w), dt, binc, off, (stride, dt.itemsize))
        return np.array(rows)

    def local(node):
        if "matrix" in node:
            return np.array(node["matrix"], float).reshape(4, 4).T
        t = np.array(node.get("translation", [0, 0, 0]), float)
        x, y, z, w = node.get("rotation", [0, 0, 0, 1])
        s = np.array(node.get("scale", [1, 1, 1]), float)
        R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                      [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                      [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
        M = np.eye(4)
        M[:3, :3] = R * s
        M[:3, 3] = t
        return M

    V, F, UV = [], [], []
    def walk(i, parent):
        node = gl["nodes"][i]
        M = parent @ local(node)
        if "mesh" in node:
            for prim in gl["meshes"][node["mesh"]]["primitives"]:
                if prim.get("mode", 4) != 4:
                    continue
                p = accessor(prim["attributes"]["POSITION"]).astype(float)
                p = (np.c_[p, np.ones(len(p))] @ M.T)[:, :3]
                f = accessor(prim["indices"]).reshape(-1, 3) if "indices" in prim else np.arange(len(p)).reshape(-1, 3)
                F.append(f.astype(np.int64) + sum(len(v) for v in V))
                V.append(p)
                UV.append(accessor(prim["attributes"]["TEXCOORD_0"]).astype(float) if "TEXCOORD_0" in prim["attributes"] else None)
        for c in node.get("children", []):
            walk(c, M)
    scene = gl["scenes"][gl.get("scene", 0)]
    for n in scene["nodes"]:
        walk(n, np.eye(4))
    uv = np.concatenate(UV) if (UV and all(u is not None for u in UV)) else None
    return np.concatenate(V), np.concatenate(F), uv


def read_glb_texture(path):
    """The first embedded image of a GLB (png/jpeg bytes), or None."""
    raw = open(path, "rb").read()
    if raw[:4] != b"glTF":
        return None
    jlen = struct.unpack_from("<I", raw, 12)[0]
    gl = json.loads(raw[20:20 + jlen])
    if not gl.get("images") or not gl.get("bufferViews"):
        return None
    img = gl["images"][0]
    if "bufferView" not in img:
        return None
    blen = struct.unpack_from("<I", raw, 20 + jlen)[0]
    binc = raw[28 + jlen:28 + jlen + blen]
    v = gl["bufferViews"][img["bufferView"]]
    off, n = v.get("byteOffset", 0), v["byteLength"]
    return binc[off:off + n]


def weld(V, F, UV=None, tol=1e-5):
    # Cluster by position AND uv: a UV seam has two vertices at one position with
    # different texcoords; collapsing them would drag triangles across the atlas.
    key = np.round(V / tol).astype(np.int64)
    if UV is not None:
        key = np.hstack([key, np.round(UV / 1e-4).astype(np.int64)])
    _, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    inv = inv.ravel()
    F = inv[F]
    F = F[(F[:, 0] != F[:, 1]) & (F[:, 1] != F[:, 2]) & (F[:, 2] != F[:, 0])]
    if UV is not None:
        UV = UV[first]
    return V[first], F, UV


# ------------------------------------------------------------------ geometry helpers
def normals(V, F):
    fn = np.cross(V[F[:, 1]] - V[F[:, 0]], V[F[:, 2]] - V[F[:, 0]])
    N = np.zeros_like(V)
    for k in range(3):
        np.add.at(N, F[:, k], fn)
    return N / (np.linalg.norm(N, axis=1, keepdims=True) + 1e-12), fn


def separate_legs(V, F, UV, crotch):
    """Faces below the crotch go to the leg on their side; a vertex shared by both legs'
    faces is split in two so the legs part instead of stretching a web between them.
    Returns V, F, UV, the split count and each vertex's leg (+1 left, -1 right, 0 neither)."""
    c = V[F].mean(1)
    low = c[:, 1] < crotch
    right = low & (c[:, 0] < 0)
    left = low & ~right
    used_l = np.zeros(len(V), bool); used_l[F[left].ravel()] = True
    used_r = np.zeros(len(V), bool); used_r[F[right].ravel()] = True
    shared = np.where(used_l & used_r)[0]
    side = np.where(used_l, 1, np.where(used_r, -1, 0))
    if not len(shared):
        return V, F, UV, 0, side
    copy = np.full(len(V), -1)
    copy[shared] = len(V) + np.arange(len(shared))
    F = F.copy()
    sub = F[right]
    sub = np.where(copy[sub] >= 0, copy[sub], sub)
    F[right] = sub
    V = np.concatenate([V, V[shared]])
    if UV is not None:
        UV = np.concatenate([UV, UV[shared]])
    return V, F, UV, len(shared), np.concatenate([side, -np.ones(len(shared), int)])


def find_crotch(V, F, H):
    """The crotch: the bottom of the arch between the legs, where the centre line's surface
    faces down (the inner thighs below face sideways, even where they touch)."""
    N, _ = normals(V, F)
    c = (np.abs(V[:, 0]) < 0.015) & (V[:, 1] > 0.42 * H) & (V[:, 1] < 0.55 * H) & (N[:, 1] < -0.5)
    if c.sum() < 10:
        print("  crotch detection unsure; using 0.47 of the height", file=sys.stderr)
        return 0.47 * H
    return float(np.percentile(V[c, 1], 10))


def slab(V, axis, at, half=0.006, mask=None):
    s = np.abs(V[:, axis] - at) < half
    return s if mask is None else s & mask


# ------------------------------------------------------------------ fitting
def fit(V, H):
    """A rig::Build for a T-pose body of height H (y up, hips centred on x = z = 0)."""
    up = V[:, 1] > 0.6 * H
    tip = V[up, 0].max()                                   # left fingertips (+x)
    tipR = -V[up, 0].min()
    reach = (tip + tipR) / 2

    def span(x, axis):                                     # cross-section extent of the arm
        s = slab(V, 0, x, mask=up)
        return (V[s, axis].max() - V[s, axis].min()) if s.sum() > 5 else np.inf

    # The armpit: walking out from the chest, the first slice that is only arm thick.
    thick = span(0.7 * reach, 1)
    xs = np.arange(0.06 * H, 0.6 * reach, 0.003)
    armpit = next((x for x in xs if span(x, 1) < 1.8 * thick), 0.12 * H)
    shoulder_x = armpit - 0.35 * thick
    arm = up & (V[:, 0] > armpit) & (V[:, 0] < armpit + 0.15 * H)
    shoulder_y = np.median(V[arm, 1])
    shoulder_z = np.median(V[arm, 2])
    arm_r = thick / 2

    # The wrist: the slimmest section (height x depth) in the outer part of the arm.
    xs = np.arange(shoulder_x + 0.55 * (reach - shoulder_x), reach - 0.05 * H, 0.003)
    area = [span(x, 1) * span(x, 2) for x in xs]
    wrist_x = xs[int(np.argmin(area))]
    hand = reach - wrist_x                                  # wrist to fingertips
    std = 0.108 * H                                         # (Drillis & Contini)
    if not 0.7 * std < hand < 1.3 * std:
        print(f"  wrist detection unsure ({hand:.3f} m hand); using {std:.3f} m", file=sys.stderr)
        hand, wrist_x = std, reach - std
    upper = (wrist_x - shoulder_x) * 0.186 / (0.186 + 0.146)  # upper arm : forearm
    fore = wrist_x - shoulder_x - upper

    # Legs: the hip joints over the thighs' centres; heights from the standard ratios. Legs
    # seldom hang plumb (they close in towards the knees): the bind pose turns the hips and
    # knees in so each bone runs down the middle of its limb (the rig's rest legs are plumb).
    thighs = slab(V, 1, 0.47 * H, 0.01)
    hip_x = np.median(np.abs(V[thighs, 0]))
    hip_y, knee_y, ankle_y = HIP_H * H, KNEE_H * H, ANKLE_H * H
    def leg_x(y):                                         # the left leg's centre at height y
        return float(V[slab(V, 1, y, 0.01) & (V[:, 0] > 0), 0].mean())
    knee_x, low_y = leg_x(knee_y), 0.12 * H
    lean_thigh = np.degrees(np.arctan2(hip_x - knee_x, hip_y - knee_y))
    lean_shin = np.degrees(np.arctan2(knee_x - leg_x(low_y), knee_y - low_y))
    leg_r = np.median(np.abs(np.abs(V[thighs, 0]) - hip_x)) * 1.3
    feet = V[:, 1] < 0.05 * H
    foot = V[feet, 2].max() - V[feet, 2].min()

    # The trunk, from the shoulder line (rig::restOffset): the shoulder joint sits
    # torso.y - 1.2 armRadius above the chest joint and torso.x / 2 + 0.6 armRadius out.
    adult_h = 1.75
    hips = 0.14 * H / adult_h
    chest_y = hip_y + hips / 2
    torso_y = shoulder_y - chest_y + 1.2 * arm_r
    neck = 0.05 * H / adult_h
    head = H - (chest_y + torso_y + neck)
    depth = V[slab(V, 1, chest_y + 0.6 * torso_y, 0.01) & (np.abs(V[:, 0]) < 0.1 * H), 2]
    build = {
        "head": head, "neck": neck,
        "torso": [2 * (shoulder_x - 0.6 * arm_r), torso_y, float(depth.max() - depth.min()) if len(depth) else 0.2 * H / adult_h],
        "hips": hips, "hipWidth": 2 * hip_x,
        "upperArm": upper, "forearm": fore, "armRadius": arm_r, "hand": hand,
        "thigh": (hip_y - knee_y) / np.cos(np.radians(lean_thigh)), "shin": knee_y - ankle_y, "legRadius": leg_r,
        "foot": foot, "ankle": ankle_y, "shoulderZ": shoulder_z,
    }
    build = {k: (float(v) if not isinstance(v, list) else [float(x) for x in v]) for k, v in build.items()}
    # Joint rotations of the bind pose (rig::Pose degrees; the right side mirrors): arms
    # straight out, legs leaning in. z > 0 swings a limb out to the side.
    bind = {"shoulder-L": [0, 0, 90], "shoulder-R": [0, 0, 90],
            "hip-L": [0, 0, -lean_thigh], "hip-R": [0, 0, -lean_thigh],
            "knee-L": [0, 0, lean_thigh - lean_shin], "knee-R": [0, 0, lean_thigh - lean_shin]}
    return build, {k: [round(float(x), 3) for x in v] for k, v in bind.items()}


def frames(b, bind):
    """Joint world frames (4x4, column vectors) in the bind pose, as rig::solve makes them
    (bind: joint name -> [0, 0, z] degrees; only z turns are used here)."""
    sx, sy = b["torso"][0] / 2 + b["armRadius"] * 0.6, b["torso"][1] - b["armRadius"] * 1.2
    pelvis = b["ankle"] + b["shin"] + b["thigh"]
    off = {ROOT: (0, 0, 0), PELVIS: (0, pelvis, 0), CHEST: (0, b["hips"] / 2, 0), NECK: (0, b["torso"][1], 0),
           HEAD: (0, b["neck"], 0), SHL: (sx, sy, b["shoulderZ"]), SHR: (-sx, sy, b["shoulderZ"]),
           ELL: (0, -b["upperArm"], 0), ELR: (0, -b["upperArm"], 0), WRL: (0, -b["forearm"], 0), WRR: (0, -b["forearm"], 0),
           HIPL: (b["hipWidth"] / 2, 0, 0), HIPR: (-b["hipWidth"] / 2, 0, 0),
           KNL: (0, -b["thigh"], 0), KNR: (0, -b["thigh"], 0), ANL: (0, -b["shin"], 0), ANR: (0, -b["shin"], 0)}
    def rz(deg):
        a = np.radians(deg)
        return np.array([[np.cos(a), -np.sin(a), 0], [np.sin(a), np.cos(a), 0], [0, 0, 1]])
    W = []
    for j in range(17):
        L = np.eye(4)
        L[:3, 3] = off[j]
        if JOINTS[j] in bind:
            z = bind[JOINTS[j]][2]
            L[:3, :3] = rz(-z if JOINTS[j].endswith("-R") else z)   # the right side mirrors
        W.append(L if j == ROOT else W[PARENT[j]] @ L)
    return W


# ------------------------------------------------------------------ weights
def heat_weights(V, F, segs, crotch, roots, pits, side):
    """segs: bone index -> (a, b) segment; roots: limb root bone -> (joint, along, fade) for
    the falloff past the joint; pits: arm bones -> (shoulder joint, arm radius) for the cut
    under the arm. Returns J4 (uint8), W4 (float32) and the vertex normals."""
    N, fn = normals(V, F)
    ids = list(segs)
    Hb = V[:, 1].max()
    D = np.zeros((len(V), len(ids)))
    for i, j in enumerate(ids):
        a, b = segs[j]
        ab = b - a
        t = np.clip(((V - a) @ ab) / (ab @ ab), 0, 1)
        d = V - (a + t[:, None] * ab)
        dn = np.linalg.norm(d, axis=1)
        # Seen from the bone: the surface faces away from it (a ray test's cheap stand-in).
        facing = np.einsum("ij,ij->i", N, d) / (dn + 1e-12)
        D[:, i] = np.where(facing > -0.2, dn, dn * 4 + 1)
    near = D.argmin(1)
    H = 1.0 / np.maximum(D[np.arange(len(V)), near], 0.01) ** 2

    def cot(a, b):
        return np.einsum("ij,ij->i", a, b) / (np.linalg.norm(np.cross(a, b), axis=1) + 1e-12)
    I, J, Wc = [], [], []
    for i0, i1, i2 in [(0, 1, 2), (1, 2, 0), (2, 0, 1)]:
        a, b, c = F[:, i0], F[:, i1], F[:, i2]
        w = np.maximum(0.5 * cot(V[b] - V[a], V[c] - V[a]), 1e-4)   # obtuse: keep positive
        I += [b, c]; J += [c, b]; Wc += [w, w]
    n = len(V)
    Coff = sp.coo_matrix((np.concatenate(Wc), (np.concatenate(I), np.concatenate(J))), shape=(n, n)).tocsr()
    C = sp.diags(np.asarray(Coff.sum(1)).ravel()) - Coff
    M = np.zeros(n)
    for k in range(3):
        np.add.at(M, F[:, k], np.linalg.norm(fn, axis=1) / 6)
    M = np.maximum(M, 1e-12)
    solve = sla.factorized((C + sp.diags(M * H)).tocsc())
    W = np.column_stack([solve(M * H * (near == i)) for i in range(len(ids))])
    W = np.clip(W, 0, None)

    # A limb's root bone fades out past its joint, towards the body: heat creeps up the
    # groin into the belly and from the armpit across the chest, and would drag them along.
    for j, (a, b, m) in roots.items():
        i = ids.index(j)
        t = (V - a) @ ((b - a) / np.linalg.norm(b - a))      # along the bone from its joint
        W[:, i] *= np.clip((t + m) / m, 0, 1)
    # Under a T-pose arm the ribs lie beside the arm's root, not past it, and nearer it than
    # the spine: they took arm weight and caved in when the arm came down. Below the arm's
    # underside and inside the armpit, the trunk owns the surface.
    for j, (p, r) in pits.items():
        i = ids.index(j)
        x = V[:, 0] * np.sign(p[0])                          # outwards on this side
        below = (p[1] - r) - V[:, 1]
        inside = (abs(p[0]) + 0.7 * r) - x                       # the armpit is 0.7 r out from the joint
        W[:, i] *= 1 - np.clip(below / (0.6 * r), 0, 1) * np.clip(inside / r, 0, 1)
    # The inner thigh just below the crotch lies nearer the pelvis bone's foot than the
    # leaning thigh bone: held by the pelvis it stayed put as the leg swung, flattening and
    # tearing. Below the crotch the legs own the surface.
    i = ids.index(PELVIS)
    W[:, i] *= np.clip((V[:, 1] - (crotch - 0.02 * Hb)) / (0.02 * Hb), 0, 1)
    # The chest bends the trunk at the waist: its heat reached down to the crotch.
    i = ids.index(CHEST)
    W[:, i] *= np.clip((V[:, 1] - (segs[CHEST][0][1] - 0.06 * Hb)) / (0.04 * Hb), 0, 1)
    # One leg never moves the other side (+x is the left): heat crosses the gluteal cleft and
    # the pelvis. Below the crotch, cut by leg. Above it the midline belongs to the pelvis:
    # held by both legs, the arch from pubis to cleft was pulled forward and back at once
    # when they scissored, a sheet between them. A leg's weight rises from 0 there to full
    # a little out to its side.
    low = V[:, 1] < crotch
    f = 0.015 * Hb
    for i, j in enumerate(ids):
        for leg, sgn in (((HIPL, KNL, ANL), 1), ((HIPR, KNR, ANR), -1)):
            if j in leg:
                x = V[:, 0] * sgn
                mine = np.where(side != 0, side == sgn, x > 0)    # below the crotch: by leg, not by x
                W[:, i] *= np.where(low, mine, np.clip(x / (2 * f), 0, 1))
    # A light smoothing per bone (never per slot: a slot holds different bones at
    # neighbouring vertices), so joints bend over a band rather than a seam.
    A = (Coff > 0).astype(float)
    A = sp.diags(1 / np.maximum(np.asarray(A.sum(1)).ravel(), 1)) @ A
    for _ in range(2):
        W = 0.5 * W + 0.5 * (A @ W)
    W[W < 0.01] = 0

    top = np.argsort(-W, axis=1)[:, :4]
    W4 = np.take_along_axis(W, top, 1)
    s = W4.sum(1, keepdims=True)
    orphan = s[:, 0] <= 0
    W4 = np.where(s > 0, W4 / np.maximum(s, 1e-12), [1, 0, 0, 0])
    J4 = np.array(ids, np.uint8)[top]
    J4[orphan, 0] = np.array(ids, np.uint8)[near[orphan]]
    return J4, W4.astype(np.float32), N


# ------------------------------------------------------------------ GLB out
def write_glb(path, V, N, F, J4, W4, Wf, color, UV=None, tex=None):
    def quat(R):
        t = np.trace(R)
        if t > 0:
            s = np.sqrt(t + 1) * 2
            return [(R[2, 1] - R[1, 2]) / s, (R[0, 2] - R[2, 0]) / s, (R[1, 0] - R[0, 1]) / s, 0.25 * s]
        i = int(np.argmax(np.diag(R)))
        j, k = (i + 1) % 3, (i + 2) % 3
        s = np.sqrt(1 + R[i, i] - R[j, j] - R[k, k]) * 2
        q = [0, 0, 0, 0]
        q[i], q[j], q[k], q[3] = 0.25 * s, (R[j, i] + R[i, j]) / s, (R[k, i] + R[i, k]) / s, (R[k, j] - R[j, k]) / s
        return q
    nodes = []
    for j in range(17):
        L = Wf[j] if j == ROOT else np.linalg.inv(Wf[PARENT[j]]) @ Wf[j]
        node = {"name": JOINTS[j], "translation": [float(x) for x in L[:3, 3]], "rotation": [float(x) for x in quat(L[:3, :3])]}
        kids = [c for c in range(1, 17) if PARENT[c] == j]
        if kids: node["children"] = kids
        nodes.append(node)
    chunks, views, accs = [], [], []
    def add(data, ctype, count, atype, target=None, minmax=None):
        while sum(len(c) for c in chunks) % 4: chunks.append(b"\0")
        views.append({"buffer": 0, "byteOffset": sum(len(c) for c in chunks), "byteLength": len(data)})
        if target: views[-1]["target"] = target
        chunks.append(data)
        a = {"bufferView": len(views) - 1, "componentType": ctype, "count": count, "type": atype}
        if minmax: a["min"], a["max"] = minmax
        accs.append(a)
        return len(accs) - 1
    V32 = V.astype("<f4")
    pa = add(V32.tobytes(), 5126, len(V), "VEC3", 34962, (V32.min(0).tolist(), V32.max(0).tolist()))
    na = add(N.astype("<f4").tobytes(), 5126, len(V), "VEC3", 34962)
    ua = None
    if UV is not None:
        ua = add(UV.astype("<f4").tobytes(), 5126, len(UV), "VEC2", 34962)
    ja = add(J4.astype(np.uint8).tobytes(), 5121, len(V), "VEC4", 34962)
    wa = add(W4.astype("<f4").tobytes(), 5126, len(V), "VEC4", 34962)
    small = len(V) < 65536
    ia = add(F.astype("<u2" if small else "<u4").tobytes(), 5123 if small else 5125, F.size, "SCALAR", 34963)
    ib = add(b"".join(np.linalg.inv(M).T.astype("<f4").tobytes() for M in Wf), 5126, 17, "MAT4")
    tex_json = {}
    if tex:
        while sum(len(c) for c in chunks) % 4: chunks.append(b"\0")
        views.append({"buffer": 0, "byteOffset": sum(len(c) for c in chunks), "byteLength": len(tex)})
        chunks.append(tex)
        tex_json = {"samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}],
                    "images": [{"bufferView": len(views) - 1, "mimeType": "image/png" if tex[:4] == bytes([137]) + b"PNG" else "image/jpeg"}],
                    "textures": [{"sampler": 0, "source": 0}]}
    attrs = {"POSITION": pa, "NORMAL": na, "JOINTS_0": ja, "WEIGHTS_0": wa}
    if ua is not None:
        attrs["TEXCOORD_0"] = ua
    mat = ({"name": "body", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0, "roughnessFactor": 0.8}}
           if tex else {"name": "body", "pbrMetallicRoughness": {"baseColorFactor": color, "metallicFactor": 0, "roughnessFactor": 0.8}})
    nodes.append({"name": "body", "mesh": 0, "skin": 0})
    gl = {"asset": {"version": "2.0", "generator": "raymotion tools/skin_bake.py"},
          "scene": 0, "scenes": [{"nodes": [0, 17]}], "nodes": nodes,
          "skins": [{"joints": list(range(17)), "inverseBindMatrices": ib, "skeleton": 0}],
          "meshes": [{"primitives": [{"attributes": attrs, "indices": ia, "material": 0}]}],
          "materials": [mat],
          "buffers": [{"byteLength": sum(len(c) for c in chunks)}], "bufferViews": views, "accessors": accs, **tex_json}
    js = json.dumps(gl, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    bin_ = b"".join(chunks)
    bin_ += b"\0" * (-len(bin_) % 4)
    with open(path, "wb") as f:
        f.write(struct.pack("<4sII", b"glTF", 2, 28 + len(js) + len(bin_)))
        f.write(struct.pack("<I4s", len(js), b"JSON") + js)
        f.write(struct.pack("<I4s", len(bin_), b"BIN\0") + bin_)


# ------------------------------------------------------------------ main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mesh"); ap.add_argument("out")
    ap.add_argument("--height", type=float, help="scale the body to this height (metres)")
    ap.add_argument("--crotch", type=float, help="crotch height as a fraction of the height (default: found)")
    ap.add_argument("--flip", action="store_true", help="the body faces -z: turn it round")
    ap.add_argument("--color", default="0.36,0.39,0.47", help="base colour r,g,b (0..1)")
    ap.add_argument("--texture", help="png/jpeg to embed as baseColorTexture (overrides the source's own)")
    a = ap.parse_args()

    V, F, UV = weld(*read_glb(a.mesh))
    tex = read_glb_texture(a.mesh) if UV is not None else None
    if a.texture and UV is not None:
        tex = open(a.texture, "rb").read()
    if a.flip:
        V[:, [0, 2]] *= -1
    V[:, 1] -= V[:, 1].min()
    if a.height:
        V *= a.height / V[:, 1].max()
    H = V[:, 1].max()
    hipband = slab(V, 1, 0.55 * H, 0.03)
    V[:, 0] -= (V[hipband, 0].min() + V[hipband, 0].max()) / 2
    V[:, 2] -= (V[hipband, 2].min() + V[hipband, 2].max()) / 2
    print(f"mesh: {len(V)} vertices, {len(F)} faces, {H:.3f} m tall")
    if len(V) > 60000:
        sys.exit("too many vertices for a raylib mesh (65535 after the leg split): decimate it first")

    crotch = a.crotch * H if a.crotch else find_crotch(V, F, H)
    V, F, UV, split, side = separate_legs(V, F, UV, crotch)
    print(f"legs: {split} vertices split below {crotch:.3f} m")

    b, bind = fit(V, H)
    print("build:", json.dumps({k: (round(v, 3) if not isinstance(v, list) else [round(x, 3) for x in v]) for k, v in b.items()}))
    print("bind:", json.dumps(bind))
    Wf = frames(b, bind)
    P = [M[:3, 3] for M in Wf]
    def ext(j, d, n): return P[j] + np.asarray(d, float) / np.linalg.norm(d) * n
    segs = {PELVIS: (np.array([0, crotch, 0]), P[CHEST]),           # the groin and seat up to the waist
            CHEST: (P[CHEST], P[NECK]), NECK: (P[NECK], P[HEAD]),
            HEAD: (P[HEAD], np.array([0, H, P[HEAD][2]])),
            SHL: (P[SHL], P[ELL]), ELL: (P[ELL], P[WRL]), WRL: (P[WRL], ext(WRL, P[WRL] - P[ELL], b["hand"] * 0.9)),
            SHR: (P[SHR], P[ELR]), ELR: (P[ELR], P[WRR]), WRR: (P[WRR], ext(WRR, P[WRR] - P[ELR], b["hand"] * 0.9)),
            HIPL: (P[HIPL], P[KNL]), KNL: (P[KNL], P[ANL]), ANL: (P[ANL], P[ANL] + [0, -b["ankle"] * 0.5, b["foot"] * 0.6]),
            HIPR: (P[HIPR], P[KNR]), KNR: (P[KNR], P[ANR]), ANR: (P[ANR], P[ANR] + [0, -b["ankle"] * 0.5, b["foot"] * 0.6])}
    roots = {j: (segs[j][0], segs[j][1], f * H) for j, f in ((SHL, 0.03), (SHR, 0.03), (HIPL, 0.025), (HIPR, 0.025))}
    r = b["armRadius"]
    pits = {j: (P[s], r) for s, js in ((SHL, (SHL, ELL, WRL)), (SHR, (SHR, ELR, WRR))) for j in js}
    J4, W4, N = heat_weights(V, F, segs, crotch, roots, pits, side)
    # The ribs under the arms (as cut above): how much arm weight is left on them.
    for s, arm in ((SHL, (SHL, ELL, WRL)), (SHR, (SHR, ELR, WRR))):
        x = V[:, 0] * np.sign(P[s][0])
        rib = (V[:, 1] < P[s][1] - 1.6 * r) & (V[:, 1] > P[CHEST][1]) & (x > abs(P[s][0]) - 2 * r) & (x < abs(P[s][0]))
        w = (W4 * np.isin(J4, arm)).sum(1)[rib]
        print(f"ribs under {JOINTS[s]}: {rib.sum()} vertices, arm weight mean {w.mean():.3f}, max {w.max():.3f}")
    # The inner thighs below the crotch: how much the pelvis still holds them.
    thigh = (V[:, 1] < crotch) & (V[:, 1] > crotch - 0.06 * H) & (np.abs(V[:, 0]) < b["hipWidth"] / 2)
    w = (W4 * (J4 == PELVIS)).sum(1)[thigh]
    print(f"inner thighs: {thigh.sum()} vertices, pelvis weight mean {w.mean():.3f}, max {w.max():.3f}")
    dom = J4[:, 0]
    print("weights: dominant vertices per bone:", {JOINTS[j]: int((dom == j).sum()) for j in range(17) if (dom == j).any()})

    color = [float(x) for x in a.color.split(",")] + [1.0]
    if tex and UV is None:
        tex = None
    write_glb(a.out, V, N, F, J4, W4, Wf, color, UV=UV, tex=tex)
    side = {"skeleton": "humanoid", "source": a.mesh, "build": b,
            "bind": bind}
    with open(a.out + ".rig.json", "w") as f:
        json.dump(side, f, indent=1)
    print("wrote", a.out, "and", a.out + ".rig.json")


if __name__ == "__main__":
    main()
