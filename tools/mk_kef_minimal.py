#!/usr/bin/env python3
import struct
from pathlib import Path

title = "Hello App".encode("utf-8")
text = "Merhaba KuvixOS".encode("utf-8")

window_w = 400
window_h = 220

hdr = struct.pack(
    "<4sIIIII",
    b"KEF0",
    1,
    window_w,
    window_h,
    len(title),
    len(text),
)

out = Path("apps_kef/hello/hello.kef")
out.parent.mkdir(parents=True, exist_ok=True)

with out.open("wb") as f:
    f.write(hdr)
    f.write(title)
    f.write(text)

print(f"Wrote {out}")