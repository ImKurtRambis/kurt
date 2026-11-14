"""A tiny mesh kit for building props in Unreal's space (X forward, Y right, Z up, centimetres).

Every primitive is convex, so each polygon is turned to face away from the primitive's centre - no winding bookkeeping.
UVs are planar per face (1 UV = 100 cm, or per material via Mesh.uv_scale), so tiling textures keep their size.
"""
import math
import numpy as np


def srgb(r, g=None, b=None, a=None):
    """sRGB 0..1 (or a #hex string) -> linear, as glTF wants"""
    if isinstance(r, str):
        h = r.lstrip("#"); r, g, b = (int(h[i:i + 2], 16) / 255 for i in (0, 2, 4))
    lin = [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in (r, g, b)]
    return tuple(lin) + ((a,) if a is not None else ())


def rot_y(deg, pivot):
    """pitch about Unreal's Y axis (tilts +X up for positive degrees) around pivot"""
    a = math.radians(deg); c, s = math.cos(a), math.sin(a); p = np.asarray(pivot, float)
    def f(v):
        d = np.asarray(v, float) - p
        return p + np.array([c * d[0] - s * d[2], d[1], s * d[0] + c * d[2]])
    return f


def rot_z(deg, pivot=(0, 0, 0)):
    a = math.radians(deg); c, s = math.cos(a), math.sin(a); p = np.asarray(pivot, float)
    def f(v):
        d = np.asarray(v, float) - p
        return p + np.array([c * d[0] - s * d[1], s * d[0] + c * d[1], d[2]])
    return f


class Mesh:
    def __init__(self):
        self.parts = {}
        self.uv_scale = {}

    def _part(self, mat):
        return self.parts.setdefault(mat, {"p": [], "n": [], "uv": [], "t": []})

    def poly(self, mat, pts, inside, normals=None, xf=None):
        """a convex planar polygon; `inside` is a point it must face away from"""
        pts = [np.asarray(xf(p) if xf else p, float) for p in pts]
        inside = np.asarray(xf(inside) if xf else inside, float)
        n = np.zeros(3)
        for i in range(len(pts)):  # Newell's method: robust for any convex polygon
            a, b = pts[i], pts[(i + 1) % len(pts)]
            n += np.array([(a[1] - b[1]) * (a[2] + b[2]), (a[2] - b[2]) * (a[0] + b[0]), (a[0] - b[0]) * (a[1] + b[1])])
        if np.linalg.norm(n) < 1e-9: return
        n /= np.linalg.norm(n)
        if np.dot(n, np.mean(pts, axis=0) - inside) < 0:
            pts = pts[::-1]; n = -n
            if normals is not None: normals = normals[::-1]
        if normals is not None:
            normals = [np.asarray(xf(np.asarray(q) + np.asarray(p0)) - xf(np.asarray(p0)) if xf else q, float) for q, p0 in zip(normals, [np.zeros(3)] * len(normals))]
        s = self.uv_scale.get(mat, 0.01)
        ax = int(np.argmax(np.abs(n)))
        u_ax, v_ax = [(1, 2), (0, 2), (0, 1)][ax]
        part = self._part(mat); base = len(part["p"])
        for i, p in enumerate(pts):
            part["p"].append(p)
            part["n"].append(normals[i] / np.linalg.norm(normals[i]) if normals is not None else n)
            part["uv"].append((p[u_ax] * s, -p[v_ax] * s))
        for i in range(1, len(pts) - 1):
            part["t"].append((base, base + i, base + i + 1))

    def hexa(self, mat, c8, xf=None):
        """8 corners: bottom ring b0..b3 then top ring t0..t3, same order"""
        c = [np.asarray(p, float) for p in c8]; mid = np.mean(c, axis=0)
        b, t = c[:4], c[4:]
        self.poly(mat, b, mid, xf=xf); self.poly(mat, t, mid, xf=xf)
        for i in range(4):
            j = (i + 1) % 4
            self.poly(mat, [b[i], b[j], t[j], t[i]], mid, xf=xf)

    def box(self, mat, mn, mx, chamfer=0.0, xf=None):
        mn, mx = np.asarray(mn, float), np.asarray(mx, float)
        if chamfer <= 0:
            x0, y0, z0 = mn; x1, y1, z1 = mx
            self.hexa(mat, [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], xf)
            return
        c = min(chamfer, *(0.49 * (mx - mn)))
        lo, hi = mn + c, mx - c
        mid = (mn + mx) / 2
        def pt(axis, side, a, b):  # a point on the face (axis, side) at in-plane coordinates a, b
            p = np.zeros(3); p[axis] = mx[axis] if side > 0 else mn[axis]
            o = [k for k in range(3) if k != axis]; p[o[0]] = a; p[o[1]] = b
            return p
        for axis in range(3):  # the six flat faces
            o = [k for k in range(3) if k != axis]
            for side in (-1, 1):
                self.poly(mat, [pt(axis, side, lo[o[0]], lo[o[1]]), pt(axis, side, hi[o[0]], lo[o[1]]),
                                pt(axis, side, hi[o[0]], hi[o[1]]), pt(axis, side, lo[o[0]], hi[o[1]])], mid, xf=xf)
        def inner(p, k, s):  # pull a corner of the outer box back to the chamfer line on axis k
            q = np.array(p, float); q[k] = hi[k] if s > 0 else lo[k]; return q
        for a in range(3):  # the twelve edge strips: along axis a, between faces on the other two axes
            b_, c_ = [k for k in range(3) if k != a]
            for sb in (-1, 1):
                for sc in (-1, 1):
                    def corner(ea):
                        p = np.zeros(3); p[a] = ea; p[b_] = mx[b_] if sb > 0 else mn[b_]; p[c_] = hi[c_] if sc > 0 else lo[c_]
                        q = np.zeros(3); q[a] = ea; q[b_] = hi[b_] if sb > 0 else lo[b_]; q[c_] = mx[c_] if sc > 0 else mn[c_]
                        return p, q
                    p0, q0 = corner(lo[a]); p1, q1 = corner(hi[a])
                    self.poly(mat, [p0, p1, q1, q0], mid, xf=xf)
        for sx in (-1, 1):  # the eight corner triangles
            for sy in (-1, 1):
                for sz in (-1, 1):
                    s = (sx, sy, sz); pts = []
                    for k in range(3):
                        p = np.array([hi[i] if s[i] > 0 else lo[i] for i in range(3)])
                        p[k] = mx[k] if s[k] > 0 else mn[k]
                        pts.append(p)
                    self.poly(mat, pts, mid, xf=xf)

    def cylinder(self, mat, base, axis, r0, length, r1=None, segs=20, caps=(True, True), smooth=True, xf=None):
        """a cylinder (or a frustum when r1 is given) from `base` along unit `axis`"""
        base = np.asarray(base, float); ax = np.asarray(axis, float); ax /= np.linalg.norm(ax)
        r1 = r0 if r1 is None else r1
        ref = np.array([0, 0, 1.0]) if abs(ax[2]) < 0.9 else np.array([1.0, 0, 0])
        u = np.cross(ax, ref); u /= np.linalg.norm(u); v = np.cross(ax, u)
        top = base + ax * length; mid = base + ax * length / 2
        ring = [(math.cos(2 * math.pi * i / segs), math.sin(2 * math.pi * i / segs)) for i in range(segs)]
        slope = (r0 - r1) / max(length, 1e-6)
        for i in range(segs):
            (ca, sa), (cb, sb) = ring[i], ring[(i + 1) % segs]
            da, db = u * ca + v * sa, u * cb + v * sb
            quad = [base + da * r0, base + db * r0, top + db * r1, top + da * r1]
            nrm = None
            if smooth:
                na, nb = da + ax * slope, db + ax * slope
                nrm = [na, nb, nb, na]
            self.poly(mat, quad, mid, normals=nrm, xf=xf)
        if caps[0] and r0 > 0: self.poly(mat, [base + (u * c + v * s) * r0 for c, s in ring], mid, xf=xf)
        if caps[1] and r1 > 0: self.poly(mat, [top + (u * c + v * s) * r1 for c, s in ring], mid, xf=xf)

    def arrays(self):
        out = {}
        for mat, p in self.parts.items():
            out[mat] = (np.array(p["p"]), np.array(p["n"]), np.array(p["uv"]), np.array(p["t"]))
        return out
