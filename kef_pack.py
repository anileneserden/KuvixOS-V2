import struct
import sys

def pack_kef(bin_file, out_file):
    try:
        with open(bin_file, "rb") as f:
            code = f.read()

        # KEF-v3 Header (7 x uint32_t)
        magic = 0x5633454B  # "KEV3"
        version = 3
        entry = 0
        text_size = len(code)
        data_size = 0
        heap_size = 4096
        checksum = sum(code) & 0xFFFFFFFF

        header = struct.pack("<IIIIIII", magic, version, entry, text_size, data_size, heap_size, checksum)

        with open(out_file, "wb") as f:
            f.write(header)
            f.write(code)
        
        print(f"[Packer] {out_file} basariyla olusturuldu ({len(code)} byte kod)")
    except Exception as e:
        print(f"[Packer] Hata: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Kullanim: python3 kef_pack.py input.bin output.kef")
    else:
        pack_kef(sys.argv[1], sys.argv[2])