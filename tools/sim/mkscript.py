import sys
rows = ["QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"]
def keypos(ch):
    for r, row in enumerate(rows):
        i = row.find(ch.upper())
        if i >= 0: return (0 if r == 0 else 16) + i * 32 + 16, 127 + r * 32
out = []
def typ(s):
    for ch in s:
        x, y = keypos(ch); out.append(f"touch {x} {y}"); out.append("wait 2")
for line in open(sys.argv[1]):
    line = line.strip()
    if line.startswith("type "): typ(line[5:])
    elif line: out.append(line)
print("\n".join(out))
