# Renders the draw-command log from sim.c into PNGs (top screen above bottom screen, like the console).
import sys
from PIL import Image, ImageDraw, ImageFont
log, outdir = sys.argv[1], sys.argv[2]
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
def col(u):
    u = int(u); return (u & 255, (u >> 8) & 255, (u >> 16) & 255, (u >> 24) & 255)
S = 2
img = None
for line in open(log):
    p = line.rstrip("\n").split(" ")
    if p[0] == "FRAME":
        name = p[1]; img = Image.new("RGBA", (400 * S, 480 * S), (0, 0, 0, 255)); off = 0
        layer = Image.new("RGBA", img.size, (0, 0, 0, 0)); d = ImageDraw.Draw(layer)
    elif p[0] == "END":
        img.alpha_composite(layer); img.convert("RGB").save(f"{outdir}/{name}.png")
    elif p[0] == "S":
        img.alpha_composite(layer); layer = Image.new("RGBA", img.size, (0, 0, 0, 0)); d = ImageDraw.Draw(layer)
        off = (0, 0) if p[1] == "0" else (40, 240)
    elif p[0] == "C":
        x0, y0 = (0, 0) if p[1] == "0" else (40, 240)
        if p[1] == "1":  # the top screen never draws past its own edge
            img.alpha_composite(layer); layer = Image.new("RGBA", img.size, (0, 0, 0, 0)); d = ImageDraw.Draw(layer)
            ImageDraw.Draw(img).rectangle([0, 240 * S, 400 * S, 480 * S], fill=(0, 0, 0, 255))
        ImageDraw.Draw(img).rectangle([x0 * S, y0 * S, (x0 + (400 if p[1] == "0" else 320)) * S - 1, (y0 + 240) * S - 1], fill=col(p[2]))
    else:
        def X(v): return (float(v) + off[0]) * S
        def Y(v): return (float(v) + off[1]) * S
        if p[0] == "P":
            cs = [col(v) for v in p[7:10]]  # average the vertex colours (the 3DS blends them smoothly)
            avg = tuple(sum(c[k] for c in cs) // 3 for k in range(4))
            d.polygon([(X(p[1]), Y(p[2])), (X(p[3]), Y(p[4])), (X(p[5]), Y(p[6]))], fill=avg)
        elif p[0] == "L":
            d.line([(X(p[1]), Y(p[2])), (X(p[3]), Y(p[4]))], fill=col(p[6]), width=max(1, int(float(p[5]) * S)))
        elif p[0] == "R":
            c = col(p[5]); img.alpha_composite(layer); layer = Image.new("RGBA", img.size, (0, 0, 0, 0)); d = ImageDraw.Draw(layer)
            ov = Image.new("RGBA", img.size, (0, 0, 0, 0)); ImageDraw.Draw(ov).rectangle([X(p[1]), Y(p[2]), X(float(p[1]) + float(p[3])) - 1, Y(float(p[2]) + float(p[4])) - 1], fill=c); img.alpha_composite(ov)
        elif p[0] == "E":
            img.alpha_composite(layer); layer = Image.new("RGBA", img.size, (0, 0, 0, 0)); d = ImageDraw.Draw(layer)
            ov = Image.new("RGBA", img.size, (0, 0, 0, 0)); ImageDraw.Draw(ov).ellipse([X(p[1]), Y(p[2]), X(float(p[1]) + float(p[3])), Y(float(p[2]) + float(p[4]))], fill=col(p[6]))
            img.alpha_composite(ov)
        elif p[0] == "T":
            x, y, sc, c, al = float(p[1]), float(p[2]), float(p[3]), col(p[4]), int(p[5]); s = " ".join(p[6:])
            f = ImageFont.truetype(FONT, int(26 * sc * S))
            img.alpha_composite(layer); layer = Image.new("RGBA", img.size, (0, 0, 0, 0)); d = ImageDraw.Draw(layer)
            w = ImageDraw.Draw(img).textlength(s, font=f)
            xx = X(x) - (w if al == 1 else w / 2 if al == 2 else 0)
            ImageDraw.Draw(img).text((xx, Y(y) + 2 * S), s, font=f, fill=c)
