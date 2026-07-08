# -*- coding: utf-8 -*-
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SKIP = {"Listings", "obj", "tools", "Download"}
fixed = []
for p in ROOT.rglob("*"):
    if p.suffix.lower() not in {".c", ".h", ".uvproj"}:
        continue
    if SKIP.intersection(p.parts):
        continue
    data = p.read_bytes()
    if data.startswith(b"\xef\xbb\xbf"):
        p.write_bytes(data[3:])
        fixed.append(str(p.relative_to(ROOT)))
print("fixed", len(fixed), "files")
for f in sorted(fixed):
    print(f)
