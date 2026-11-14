"""Writes a mesh built in Unreal's own space (X forward, Y right, Z up, centimetres) as a .gltf + .bin + textures.

glTF is right-handed, Y up, in metres. Unreal 5.8's glTF importer (Interchange) reads glTF (x, y, z) as Unreal
(X, Z, Y) x 100 - measured by importing a box that sat at known positive offsets. Builders here make triangles whose
right-hand normal points outwards in Unreal tuples; swapping two axes mirrors that, so the writer reverses the winding.
"""
import json, os
import numpy as np

# Unreal (X, Y, Z) cm -> glTF (x, y, z) m
AXES = np.array([[1.0, 0.0, 0.0],   # glTF x = Unreal X
                 [0.0, 0.0, 1.0],   # glTF y = Unreal Z (up)
                 [0.0, 1.0, 0.0]])  # glTF z = Unreal Y
SCALE = 0.01


def to_gltf(p):
    return (AXES @ np.asarray(p, dtype=np.float64).T).T * SCALE


def _flips_handedness():
    return np.linalg.det(AXES) < 0  # a mirroring map turns outward-facing triangles inward unless their winding is reversed


def write(path, mesh, materials, extra_images=None):
    """mesh: {material name: (positions Nx3 cm, normals Nx3, uvs Nx2, triangles Mx3)} in Unreal space.
    materials: {name: dict(color=(r,g,b[,a]) linear, metallic, roughness, emissive=(r,g,b), texture='file.png', blend=False)}"""
    base = os.path.splitext(os.path.basename(path))[0]
    blob = bytearray()
    views, accessors, prims, gmats, images, textures = [], [], [], [], [], []

    def view(data, target):
        while len(blob) % 4: blob.append(0)
        off = len(blob); blob.extend(data)
        views.append({"buffer": 0, "byteOffset": off, "byteLength": len(data), "target": target})
        return len(views) - 1

    def accessor(arr, ctype, kind, target, minmax=False):
        a = {"bufferView": view(arr.tobytes(), target), "componentType": ctype, "count": int(len(arr)), "type": kind}
        if minmax: a["min"] = arr.min(axis=0).tolist(); a["max"] = arr.max(axis=0).tolist()
        accessors.append(a)
        return len(accessors) - 1

    flip = _flips_handedness()
    for name, (pos, nrm, uv, tri) in mesh.items():
        if len(tri) == 0: continue
        if name not in [m["name"] for m in gmats]:
            m = materials[name]
            pbr = {"baseColorFactor": list(m.get("color", (0.8, 0.8, 0.8))) + ([] if len(m.get("color", (0, 0, 0))) == 4 else [1.0]),
                   "metallicFactor": float(m.get("metallic", 0.0)), "roughnessFactor": float(m.get("roughness", 0.6))}
            if m.get("texture"):
                images.append({"uri": m["texture"]}); textures.append({"source": len(images) - 1})
                pbr["baseColorTexture"] = {"index": len(textures) - 1}
            g = {"name": name, "pbrMetallicRoughness": pbr}
            if m.get("emissive"): g["emissiveFactor"] = list(m["emissive"])
            if m.get("blend"): g["alphaMode"] = "BLEND"
            if m.get("two_sided"): g["doubleSided"] = True
            gmats.append(g)
        mi = [m["name"] for m in gmats].index(name)
        P = to_gltf(pos).astype(np.float32)
        N = (AXES @ np.asarray(nrm, dtype=np.float64).T).T
        N = (N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)).astype(np.float32)
        T = np.asarray(tri, dtype=np.uint32)
        if flip: T = T[:, ::-1]
        prims.append({"attributes": {"POSITION": accessor(P, 5126, "VEC3", 34962, True), "NORMAL": accessor(N, 5126, "VEC3", 34962),
                                     "TEXCOORD_0": accessor(np.asarray(uv, dtype=np.float32), 5126, "VEC2", 34962)},
                      "indices": accessor(T.reshape(-1), 5125, "SCALAR", 34963), "material": mi})
    doc = {"asset": {"version": "2.0", "generator": "Homicide Unit PropKit"},
           "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"name": base, "mesh": 0}],
           "meshes": [{"name": base, "primitives": prims}], "materials": gmats,
           "buffers": [{"uri": base + ".bin", "byteLength": len(blob)}], "bufferViews": views, "accessors": accessors}
    if images: doc["images"] = images; doc["textures"] = textures; doc["samplers"] = [{}]
    with open(os.path.join(os.path.dirname(path), base + ".bin"), "wb") as f: f.write(bytes(blob))
    with open(path, "w") as f: json.dump(doc, f, indent=1)
