"""Makes the game's surface textures: original, seamless, 1024 x 1024, each a BaseColor (sRGB), a Normal (tangent space,
DirectX / Unreal convention) and a Roughness map. Everything is drawn from seeded noise, so running it again gives the
same files. Writes Content/Art/Textures/Source/T_<Name>_{BaseColor,Normal,Roughness}.png and prints each base colour's
average (linear) - the reference the game tints against (HUSurfaces.h).

Usage: AI\\python\\python.exe -I Tools\\Surfaces\\make_surfaces.py <project dir> [--preview <png>]
"""
import os, sys, json
import numpy as np
from PIL import Image

N = 1024
ROOT = sys.argv[1]
OUT = os.path.join(ROOT, "Content", "Art", "Textures", "Source")
PREVIEW = sys.argv[sys.argv.index("--preview") + 1] if "--preview" in sys.argv else None

# ---------- seamless building blocks (everything wraps at the edges) ----------
def fbm(seed, beta=2.0, lo=2, hi=None):
    """Fractal noise with a 1/f^beta spectrum - periodic, so it tiles. lo/hi: lowest/highest frequency kept."""
    r = np.random.default_rng(seed)
    fy = np.fft.fftfreq(N)[:, None] * N
    fx = np.fft.fftfreq(N)[None, :] * N
    f = np.sqrt(fx * fx + fy * fy)
    amp = np.where(f >= lo, 1.0 / np.maximum(f, 1e-6) ** (beta / 2), 0.0)
    if hi: amp *= f <= hi
    spec = amp * np.exp(2j * np.pi * r.random((N, N)))
    x = np.real(np.fft.ifft2(spec))
    return (x - x.min()) / (x.max() - x.min() + 1e-9)

def stretched(seed, sx, sy, beta=2.0):
    """Noise squashed along one axis (wood grain, fibres): frequencies scaled per axis."""
    r = np.random.default_rng(seed)
    fy = np.fft.fftfreq(N)[:, None] * N / sy
    fx = np.fft.fftfreq(N)[None, :] * N / sx
    f = np.sqrt(fx * fx + fy * fy)
    amp = np.where(f > 0.5, 1.0 / np.maximum(f, 1e-6) ** (beta / 2), 0.0)
    x = np.real(np.fft.ifft2(amp * np.exp(2j * np.pi * r.random((N, N)))))
    return (x - x.min()) / (x.max() - x.min() + 1e-9)

def cells(seed, grid):
    """Worley noise: one random point per cell of a grid x grid lattice (wrapping); returns (distance to the nearest
    point in cell widths, that point's id). Pebbles, stones, pores."""
    r = np.random.default_rng(seed)
    jit = r.random((grid, grid, 2))
    c = N / grid
    yy, xx = np.mgrid[0:N, 0:N].astype(np.float32)
    cy, cx = (yy // c).astype(int), (xx // c).astype(int)
    best = np.full((N, N), 1e9, np.float32); bid = np.zeros((N, N), np.int64)
    for oy in (-1, 0, 1):
        for ox in (-1, 0, 1):
            ny, nx = (cy + oy) % grid, (cx + ox) % grid
            py = (cy + oy + jit[ny, nx, 0]) * c; px = (cx + ox + jit[ny, nx, 1]) * c
            d = (yy - py) ** 2 + (xx - px) ** 2
            m = d < best
            best = np.where(m, d, best); bid = np.where(m, ny * grid + nx, bid)
    return np.sqrt(best) / c, bid

def blur(x, k):
    """Box blur that wraps (k pixels), three passes ~ Gaussian."""
    for _ in range(3):
        for ax in (0, 1):
            acc = np.zeros_like(x)
            for s in range(-k, k + 1): acc += np.roll(x, s, axis=ax)
            x = acc / (2 * k + 1)
    return x

def normal_from(height, strength):
    """Tangent-space normal map, DirectX convention (Unreal): green points down the image."""
    dx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * 0.5
    dy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * 0.5
    n = np.dstack([-dx * strength, -dy * strength, np.ones_like(height)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    return n

def mix(a, b, t):
    t = t[..., None] if np.ndim(t) == 2 else t
    return np.asarray(a) * (1 - t) + np.asarray(b) * t

def srgb(c): return np.asarray(c, np.float32) / 255.0

def save(name, base, height, strength, rough):
    os.makedirs(OUT, exist_ok=True)
    base = np.clip(base, 0, 1)
    Image.fromarray((base * 255 + 0.5).astype(np.uint8), "RGB").save(os.path.join(OUT, f"T_{name}_BaseColor.png"))
    n = normal_from(height, strength)
    Image.fromarray(((n * 0.5 + 0.5) * 255 + 0.5).astype(np.uint8), "RGB").save(os.path.join(OUT, f"T_{name}_Normal.png"))
    Image.fromarray((np.clip(rough, 0, 1) * 255 + 0.5).astype(np.uint8), "L").save(os.path.join(OUT, f"T_{name}_Roughness.png"))
    lin = np.where(base <= 0.04045, base / 12.92, ((base + 0.055) / 1.055) ** 2.4)
    avg = lin.reshape(-1, 3).mean(0)
    print(f"{name:13s} average linear colour {avg[0]:.3f} {avg[1]:.3f} {avg[2]:.3f}", flush=True)
    return name, avg, base, n

yy, xx = np.mgrid[0:N, 0:N].astype(np.float32)
made = []

# ---------- BRICK: running bond, 16 courses x 4 bricks, recessed mortar (tile ~110 cm) ----------
def brick():
    rows, per = 16, 4
    h, w = N / rows, N / per
    row = (yy // h).astype(int)
    off = np.where(row % 2 == 1, w / 2, 0)
    col = (((xx + off) % N) // w).astype(int)
    u = ((xx + off) % w) / w; v = (yy % h) / h
    r = np.random.default_rng(11)
    tone = r.random((rows, per))
    t = tone[row, col % per]
    edge = np.minimum(np.minimum(u, 1 - u) * w, np.minimum(v, 1 - v) * h)  # px to the brick's edge
    chip = fbm(12, 1.6, 8)
    mortar = edge < (5 + 4 * chip)
    face = fbm(13, 1.8, 16)
    speck = (fbm(14, 0.6, 64) > 0.72).astype(np.float32)
    col_b = mix(srgb([128, 52, 38]), srgb([168, 86, 58]), t) * (0.82 + 0.3 * face[..., None]) - 0.08 * speck[..., None]
    col_b = mix(col_b, srgb([60, 40, 34]), 0.25 * (fbm(15, 2.2, 3) > 0.7))  # soot patches
    col_m = srgb([172, 166, 152]) * (0.85 + 0.2 * fbm(16, 1.2, 32)[..., None])
    base = np.where(mortar[..., None], col_m, col_b)
    height = np.where(mortar, 0.0, np.clip(edge / 9, 0, 1)) + 0.15 * face
    rough = np.where(mortar, 0.95, 0.78 + 0.15 * face)
    return save("Brick", base, height, 6.0, rough)
made.append(brick())

# ---------- CONCRETE: poured, blotchy, pores (tile ~300 cm) ----------
def concrete():
    big, mid, fine = fbm(21, 2.4, 2), fbm(22, 1.8, 12), fbm(23, 0.8, 128)
    pores = cells(24, 30)[0] < 0.07
    base = srgb([150, 148, 141]) * (0.78 + 0.28 * big[..., None] + 0.12 * mid[..., None] + 0.06 * fine[..., None])
    base = np.where(pores[..., None], base * 0.55, base)
    height = 0.4 * mid + 0.3 * fine - 0.8 * pores
    rough = 0.78 + 0.15 * fine + 0.05 * big
    return save("Concrete", base, height, 3.0, rough)
made.append(concrete())

# ---------- ASPHALT: dark binder, pale aggregate stones, worn patches (tile ~300 cm) ----------
def asphalt():
    d, sid = cells(31, 56)
    shade = np.random.default_rng(32).random(56 * 56)[sid]
    agg = np.clip((0.42 - d) / 0.12, 0, 1)                      # a stone round each point, binder between
    wear, grit = fbm(33, 2.2, 2), fbm(34, 0.5, 200)
    base = srgb([40, 40, 42]) * (0.85 + 0.35 * wear[..., None] + 0.1 * grit[..., None])
    stone = mix(srgb([92, 90, 86]), srgb([150, 146, 138]), shade) * (0.85 + 0.3 * grit[..., None])
    base = mix(base, stone, 0.75 * agg * (shade > 0.35))         # some stones show, some are worn under binder
    height = 0.7 * agg + 0.2 * grit
    rough = 0.9 - 0.08 * agg + 0.05 * grit
    return save("Asphalt", base, height, 4.0, rough)
made.append(asphalt())

# ---------- TILE: white glazed ceramic, 8 x 8, grey grout (tile ~120 cm) ----------
def tile():
    k = 8; s = N / k
    u, v = (xx % s) / s, (yy % s) / s
    edge = np.minimum(np.minimum(u, 1 - u), np.minimum(v, 1 - v)) * s
    grout = edge < 4
    r = np.random.default_rng(41)
    t = r.random((k, k))[(yy // s).astype(int), (xx // s).astype(int)]
    glaze = fbm(42, 2.0, 4)
    base = srgb([226, 224, 216]) * (0.94 + 0.05 * t[..., None] + 0.04 * glaze[..., None])
    base = np.where(grout[..., None], srgb([128, 126, 120]) * (0.9 + 0.2 * fbm(43, 1.0, 64)[..., None]), base)
    bevel = np.clip((edge - 4) / 6, 0, 1)
    height = np.where(grout, 0.0, bevel) + 0.03 * glaze
    rough = np.where(grout, 0.92, 0.16 + 0.12 * fbm(44, 1.5, 8))
    return save("Tile", base, height, 5.0, rough)
made.append(tile())

# ---------- CARPET: old hotel carpet, burgundy with a gold diamond lattice, dense pile (tile ~150 cm) ----------
def carpet():
    p = N / 8
    a = ((xx + yy) % p) / p; b = ((xx - yy) % p) / p
    lattice = (np.abs(a - 0.5) < 0.06) | (np.abs(b - 0.5) < 0.06)
    dot = ((np.abs(a - 0.5) + np.abs(b - 0.5)) < 0.14) & ~lattice
    pile = fbm(51, 0.4, 256)
    wear = fbm(52, 2.2, 2)
    base = srgb([96, 22, 30]).copy() * np.ones((N, N, 3), np.float32)
    base = np.where(lattice[..., None], srgb([150, 112, 52]), base)
    base = np.where(dot[..., None], srgb([60, 70, 52]), base)
    base = base * (0.78 + 0.3 * pile[..., None]) * (0.85 + 0.2 * wear[..., None])
    height = 0.6 * pile + 0.2 * lattice
    rough = 0.93 + 0.05 * pile
    return save("Carpet", base, height, 2.5, rough)
made.append(carpet())

# ---------- WOOD FLOOR: oak planks, 8 rows, staggered joints, varnished (tile ~100 cm) ----------
def woodfloor():
    rows = 8; h = N / rows
    row = (yy // h).astype(int)
    r = np.random.default_rng(61)
    # each row: planks with joints at random places (wrapping)
    # two joints a row, at least a third of the tile apart: long boards, never stubs
    joints = []
    for _ in range(rows):
        a = r.integers(0, N); b = (a + r.integers(N // 3, 2 * N // 3)) % N
        joints.append(np.array([a, b]))
    plank = np.zeros((N, N), np.int32)
    for i in range(rows):
        js = np.asarray(joints[i]) % N
        m = row == i
        plank[m] = np.searchsorted(np.sort(js), xx[m]) % len(js) + i * 10
    tone = r.random(rows * 10 + 10)[plank]
    grain = stretched(62, 60.0, 1.0, 1.4)
    warp = stretched(63, 80.0, 3.0, 2.2)
    rings = 0.5 + 0.5 * np.sin((yy % h) * 0.35 + warp * 40.0 + plank * 1.7)   # flat-sawn figure along each board
    base = mix(srgb([122, 78, 44]), srgb([176, 124, 74]), tone) * (0.72 + 0.3 * grain[..., None] + 0.16 * rings[..., None])
    seam = (np.abs((yy % h) - 0) < 2) | (np.abs((yy % h) - h) < 2)
    for i in range(rows):
        for j in np.asarray(joints[i]) % N:
            seam |= (row == i) & (np.minimum(np.abs(xx - j), N - np.abs(xx - j)) < 2)
    base = np.where(seam[..., None], base * 0.35, base)
    height = np.where(seam, 0.0, 1.0) + 0.06 * grain
    rough = np.where(seam, 0.9, 0.42 + 0.15 * grain)
    return save("WoodFloor", base, height, 4.0, rough)
made.append(woodfloor())

# ---------- CEILING TILE: 2 x 2 fissured acoustic tiles in a metal T-bar grid (tile ~120 cm) ----------
def ceilingtile():
    s = N / 2
    u, v = (xx % s) / s, (yy % s) / s
    edge = np.minimum(np.minimum(u, 1 - u), np.minimum(v, 1 - v)) * s
    bar = edge < 7
    fiss = stretched(71, 6.0, 1.0, 1.0) * stretched(72, 1.0, 6.0, 1.0)
    holes = (fbm(73, 0.3, 300) > 0.8) | (fiss < 0.08)
    stain = fbm(74, 2.5, 2)
    base = srgb([226, 223, 212]) * (0.93 + 0.06 * fbm(75, 1.0, 64)[..., None]) * (1 - 0.12 * np.clip((stain[..., None] - 0.75) * 4, 0, 1))
    base = np.where(holes[..., None], base * 0.7, base)
    base = np.where(bar[..., None], srgb([205, 205, 200]), base)
    height = np.where(bar, 1.0, 0.6) - 0.15 * holes
    rough = np.where(bar, 0.45, 0.94)
    return save("CeilingTile", base, height, 3.0, rough)
made.append(ceilingtile())

# ---------- PAINTED METAL: light enamel over steel, orange peel, scratches (tile ~200 cm; tint it any colour) ----------
def paintedmetal():
    peel = fbm(81, 0.9, 64)
    r = np.random.default_rng(82)
    scratch = np.zeros((N, N), np.float32)
    for _ in range(40):
        x0, y0 = r.random(2) * N
        ang = r.random() * np.pi; ln = r.random() * 120 + 20
        t = np.linspace(0, 1, int(ln) * 2)
        px = ((x0 + np.cos(ang) * ln * t) % N).astype(int); py = ((y0 + np.sin(ang) * ln * t) % N).astype(int)
        scratch[py, px] = 1
    scratch = np.clip(blur(scratch, 1) * 3, 0, 1) * 0.6
    grime = fbm(83, 2.2, 3)
    base = srgb([206, 206, 200]) * (0.94 + 0.05 * peel[..., None]) * (0.9 + 0.1 * grime[..., None])
    base = mix(base, srgb([120, 122, 124]), 0.8 * scratch)
    height = 0.25 * peel - 0.5 * scratch
    rough = 0.42 + 0.1 * peel - 0.15 * scratch + 0.15 * (grime < 0.3)
    return save("PaintedMetal", base, height, 2.0, rough)
made.append(paintedmetal())

# ---------- GLASS: faint green-grey, dust and finger smudges, a slight old-glass waviness (tile ~150 cm) ----------
def glass():
    wave = fbm(91, 3.0, 1, 6)
    smudge = np.clip((fbm(92, 1.8, 6) - 0.55) * 3, 0, 1)
    dust = fbm(93, 0.6, 128)
    base = srgb([196, 212, 208]) * (0.97 + 0.03 * dust[..., None]) - 0.05 * smudge[..., None]
    height = 0.6 * wave
    rough = 0.04 + 0.18 * smudge + 0.04 * dust
    return save("Glass", base, height, 1.5, rough)
made.append(glass())

# ---------- FABRIC: a twill weave in warm grey (tile ~60 cm; tint it for upholstery, curtains, clothes) ----------
def fabric():
    p = 8.0
    warp = 0.5 + 0.5 * np.sin(2 * np.pi * xx / p)
    weft = 0.5 + 0.5 * np.sin(2 * np.pi * yy / p)
    twill = (((xx // p) + (yy // p)) % 4 < 2)
    weave = np.where(twill, warp * (0.6 + 0.4 * weft), weft * (0.6 + 0.4 * warp))
    slub = stretched(101, 30.0, 1.0, 1.2)
    fuzz = fbm(102, 0.4, 256)
    base = srgb([168, 158, 142]) * (0.8 + 0.18 * weave[..., None] + 0.06 * slub[..., None] + 0.04 * fuzz[..., None])
    height = 0.7 * weave + 0.15 * fuzz
    rough = 0.9 + 0.06 * fuzz
    return save("Fabric", base, height, 3.0, rough)
made.append(fabric())

# ---------- LEATHER: pebbled grain, creases, a soft sheen (tile ~80 cm) ----------
def leather():
    d, _ = cells(111, 90)
    grain = np.clip(d / 0.7, 0, 1)                              # 0 on a pebble's crown, 1 in the crevice between
    crease = 1 - np.abs(fbm(112, 2.4, 2, 12) * 2 - 1)           # a few long, soft folds
    crease = np.clip((crease - 0.94) * 12, 0, 1)
    tone = fbm(113, 2.4, 2)
    base = srgb([92, 56, 36]) * (0.8 + 0.25 * tone[..., None] - 0.12 * grain[..., None]) * (1 - 0.25 * crease[..., None])
    height = 0.6 * (1 - grain ** 2) - 0.6 * crease
    rough = 0.52 + 0.12 * (1 - grain) + 0.15 * crease
    return save("Leather", base, height, 3.0, rough)
made.append(leather())

refs = {name: [round(float(c), 4) for c in avg] for name, avg, _, _ in made}
json.dump(refs, open(os.path.join(OUT, "surface_averages.json"), "w"), indent=1)

# ---------- a preview sheet: each surface lit from the top-left, so the normal maps show ----------
if PREVIEW:
    tiles = []
    L = np.array([-0.5, -0.5, 0.7]); L /= np.linalg.norm(L)
    for name, avg, base, n in made:
        shade = np.clip((n @ L) * 1.1, 0, 1)[..., None]
        img = (np.clip(base * (0.35 + 0.75 * shade), 0, 1) * 255).astype(np.uint8)
        tiles.append(Image.fromarray(img).resize((340, 340)))
    sheet = Image.new("RGB", (4 * 344, ((len(tiles) + 3) // 4) * 344), (20, 20, 20))
    for i, t in enumerate(tiles): sheet.paste(t, ((i % 4) * 344, (i // 4) * 344))
    sheet.save(PREVIEW)
    print("preview:", PREVIEW)
