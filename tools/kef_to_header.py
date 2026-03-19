#!/usr/bin/env python3
from pathlib import Path

src = Path("apps_kef/hello/hello.kef")
dst = Path("include/kernel/exec/kef_minimal_blob.h")

data = src.read_bytes()

with dst.open("w", encoding="utf-8") as f:
    f.write("#pragma once\n")
    f.write("#include <stdint.h>\n\n")
    f.write(f"static const uint32_t g_kef_hello_size = {len(data)};\n")
    f.write("static const uint8_t g_kef_hello_data[] = {")
    for i, b in enumerate(data):
        if i % 12 == 0:
            f.write("\n    ")
        f.write(f"0x{b:02X}, ")
    f.write("\n};\n")

print(f"Wrote {dst}")