#!/usr/bin/env python3
# tools/mk_kef.py
#
# Usage:
#   python3 tools/mk_kef.py <in.elf> <in.bin> <out.kef> <entry_rva> <bss_size>

import struct
import sys
import subprocess
import re
from typing import List

KEF_MAGIC = 0x3146454B  # "KEF1"


def _run(cmd: List[str]) -> str:
    return subprocess.check_output(cmd, text=True, errors="ignore")


def parse_reloc_offsets(elf_path: str) -> List[int]:
    """
    Extract R_386_32 relocation offsets via:
      objdump -r <elf>
    Requires linking the ELF with: -Wl,--emit-relocs
    """
    out = _run(["objdump", "-r", elf_path])
    relocs: List[int] = []
    for line in out.splitlines():
        m = re.match(r"^\s*([0-9a-fA-F]+)\s+R_386_32\b", line)
        if m:
            relocs.append(int(m.group(1), 16))
    # uniq + sorted for stability
    return sorted(set(relocs))


def main() -> int:
    if len(sys.argv) != 6:
        print("usage: mk_kef.py <in.elf> <in.bin> <out.kef> <entry_rva> <bss_size>")
        return 1

    in_elf = sys.argv[1]
    in_bin = sys.argv[2]
    out_kef = sys.argv[3]
    entry_rva = int(sys.argv[4], 0)
    bss_size = int(sys.argv[5], 0)

    with open(in_bin, "rb") as f:
        image = f.read()

    relocs = parse_reloc_offsets(in_elf)
    reloc_count = len(relocs)

    reloc_off = len(image)  # from image base
    reloc_blob = b"".join(struct.pack("<I", x) for x in relocs)

    # ------------------------------------------------------------
    # C side kef_header_t (32 bytes):
    #   uint32 magic
    #   uint16 version
    #   uint16 flags
    #   uint32 entry_rva
    #   uint32 image_size
    #   uint32 bss_size
    #   uint32 reserved0  (reloc_off)
    #   uint32 reserved1  (reloc_count)
    #
    # Python pack of these fields is 28 bytes, so we add 4-byte pad
    # to reach exactly 32 bytes total.
    # ------------------------------------------------------------
    header28 = struct.pack(
        "<IHHIIIII",
        KEF_MAGIC,
        1,
        0,
        entry_rva,
        len(image),
        bss_size,
        reloc_off,
        reloc_count,
    )
    if len(header28) != 28:
        raise RuntimeError(f"internal error: header28 size {len(header28)} != 28")

    header = header28 + struct.pack("<I", 0)  # padding to 32 bytes
    if len(header) != 32:
        raise RuntimeError(f"internal error: header size {len(header)} != 32")

    with open(out_kef, "wb") as f:
        f.write(header)
        f.write(image)
        f.write(reloc_blob)

    print(
        f"wrote {out_kef}: image={len(image)} entry={hex(entry_rva)} "
        f"bss={bss_size} relocs={reloc_count} reloc_off={reloc_off}"
    )
    if reloc_count:
        shown = ", ".join(hex(x) for x in relocs[:32])
        print("reloc offsets:", shown + (" ..." if reloc_count > 32 else ""))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())