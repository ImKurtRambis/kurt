"""Builds the precinct furniture as glTF files in Content/Art/Props/Source/<Name>/<Name>.gltf, ready for
Tools/ImportProps.py (run inside the editor) to turn into /Game/Art/Props/SM_<Name>.

  python -I Tools/PropKit/make_props.py            (the project's AI\\python works: it has numpy)

Every prop is modelled in Unreal's space, in centimetres, at real-world size:
  floor props - pivot at the floor, centred; front faces +X
  wall props  - pivot at the centre of the back (the wall); front faces +X
Five props come from Poly Haven (CC0, Source/PolyHaven/<id>/, downloaded 2026-11-14); they're re-wrapped here with a
quarter turn so they face +X like the rest. The game fits each mesh into the size its block used to have (HUBuild::Prop).
"""
import json, math, os, shutil, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from meshkit import Mesh, srgb, rot_y, rot_z
import gltf_out

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "Content", "Art", "Props", "Source")

# ---------------------------------------------------------------- materials (linear colours)
M = {
    "Paint":       dict(color=srgb("#7d8577"), roughness=0.55, metallic=0.1),   # institutional grey-green enamel
    "PaintLight":  dict(color=srgb("#8c9486"), roughness=0.5, metallic=0.1),
    "Khaki":       dict(color=srgb("#9d977f"), roughness=0.5, metallic=0.1),    # filing-cabinet beige-grey
    "KhakiLight":  dict(color=srgb("#aaa48c"), roughness=0.45, metallic=0.1),
    "Dark":        dict(color=srgb("#141414"), roughness=0.8),
    "Black":       dict(color=srgb("#1c1c1e"), roughness=0.55),
    "Chrome":      dict(color=srgb("#d8d8d8"), roughness=0.22, metallic=1.0),
    "Steel":       dict(color=srgb("#2a2b2d"), roughness=0.45, metallic=0.8),
    "Brass":       dict(color=srgb("#b8923a"), roughness=0.35, metallic=1.0),
    "Paper":       dict(color=srgb("#ece8dc"), roughness=0.9),
    "PaperYellow": dict(color=srgb("#e8d77a"), roughness=0.9),
    "PaperPink":   dict(color=srgb("#e4a9b4"), roughness=0.9),
    "PaperBlue":   dict(color=srgb("#a9c3e0"), roughness=0.9),
    "PinRed":      dict(color=srgb("#b3221d"), roughness=0.35),
    "Beige":       dict(color=srgb("#cdc5ad"), roughness=0.55),                 # 2000-era computer plastic
    "BeigeDark":   dict(color=srgb("#a49c86"), roughness=0.55),
    "KeyGrey":     dict(color=srgb("#6f6c66"), roughness=0.6),
    "Screen":      dict(color=srgb("#151a17"), roughness=0.08, emissive=srgb("#0b130e")),
    "LedGreen":    dict(color=srgb("#2fd35a"), roughness=0.3, emissive=srgb("#3cff6a")),
    "Lcd":         dict(color=srgb("#7b8a63"), roughness=0.3, emissive=srgb("#1c2614")),
    "PhoneBody":   dict(color=srgb("#2b2c2e"), roughness=0.5),
    "Button":      dict(color=srgb("#b9b7b1"), roughness=0.5),
    "Fabric":      dict(color=srgb("#23262c"), roughness=0.95),
    "Cooler":      dict(color=srgb("#e6e3da"), roughness=0.45),
    "TapCold":     dict(color=srgb("#2c5fb8"), roughness=0.35),
    "TapHot":      dict(color=srgb("#c0352b"), roughness=0.35),
    "Water":       dict(color=srgb(0.62, 0.80, 0.96, 0.32), roughness=0.04, blend=True),
    "Coat":        dict(color=srgb("#3a2f26"), roughness=0.9),
    "Cork":        dict(color=(1, 1, 1), roughness=0.95, texture="cork.png"),
    "Wood":        dict(color=(1, 1, 1), roughness=0.55, texture="wood.png"),
}


def textures(folder, needed):
    """cork and wood grain, made here (no downloads): 512 px tiles"""
    from PIL import Image  # noqa - available in AI\python (transformers needs it)
    rng = np.random.default_rng(7)
    n = 512
    # cork: tan with a scatter of darker and lighter granules
    base = np.array([0.72, 0.53, 0.33])
    img = np.ones((n, n, 3)) * base
    low = rng.normal(0, 1, (n // 8, n // 8)); low = np.kron(low, np.ones((8, 8))) * 0.05
    img += low[..., None]
    for _ in range(9000):
        x, y = rng.integers(0, n, 2); r = rng.integers(1, 4); d = rng.choice([-0.22, -0.12, 0.08])
        img[max(0, y - r):y + r, max(0, x - r):x + r] += d * np.array([1, 0.9, 0.8])
    if "cork.png" in needed: Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8)).save(os.path.join(folder, "cork.png"))
    # wood: warm brown, grain along U
    u = np.linspace(0, 1, n)[None, :]; v = np.linspace(0, 1, n)[:, None]
    warp = np.sin(v * 9 + np.sin(u * 3) * 1.5) * 0.15 + rng.normal(0, 0.02, (n, n))
    grain = 0.5 + 0.5 * np.sin((v + warp) * 140)
    g = 0.55 + 0.45 * grain ** 3
    wood = np.stack([0.45 * g, 0.29 * g, 0.17 * g], axis=-1) + rng.normal(0, 0.012, (n, n, 1))
    if "wood.png" in needed: Image.fromarray((np.clip(wood, 0, 1) * 255).astype(np.uint8)).save(os.path.join(folder, "wood.png"))


def save(name, mesh, extra_tex=()):
    folder = os.path.join(OUT, name); os.makedirs(folder, exist_ok=True)
    mats = {k: M[k] for k in mesh.parts}
    needed = {M[k]["texture"] for k in mats if M[k].get("texture")}
    if needed: textures(folder, needed)
    gltf_out.write(os.path.join(folder, name + ".gltf"), mesh.arrays(), mats)
    tris = sum(len(p["t"]) for p in mesh.parts.values())
    print(f"{name}: {tris} triangles, {len(mats)} materials")


# ---------------------------------------------------------------- the props
def locker(open_door=False):
    m = Mesh()
    m.box("Black", (-21, -18, 0), (21, 18, 10))
    if not open_door:
        m.box("Paint", (-23, -19, 10), (22, 19, 183), 0.8)
        door = None
    else:
        # a shell you can see into: back, sides, top, floor, a shelf, a coat on the hook
        for mn, mx in (((-23, -19, 10), (-21, 19, 183)), ((-23, -19, 10), (22, -17, 183)), ((-23, 17, 10), (22, 19, 183)),
                       ((-23, -19, 181), (22, 19, 183)), ((-23, -19, 10), (22, 19, 12.5))):
            m.box("Paint", mn, mx, 0.3)
        m.box("Dark", (-21, -17, 12.5), (-20.6, 17, 181))
        m.box("Paint", (-21, -17, 158), (20, 17, 159.5))
        m.cylinder("Chrome", (-20.6, 0, 150), (1, 0, 0), 0.6, 6, segs=8)
        m.box("Coat", (-14, -12, 82), (2, 12, 148), 3.0)
        m.box("Coat", (-12, -6, 148), (0, 6, 152), 1.0)
        door = rot_z(-100, (22.6, -18.4, 0))                                # swung out on its hinges
    m.box("PaintLight", (22, -17.6, 12.5), (23.2, 17.6, 180.5), 0.4, xf=door)   # the door
    for z0 in (22, 158):                                                    # louvres, top and bottom
        for i in range(6):
            m.box("Dark", (23.15, -8, z0 + i * 3.1), (23.5, 8, z0 + i * 3.1 + 1.3), xf=door)
    m.box("Chrome", (23.2, 11, 96), (25.4, 14, 116), 0.5, xf=door)          # latch handle
    m.box("Brass", (23.2, -4, 147), (23.5, 4, 151.5), xf=door)              # number plate
    for z in (30, 95, 160):                                                 # hinges
        m.cylinder("Steel", (23.4, -17.9, z), (0, 0, 1), 0.7, 6, segs=8)
    save("LockerOpen" if open_door else "Locker", m)


def filing_cabinet():
    m = Mesh()
    m.box("Khaki", (-33, -19, 1.5), (32, 19, 132), 0.8)
    m.box("Dark", (31.8, -18.2, 3), (32.05, 18.2, 131))                     # the gaps between drawers
    for x in (-30, 29):
        for y in (-16, 16):
            m.box("Dark", (x - 1.5, y - 1.5, 0), (x + 1.5, y + 1.5, 1.5))
    for i in range(4):
        z0 = 4 + i * 32; z1 = z0 + 30
        m.box("KhakiLight", (32, -18, z0), (34, 18, z1), 0.5)
        m.box("Chrome", (34, -8, z1 - 12), (36, 8, z1 - 9.5), 0.4)          # pull
        m.box("Chrome", (34, -5.5, z1 - 7), (34.4, 5.5, z1 - 3))            # label holder
        m.box("Paper", (34.4, -5, z1 - 6.6), (34.5, 5, z1 - 3.4))
    save("FilingCabinet", m)


def water_cooler():
    m = Mesh()
    m.box("Cooler", (-16, -16, 0), (16, 16, 96), 1.5)
    m.box("Dark", (16, -11, 8), (16.4, 11, 30))                              # grille
    for i in range(6):
        m.box("Cooler", (16.4, -11, 10 + i * 3.4), (16.6, 11, 11 + i * 3.4))
    m.box("Dark", (15.8, -11, 52), (16.2, 11, 78))                           # the dispensing recess
    m.box("Black", (16, -10, 52), (21, 10, 54))                              # drip tray
    for y, mat in ((-5, "TapCold"), (5, "TapHot")):
        m.cylinder(mat, (16, y, 72), (1, 0, 0), 1.4, 4, segs=12)
        m.box(mat, (19, y - 1.2, 66), (20.5, y + 1.2, 72))
    m.cylinder("Cooler", (0, 0, 96), (0, 0, 1), 8, 4, segs=24)               # collar
    m.cylinder("Water", (0, 0, 96), (0, 0, 1), 3.2, 5, segs=16, caps=(False, False))  # neck
    m.cylinder("Water", (0, 0, 100), (0, 0, 1), 4, 6, r1=13.5, segs=32, caps=(True, False))
    m.cylinder("Water", (0, 0, 106), (0, 0, 1), 13.5, 38, segs=32, caps=(False, False))
    m.cylinder("Water", (0, 0, 144), (0, 0, 1), 13.5, 3, r1=11.5, segs=32, caps=(False, True))
    for z in (117, 131):
        m.cylinder("Water", (0, 0, z), (0, 0, 1), 13.9, 1.2, segs=32)
    save("WaterCooler", m)


def cork_board(name, notices, w=120, h=90):
    m = Mesh()
    m.uv_scale["Cork"] = 1 / 60.0; m.uv_scale["Wood"] = 1 / 80.0
    hw, hh = w / 2, h / 2
    for (y0, y1, z0, z1) in ((-hw, -hw + 4, -hh, hh), (hw - 4, hw, -hh, hh), (-hw + 4, hw - 4, hh - 4, hh), (-hw + 4, hw - 4, -hh, -hh + 4)):
        m.box("Wood", (0, y0, z0), (3, y1, z1), 0.5)
    m.box("Cork", (0, -hw + 4, -hh + 4), (1.8, hw - 4, hh - 4))
    if notices:
        rng = np.random.default_rng(3)
        sheets = [(-45, -20, 10, 38, "Paper"), (-15, 12, 14, 36, "PaperYellow"), (18, 50, 8, 36, "Paper"),
                  (-48, -22, -36, 2, "PaperBlue"), (-14, 14, -34, 6, "Paper"), (22, 48, -30, 0, "PaperPink")]
        for (y0, y1, z0, z1, mat) in sheets:
            m.box(mat, (1.8, y0, z0), (1.9, y1, z1))
            pin_y = (y0 + y1) / 2 + rng.uniform(-3, 3)
            m.cylinder("PinRed", (1.9, pin_y, z1 - 2), (1, 0, 0), 0.7, 1.4, segs=8)
    save(name, m)


def desk_phone():
    m = Mesh()
    def z_at(x): return 3.5 + (11 - x) * (4.5 / 22)                       # the sloped top, high at the back
    m.hexa("PhoneBody", [(-11, -11, 0), (11, -11, 0), (11, 11, 0), (-11, 11, 0),
                         (-11, -11, 8), (11, -11, 3.5), (11, 11, 3.5), (-11, 11, 8)])
    def on_slope(mat, x0, x1, y0, y1, h):
        m.hexa(mat, [(x0, y0, z_at(x0)), (x1, y0, z_at(x1)), (x1, y1, z_at(x1)), (x0, y1, z_at(x0)),
                     (x0, y0, z_at(x0) + h), (x1, y0, z_at(x1) + h), (x1, y1, z_at(x1) + h), (x0, y1, z_at(x0) + h)])
    on_slope("PhoneBody", -10.5, -4, -10.8, -3.8, 4.2)                     # handset: earpiece
    on_slope("PhoneBody", -4, 4, -10, -4.6, 2.6)                            #          grip
    on_slope("PhoneBody", 4, 10.5, -10.8, -3.8, 4.2)                        #          mouthpiece
    for r in range(4):
        for c in range(3):
            x0 = 6.5 - r * 2.9; y0 = 0.5 + c * 2.9
            on_slope("Button", x0 - 2.1, x0, y0, y0 + 2.1, 0.7)
    on_slope("Lcd", -9.5, -4.5, 0.5, 8.6, 0.25)
    save("DeskPhone", m)


def crt_monitor():
    m = Mesh()
    m.cylinder("Beige", (-2, 0, 0), (0, 0, 1), 12, 2, segs=28)
    m.box("Beige", (-6, -5, 2), (4, 5, 6))
    m.box("Beige", (8, -20, 6), (21, 20, 40), 1.5)                          # front shell
    m.hexa("Beige", [(8, -18.5, 7.5), (-20, -11.5, 12), (-20, 11.5, 12), (8, 18.5, 7.5),
                     (8, -18.5, 38.5), (-20, -11.5, 33), (-20, 11.5, 33), (8, 18.5, 38.5)])
    for i in range(5):                                                      # vents on the back
        m.box("BeigeDark", (-14 + i * 4, -9, 33.6 - i * 0.6), (-12 + i * 4, 9, 34.2 - i * 0.6))
    m.box("Screen", (20.6, -16.5, 10.5), (21.3, 16.5, 36), 0.3)
    m.box("BeigeDark", (21, 14.5, 7.2), (21.6, 17, 8.6))                    # power button
    m.box("LedGreen", (21, 12, 7.5), (21.4, 13, 8.3))
    save("CRTMonitor", m)


def keyboard():
    m = Mesh()
    def z_at(x): return 1.5 + (8 - x) * (1.7 / 16)
    m.hexa("Beige", [(-8, -22.5, 0), (8, -22.5, 0), (8, 22.5, 0), (-8, 22.5, 0),
                     (-8, -22.5, z_at(-8)), (8, -22.5, z_at(8)), (8, 22.5, z_at(8)), (-8, 22.5, z_at(-8))])
    def key(x0, x1, y0, y1):
        m.hexa("KeyGrey", [(x0, y0, z_at(x0)), (x1, y0, z_at(x1)), (x1, y1, z_at(x1)), (x0, y1, z_at(x0)),
                           (x0, y0, z_at(x0) + 0.9), (x1, y0, z_at(x1) + 0.9), (x1, y1, z_at(x1) + 0.9), (x0, y1, z_at(x0) + 0.9)])
    for row in range(5):
        x1 = 6.5 - row * 2.75; x0 = x1 - 2.2
        if row == 0:
            for c in range(3): key(x0, x1, -21 + c * 2.75, -21 + c * 2.75 + 2.3)
            key(x0, x1, -12.5, 7.5)                                         # space bar
            for c in range(4): key(x0, x1, 9 + c * 2.75, 9 + c * 2.75 + 2.3)
        else:
            for c in range(15): key(x0, x1, -21 + c * 2.75, -21 + c * 2.75 + 2.3)
    save("Keyboard", m)


def office_chair():
    m = Mesh()
    for k in range(5):                                                      # the five-star base
        a = math.radians(k * 72 + 36)
        d = np.array([math.cos(a), math.sin(a), 0]); s = np.array([-d[1], d[0], 0])
        p0, p1 = d * 4, d * 29
        m.hexa("Black", [p0 - s * 2.4 + [0, 0, 8], p1 - s * 1.5 + [0, 0, 7], p1 + s * 1.5 + [0, 0, 7], p0 + s * 2.4 + [0, 0, 8],
                         p0 - s * 2.4 + [0, 0, 11], p1 - s * 1.5 + [0, 0, 9], p1 + s * 1.5 + [0, 0, 9], p0 + s * 2.4 + [0, 0, 11]])
        m.cylinder("Black", d * 30 + s * -2 + [0, 0, 2.6], s, 2.6, 4, segs=12)   # caster wheel
        m.box("Black", (d * 30 - 1.2 + [0, 0, 4])[:3], (d * 30 + 1.2 + [0, 0, 8])[:3])
    m.cylinder("Black", (0, 0, 6), (0, 0, 1), 5, 6, segs=16)
    m.cylinder("Black", (0, 0, 12), (0, 0, 1), 3.2, 18, segs=16)
    m.cylinder("Chrome", (0, 0, 30), (0, 0, 1), 2.4, 10, segs=16)
    m.box("Black", (-12, -12, 40), (12, 12, 43))
    m.box("Fabric", (-24, -24, 43), (24, 24, 51), 2.5)                      # seat
    m.box("Black", (-27, -3, 43), (-23, 3, 64))                             # back post
    back = rot_y(-8, (-26, 0, 58))                                          # leaning back a little
    m.box("Fabric", (-29, -22, 58), (-24, 22, 98), 2.5, xf=back)
    for sy in (-1, 1):
        y0, y1 = sorted((sy * 24, sy * 26.5))
        m.box("Black", (-6, y0, 45), (-2, y1, 62))
        y0, y1 = sorted((sy * 23.5, sy * 28))
        m.box("Black", (-14, y0, 62), (8, y1, 64.5), 1.0)
    save("OfficeChair", m)


def bench(name="Bench", length=180, depth=36, frames=(-70, 70), back=False):
    """slatted wooden seat on black steel frames; with back=True, a two-board backrest on the -X side"""
    m = Mesh()
    m.uv_scale["Wood"] = 1 / 90.0
    hl, hd = length / 2, depth / 2
    gap = 1.2; slat = (depth - 2 * gap) / 3
    for k in range(3):
        x0 = -hd + k * (slat + gap)
        m.box("Wood", (x0, -hl, 42), (x0 + slat, hl, 46), 0.5)
    for y in frames:
        for x0, x1 in ((-hd + 2, -hd + 6), (hd - 6, hd - 2)):
            m.box("Steel", (x0, y - 2, 0), (x1, y + 2, 42))
        m.box("Steel", (-hd, y - 2, 38.5), (hd, y + 2, 42))
        m.box("Steel", (-hd + 2, y - 1.5, 6), (hd - 2, y + 1.5, 9))
        if back: m.box("Steel", (-hd, y - 2, 42), (-hd + 4, y + 2, 88))
    m.box("Steel", (-1.5, -(hl - 18), 36), (1.5, hl - 18, 38.5))
    if back:
        for z0, z1 in ((62, 73), (76, 87)):
            m.box("Wood", (-hd, -hl, z0), (-hd + 4, hl, z1), 0.5)
    save(name, m)


# ---------------------------------------------------------------- Poly Haven (CC0): turn to face +X
POLYHAVEN = {"metal_office_desk": "Desk", "SchoolChair_01": "StackChair", "wall_clock": "WallClock",
             "television_02": "CRTTV", "desk_lamp_arm_01": "DeskLamp"}


def polyhaven():
    src_root = os.path.join(OUT, "PolyHaven")
    for pid, name in POLYHAVEN.items():
        src = os.path.join(src_root, pid)
        g = next((f for f in os.listdir(src) if f.endswith(".gltf")), None)
        if not g: print("missing", pid); continue
        doc = json.load(open(os.path.join(src, g)))
        # one mesh per prop: the desk ships as body + tray + six drawers, the clock as case + hands. Their nodes sit
        # at the origin untransformed, so their primitives can simply be gathered into one mesh.
        if len(doc["meshes"]) > 1:
            if any(k in n for n in doc["nodes"] for k in ("translation", "rotation", "scale", "matrix", "children")):
                print(f"{name}: parts are transformed - left as {len(doc['meshes'])} meshes")
            else:
                doc["meshes"] = [{"name": name, "primitives": [p for mesh in doc["meshes"] for p in mesh["primitives"]]}]
                doc["nodes"] = [{"name": name, "mesh": 0}]
                doc["scenes"] = [{"nodes": [0]}]; doc["scene"] = 0
        # glTF assets face +Z, which Unreal reads as +Y: a quarter turn about up makes them face +X like ours
        scene = doc["scenes"][doc.get("scene", 0)]
        doc["nodes"].append({"name": name, "children": scene["nodes"], "rotation": [0, math.sin(math.pi / 4), 0, math.cos(math.pi / 4)]})
        scene["nodes"] = [len(doc["nodes"]) - 1]
        for b in doc.get("buffers", []): b["uri"] = "../PolyHaven/" + pid + "/" + b["uri"]
        for im in doc.get("images", []): im["uri"] = "../PolyHaven/" + pid + "/" + im["uri"]
        folder = os.path.join(OUT, name); os.makedirs(folder, exist_ok=True)
        json.dump(doc, open(os.path.join(folder, name + ".gltf"), "w"), indent=1)
        print(f"{name}: Poly Haven {pid}, turned to face +X")


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    locker(); locker(open_door=True); filing_cabinet(); water_cooler()
    cork_board("CorkBoard", False); cork_board("CorkBoardNotices", True); cork_board("CorkBoardWide", False, 250, 150)
    desk_phone(); crt_monitor(); keyboard(); office_chair()
    bench()                                                                  # 180 cm, backless
    bench("BenchLong", 420, 40, (-180, 0, 180))                              # locker room
    bench("RollCallBench", 290, 42, (-130, 0, 130), back=True)              # roll call and the lobby
    polyhaven()
