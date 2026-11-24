#!/usr/bin/env python3
"""Generate source/countrydata.c from Natural Earth 1:50m admin-0 countries (public domain).

Usage: python3 tools/gen_countries.py ne_50m_admin_0_countries.geojson source/countrydata.c
Needs: shapely, numpy, mapbox_earcut
"""
import json, sys, math, unicodedata
import numpy as np
from shapely.geometry import shape, Polygon, MultiPolygon
from shapely.ops import orient
import shapely
import mapbox_earcut as earcut

SRC, OUT = sys.argv[1], sys.argv[2]
R_EARTH = 6371.0
MAX_EDGE = 5.0  # degrees; long triangle edges are split so they follow the globe's curve

NAME_OVERRIDE = {
    "United States of America": "United States",
    "Russia": "Russia",
    "Republic of Serbia": "Serbia",
    "United Republic of Tanzania": "Tanzania",
    "The Bahamas": "Bahamas",
    "Macao S.A.R": "Macao",
    "Hong Kong S.A.R.": "Hong Kong",
    "eSwatini": "Eswatini",
    "Ivory Coast": "Cote d'Ivoire",
    "East Timor": "Timor-Leste",
    "Federated States of Micronesia": "Micronesia",
    "Aland": "Aland Islands",
    "São Tomé and Principe": "Sao Tome and Principe",
    "Curaçao": "Curacao",
    "Saint Barthelemy": "Saint Barthelemy",
    "Heard Island and McDonald Islands": "Heard and McDonald Islands",
    "French Southern and Antarctic Lands": "French Southern Lands",
}
EXTRA_ALIASES = {
    "United States": ["USA", "US", "America", "United States of America"],
    "United Kingdom": ["UK", "Great Britain", "Britain", "England", "Scotland", "Wales"],
    "United Arab Emirates": ["UAE", "Emirates"],
    "Democratic Republic of the Congo": ["DRC", "DR Congo", "Congo Kinshasa", "Zaire"],
    "Republic of the Congo": ["Congo", "Congo Brazzaville"],
    "Czechia": ["Czech Republic"],
    "Cote d'Ivoire": ["Ivory Coast"],
    "Timor-Leste": ["East Timor"],
    "Eswatini": ["Swaziland"],
    "Myanmar": ["Burma"],
    "North Macedonia": ["Macedonia"],
    "South Korea": ["Korea South", "Republic of Korea"],
    "North Korea": ["Korea North", "DPRK"],
    "Cabo Verde": ["Cape Verde"],
    "Vatican": ["Holy See", "Vatican City"],
    "Netherlands": ["Holland"],
    "Turkey": ["Turkiye"],
    "Laos": ["Lao"],
    "Bosnia and Herzegovina": ["Bosnia"],
    "Central African Republic": ["CAR"],
    "Falkland Islands": ["Malvinas"],
}
TARGET_EXTRA = {"Israel", "Palestine", "Kosovo", "Greenland", "Western Sahara", "Taiwan",
                "United States", "United Kingdom", "France", "Netherlands", "Denmark",
                "Finland", "Australia", "China", "New Zealand", "Kazakhstan", "Cuba"}
TARGET_EXCLUDE = {"Somaliland", "Northern Cyprus"}

def fold(s):
    s = unicodedata.normalize("NFKD", s)
    return "".join(c for c in s if not unicodedata.combining(c))

def key(s):
    return "".join(c for c in fold(s).lower() if c.isalpha())

feats = json.load(open(SRC))["features"]
countries = []
for f in feats:
    p = f["properties"]
    name = NAME_OVERRIDE.get(p["ADMIN"], p["ADMIN"])
    if name in ("United States", "Republic of the Congo", "Democratic Republic of the Congo"):
        pass
    elif p["NAME_LONG"] and len(p["NAME_LONG"]) < len(name) + 6 and p["ADMIN"] not in NAME_OVERRIDE:
        name = fold(p["NAME_LONG"])
    name = fold(name)
    if name == "Falkland Islands / Malvinas": name = "Falkland Islands"
    if name == "Russian Federation": name = "Russia"
    if name == "Lao PDR": name = "Laos"
    if name == "Brunei Darussalam": name = "Brunei"
    if name == "Republic of Korea": name = "South Korea"
    if name == "Dem. Rep. Korea": name = "North Korea"
    if name == "Czech Republic": name = "Czechia"
    if name == "The Gambia": name = "Gambia"
    if name == "Kingdom of eSwatini": name = "Eswatini"
    if name == "Republic of Cabo Verde": name = "Cabo Verde"
    if name == "Faeroe Islands": name = "Faroe Islands"
    if name == "Heard I. and McDonald Islands": name = "Heard and McDonald Islands"
    aliases = {name, fold(p["ADMIN"]), fold(p["NAME"]), fold(p["NAME_SORT"] or "")}
    if p.get("NAME_ALT"): aliases.add(fold(p["NAME_ALT"]))
    aliases |= set(EXTRA_ALIASES.get(name, []))
    keys = []
    for a in aliases:
        k = key(a)
        if len(k) >= 2 and k not in keys: keys.append(k)
    keys.sort(key=lambda k: (k != key(name), k))
    t = p["TYPE"]
    target = (t in ("Sovereign country", "Sovereignty") or name in TARGET_EXTRA) and name not in TARGET_EXCLUDE
    geom = shape(f["geometry"])
    if not geom.is_valid: geom = geom.buffer(0)
    countries.append(dict(name=name, keys=keys, geom=geom, shade=int(p["MAPCOLOR7"]), target=target,
                          sort=key(name)))

countries.sort(key=lambda c: c["sort"])
N = len(countries)
names = [c["name"] for c in countries]
assert len(set(names)) == N, [n for n in names if names.count(n) > 1]
print(N, "countries,", sum(c["target"] for c in countries), "targets", file=sys.stderr)

# ---------- distances (min border-to-border great-circle distance, km) ----------
def unitvec(lonlat):
    lon = np.radians(lonlat[:, 0]); lat = np.radians(lonlat[:, 1])
    return np.stack([np.cos(lat) * np.cos(lon), np.cos(lat) * np.sin(lon), np.sin(lat)], 1)

pts = []
for c in countries:
    g = c["geom"].simplify(0.02)
    if g.is_empty: g = c["geom"]
    g = shapely.segmentize(g.boundary, 0.2)
    coords = shapely.get_coordinates(g)
    pts.append(unitvec(coords).astype(np.float32))
offs = np.cumsum([0] + [len(p) for p in pts])
allp = np.concatenate(pts)
print("distance sample points:", len(allp), file=sys.stderr)
D = np.zeros((N, N), dtype=np.float64)
for i in range(N):
    best = np.full(len(allp), -2.0, dtype=np.float32)
    P = pts[i]
    for s in range(0, len(P), 256):
        dots = P[s:s + 256] @ allp.T
        np.maximum(best, dots.max(0), out=best)
    mx = np.maximum.reduceat(best, offs[:-1])
    D[i] = np.degrees(np.arccos(np.clip(mx, -1, 1))) * math.pi / 180 * R_EARTH
D = np.minimum(D, D.T)
# touching neighbours -> 0
for i in range(N):
    gi = countries[i]["geom"]
    for j in range(i + 1, N):
        if D[i, j] < 150 and gi.distance(countries[j]["geom"]) < 0.01:
            D[i, j] = D[j, i] = 0
np.fill_diagonal(D, 0)

# ---------- render meshes ----------
# Two levels of detail: a light one for the whole globe and a detailed one (with small islands) for close zoom.
LODS = [(0.12, 0.03), (0.03, 0.001)]  # (simplify tolerance in degrees, smallest island area kept in sq. degrees)

def parts(g):
    return list(g.geoms) if isinstance(g, MultiPolygon) else [g]

def triangulate(poly, verts, tris, vstart):
    poly = orient(poly, 1.0)  # CCW exterior in lon/lat == CCW seen from outside the globe
    poly = shapely.segmentize(poly, 4.0)
    rings = [np.asarray(poly.exterior.coords)[:-1]] + [np.asarray(r.coords)[:-1] for r in poly.interiors]
    flat = np.concatenate(rings)
    ends = np.cumsum([len(r) for r in rings]).astype(np.uint32)
    idx = earcut.triangulate_float64(flat, ends)
    base = len(verts) - vstart
    local = [(float(x), float(y)) for x, y in flat]
    mids = {}
    def mid(a, b):
        k = (min(a, b), max(a, b))
        if k not in mids:
            (x0, y0), (x1, y1) = local[a], local[b]
            local.append(((x0 + x1) / 2, (y0 + y1) / 2)); mids[k] = len(local) - 1
        return mids[k]
    def edge2(a, b):
        (x0, y0), (x1, y1) = local[a], local[b]
        return ((x1 - x0) * math.cos(math.radians((y0 + y1) / 2))) ** 2 + (y1 - y0) ** 2
    stack = [(int(idx[k]), int(idx[k + 1]), int(idx[k + 2])) for k in range(0, len(idx), 3)]
    while stack:
        a, b, c = stack.pop()
        if max(edge2(a, b), edge2(b, c), edge2(c, a)) <= MAX_EDGE ** 2:
            tris.append((base + a, base + b, base + c)); continue
        ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
        stack += [(a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca)]
    verts.extend(local)

def angle_deg(lat1, lon1, lat2, lon2):
    p1, p2, dl = math.radians(lat1), math.radians(lat2), math.radians(lon2 - lon1)
    d = math.sin(p1) * math.sin(p2) + math.cos(p1) * math.cos(p2) * math.cos(dl)
    return math.degrees(math.acos(max(-1.0, min(1.0, d))))

meshes = []
for tol, min_area in LODS:
    verts, tris, info = [], [], []
    for c in countries:
        g = c["geom"]
        ps = [p for p in parts(g.simplify(tol, preserve_topology=True)) if not p.is_empty]
        big = max(parts(g), key=lambda p: p.area)
        keep = [p for p in ps if p.area > min_area] or [big.simplify(0.005, preserve_topology=True)]
        vstart, tstart = len(verts), len(tris)
        for poly in keep:
            triangulate(poly, verts, tris, vstart)
        vcount, tcount = len(verts) - vstart, len(tris) - tstart
        assert vcount < 65535, (c['name'], vcount)
        info.append((vstart, vcount, tstart, tcount))
    meshes.append((verts, tris, info))
    print(f"LOD tol={tol}: vertices {len(verts)} triangles {len(tris)}", file=sys.stderr)

cinfo = []
for i, c in enumerate(countries):
    g = c["geom"]
    big = max(parts(g), key=lambda p: p.area)
    cen = g.representative_point() if g.area < 1 else big.centroid
    minx, miny, maxx, maxy = big.bounds
    extent = max(maxx - minx, maxy - miny)
    # radius of a cap around the label point that holds every vertex, so off-screen countries can be skipped
    fv, _, finfo = meshes[1]
    vs, vc = finfo[i][0], finfo[i][1]
    cap = max(angle_deg(cen.y, cen.x, y, x) for x, y in fv[vs:vs + vc])
    cinfo.append((cen.y, cen.x, extent, cap))

def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'

with open(OUT, "w") as o:
    o.write("// Generated by tools/gen_countries.py from Natural Earth 1:50m (public domain). Do not edit.\n")
    o.write('#include "countrydata.h"\n\n')
    o.write(f"const int COUNTRY_COUNT = {N};\n")
    o.write(f"const int VERTEX_COUNT[LOD_COUNT] = {{{len(meshes[0][0])}, {len(meshes[1][0])}}};\n")
    o.write(f"const int TRIANGLE_COUNT[LOD_COUNT] = {{{len(meshes[0][1])}, {len(meshes[1][1])}}};\n\n")
    o.write("const Country COUNTRIES[] = {\n")
    for i, (c, (la, lo, ex, cap)) in enumerate(zip(countries, cinfo)):
        l0, l1 = meshes[0][2][i], meshes[1][2][i]
        o.write(f"  {{{cstr(c['name'])}, {cstr('|'.join(c['keys']))}, {la:.3f}f, {lo:.3f}f, {ex:.2f}f, {cap:.2f}f, "
                f"{{{l0[0]}, {l1[0]}}}, {{{l0[1]}, {l1[1]}}}, {{{l0[2]}, {l1[2]}}}, {{{l0[3]}, {l1[3]}}}, "
                f"{c['shade']}, {int(c['target'])}}},\n")
    o.write("};\n\n")
    for lod, (verts, tris, _) in enumerate(meshes):
        o.write(f"static const short VERTS{lod}[] = {{\n")
        for k in range(0, len(verts), 8):
            o.write("  " + ",".join(f"{int(round(x*100))},{int(round(y*100))}" for x, y in verts[k:k+8]) + ",\n")
        o.write("};\n\n")
        o.write(f"static const unsigned short TRIS{lod}[] = {{\n")
        for k in range(0, len(tris), 10):
            o.write("  " + ",".join(f"{a},{b},{c}" for a, b, c in tris[k:k+10]) + ",\n")
        o.write("};\n\n")
    o.write("const short* const VERTS[LOD_COUNT] = { VERTS0, VERTS1 };\n")
    o.write("const unsigned short* const TRIS[LOD_COUNT] = { TRIS0, TRIS1 };\n\n")
    o.write("const unsigned short DIST_KM[] = {\n")
    for i in range(N):
        o.write("  " + ",".join(str(int(round(min(d, 65000)))) for d in D[i]) + ",\n")
    o.write("};\n")

# sanity checks
def d(a, b): return D[names.index(a), names.index(b)]
for a, b in [("France", "Germany"), ("France", "Brazil"), ("Spain", "Morocco"), ("Australia", "New Zealand"),
             ("United States", "Russia"), ("Chile", "Norway")]:
    print(f"{a}-{b}: {d(a,b):.0f} km", file=sys.stderr)
