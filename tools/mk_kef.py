import struct, sys, subprocess, re

KEF_MAGIC = 0x3146454B  # "KEF1"

def parse_reloc_offsets(elf_path: str):
    # objdump -r çıktısından sadece R_386_32 reloc offsetlerini al
    # Örnek satır: 0000001a R_386_32  .rodata
    out = subprocess.check_output(["objdump", "-r", elf_path], text=True, errors="ignore")
    relocs = []
    for line in out.splitlines():
        m = re.match(r"^\s*([0-9a-fA-F]+)\s+R_386_32\b", line)
        if m:
            off = int(m.group(1), 16)
            relocs.append(off)
    return relocs

def main():
    # usage: mk_kef.py <in.elf> <in.bin> <out.kef> <entry_rva> <bss_size>
    if len(sys.argv) != 6:
        print("usage: mk_kef.py <in.elf> <in.bin> <out.kef> <entry_rva> <bss_size>")
        sys.exit(1)

    in_elf, in_bin, out_kef = sys.argv[1], sys.argv[2], sys.argv[3]
    entry_rva = int(sys.argv[4], 0)
    bss_size  = int(sys.argv[5], 0)

    image = open(in_bin, "rb").read()

    relocs = parse_reloc_offsets(in_elf)
    reloc_count = len(relocs)

    # reloc table, image'in hemen arkasına koyuyoruz
    reloc_off = len(image)
    reloc_blob = b"".join(struct.pack("<I", x) for x in relocs)

    # kef_header_t: <I H H I I I I I> (32 byte)
    header = struct.pack("<IHHIIIII",
        KEF_MAGIC,     # magic
        1,             # version
        0,             # flags
        entry_rva,     # entry_rva
        len(image),    # image_size (SADECE image)
        bss_size,      # bss_size
        reloc_off,     # reserved0 = reloc_table_off
        reloc_count    # reserved1 = reloc_count
    )

    open(out_kef, "wb").write(header + image + reloc_blob)
    print(f"wrote {out_kef}: image={len(image)} entry={hex(entry_rva)} bss={bss_size} relocs={reloc_count}")

if __name__ == "__main__":
    main()