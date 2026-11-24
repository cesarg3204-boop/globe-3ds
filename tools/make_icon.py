#!/usr/bin/env python3
"""Draw the 48x48 Homebrew Launcher icon: a globe rendered from the Natural Earth country shapes.

Usage: python3 tools/make_icon.py ne_50m_admin_0_countries.geojson icon.png
"""
import json, math, sys
from PIL import Image, ImageDraw, ImageFilter

SRC, OUT = sys.argv[1], sys.argv[2]
S = 768                      # draw big, then shrink for smooth edges
LAT0, LON0 = 18.0, 15.0      # Europe / Africa facing the viewer
R = S * 0.46
CX = CY = S / 2
# heat colours from the game, so the icon hints at "warm = close"
HEAT = {"Algeria": (255, 140, 40), "Libya": (255, 64, 32), "Egypt": (200, 16, 32), "Chad": (255, 64, 32),
        "Niger": (255, 140, 40), "Sudan": (255, 140, 40), "Mali": (255, 210, 90), "Brazil": (70, 130, 210),
        "Russia": (170, 220, 235), "Kazakhstan": (170, 220, 235), "Spain": (255, 210, 90)}

def project(lon, lat):
    la, lo, la0, lo0 = map(math.radians, (lat, lon, LAT0, LON0))
    d = math.sin(la0) * math.sin(la) + math.cos(la0) * math.cos(la) * math.cos(lo - lo0)
    x = math.cos(la) * math.sin(lo - lo0)
    y = math.cos(la0) * math.sin(la) - math.sin(la0) * math.cos(la) * math.cos(lo - lo0)
    return CX + R * x, CY - R * y, d

img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
# glow and ocean
d.ellipse((CX - R - 14, CY - R - 14, CX + R + 14, CY + R + 14), fill=(90, 150, 255, 60))
d.ellipse((CX - R, CY - R, CX + R, CY + R), fill=(18, 44, 86, 255))
land = Image.new("RGBA", (S, S), (0, 0, 0, 0))
ld = ImageDraw.Draw(land)
for f in json.load(open(SRC))["features"]:
    name = f["properties"]["ADMIN"]
    g = f["geometry"]
    polys = g["coordinates"] if g["type"] == "MultiPolygon" else [g["coordinates"]]
    col = HEAT.get(name, (150, 160, 140))
    for poly in polys:
        ring = poly[0]
        pts = [project(lon, lat) for lon, lat in ring]
        if all(p[2] < 0 for p in pts):
            continue
        # clamp hidden points to the horizon so shapes crossing the edge stay closed
        out = []
        for x, y, z in pts:
            if z < 0:
                dx, dy = x - CX, y - CY
                n = math.hypot(dx, dy) or 1
                x, y = CX + dx / n * R, CY + dy / n * R
            out.append((x, y))
        if len(out) > 2:
            ld.polygon(out, fill=col + (255,), outline=(40, 40, 40, 255), width=2)
# keep land inside the disc
mask = Image.new("L", (S, S), 0)
ImageDraw.Draw(mask).ellipse((CX - R, CY - R, CX + R, CY + R), fill=255)
img.paste(land, (0, 0), Image.composite(land.getchannel("A"), Image.new("L", (S, S), 0), mask))
# soft shading: light from the upper left, dark towards the lower right edge
shade = Image.new("RGBA", (S, S), (0, 0, 0, 0))
sd = ImageDraw.Draw(shade)
for i in range(40):
    t = i / 40
    r = R * (1 - t * 0.9)
    off = R * 0.35 * t
    sd.ellipse((CX - r - off, CY - r - off, CX + r - off, CY + r - off), fill=(255, 255, 255, 3))
dark = Image.new("RGBA", (S, S), (0, 0, 0, 0))
ImageDraw.Draw(dark).ellipse((CX - R + R * 0.35, CY - R + R * 0.35, CX + R + R * 0.6, CY + R + R * 0.6), fill=(0, 0, 0, 90))
dark = dark.filter(ImageFilter.GaussianBlur(S / 12))
dark_masked = Image.new("RGBA", (S, S), (0, 0, 0, 0))
dark_masked.paste(dark, (0, 0), Image.composite(dark.getchannel("A"), Image.new("L", (S, S), 0), mask))
img.alpha_composite(shade)
img.alpha_composite(dark_masked)
img.resize((48, 48), Image.LANCZOS).save(OUT)
img.resize((256, 256), Image.LANCZOS).save(OUT.replace(".png", "_preview.png"))
