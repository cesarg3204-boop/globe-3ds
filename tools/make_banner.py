#!/usr/bin/env python3
"""Draw the HOME Menu banner (256x128) and its short jingle for the .cia.

Usage: python3 tools/make_banner.py ne_50m_admin_0_countries.geojson cia/
"""
import json, math, struct, sys, wave, random
from PIL import Image, ImageDraw, ImageFont, ImageFilter

SRC, OUTDIR = sys.argv[1], sys.argv[2].rstrip("/")
W, H, SS = 256, 128, 4
img = Image.new("RGBA", (W * SS, H * SS), (6, 9, 20, 255))
d = ImageDraw.Draw(img)
random.seed(3)
for _ in range(120):
    x, y = random.randrange(W * SS), random.randrange(H * SS)
    a = random.randrange(60, 230)
    d.ellipse((x, y, x + 4, y + 4), fill=(255, 255, 255, a))

# globe on the left, Americas facing the viewer for variety from the icon
R = 52 * SS
CX, CY = 62 * SS, 64 * SS
LAT0, LON0 = 12.0, -60.0
HEAT = {"Brazil": (255, 64, 32), "Peru": (255, 140, 40), "Bolivia": (200, 16, 32), "Colombia": (255, 140, 40),
        "Argentina": (255, 210, 90), "Paraguay": (255, 64, 32), "United States of America": (170, 220, 235),
        "Canada": (70, 130, 210), "Mexico": (170, 220, 235)}

def project(lon, lat):
    la, lo, la0, lo0 = map(math.radians, (lat, lon, LAT0, LON0))
    z = math.sin(la0) * math.sin(la) + math.cos(la0) * math.cos(la) * math.cos(lo - lo0)
    x = math.cos(la) * math.sin(lo - lo0)
    y = math.cos(la0) * math.sin(la) - math.sin(la0) * math.cos(la) * math.cos(lo - lo0)
    return CX + R * x, CY - R * y, z

d.ellipse((CX - R - 6, CY - R - 6, CX + R + 6, CY + R + 6), fill=(90, 150, 255, 35))
d.ellipse((CX - R, CY - R, CX + R, CY + R), fill=(18, 44, 86, 255))
land = Image.new("RGBA", img.size, (0, 0, 0, 0))
ld = ImageDraw.Draw(land)
for f in json.load(open(SRC))["features"]:
    g = f["geometry"]
    polys = g["coordinates"] if g["type"] == "MultiPolygon" else [g["coordinates"]]
    col = HEAT.get(f["properties"]["ADMIN"], (150, 160, 140))
    for poly in polys:
        pts = [project(lon, lat) for lon, lat in poly[0]]
        if all(p[2] < 0 for p in pts):
            continue
        out = []
        for x, y, z in pts:
            if z < 0:
                dx, dy = x - CX, y - CY
                n = math.hypot(dx, dy) or 1
                x, y = CX + dx / n * R, CY + dy / n * R
            out.append((x, y))
        if len(out) > 2:
            ld.polygon(out, fill=col + (255,), outline=(40, 40, 40, 255), width=2)
mask = Image.new("L", img.size, 0)
ImageDraw.Draw(mask).ellipse((CX - R, CY - R, CX + R, CY + R), fill=255)
img.paste(land, (0, 0), Image.composite(land.getchannel("A"), Image.new("L", img.size, 0), mask))
dark = Image.new("RGBA", img.size, (0, 0, 0, 0))
ImageDraw.Draw(dark).ellipse((CX - R * 0.6, CY - R * 0.6, CX + R * 1.6, CY + R * 1.6), fill=(0, 0, 0, 70))
dark = dark.filter(ImageFilter.GaussianBlur(R / 5))
img.paste(dark, (0, 0), Image.composite(dark.getchannel("A"), Image.new("L", img.size, 0), mask))

# title
bold = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
reg = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
d = ImageDraw.Draw(img)
d.text((124 * SS, 36 * SS), "Globe3DS", font=ImageFont.truetype(bold, 23 * SS), fill=(255, 138, 61, 255))
d.text((125 * SS, 70 * SS), "Find countries", font=ImageFont.truetype(reg, 11 * SS), fill=(232, 236, 244, 255))
d.text((125 * SS, 85 * SS), "on a spinning globe", font=ImageFont.truetype(reg, 11 * SS), fill=(140, 150, 170, 255))
img.resize((W, H), Image.LANCZOS).save(f"{OUTDIR}/banner.png")

# jingle: three rising chime notes, 16-bit mono PCM, under 3 seconds
RATE = 32000
notes = [(0.00, 523.25), (0.18, 659.25), (0.36, 783.99), (0.54, 1046.5)]
n = int(RATE * 1.8)
buf = [0.0] * n
for start, freq in notes:
    s0 = int(start * RATE)
    for i in range(s0, n):
        t = (i - s0) / RATE
        env = math.exp(-t * 3.2) * min(1.0, t * 200)
        buf[i] += env * (math.sin(2 * math.pi * freq * t) + 0.3 * math.sin(4 * math.pi * freq * t))
peak = max(abs(v) for v in buf) or 1
with wave.open(f"{OUTDIR}/audio.wav", "wb") as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
    w.writeframes(b"".join(struct.pack("<h", int(v / peak * 0.6 * 32767)) for v in buf))
