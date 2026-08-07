# check_floppy.py
import struct, sys

def check(path):
    f = open(path, "rb").read()
    fat_sec = f[22] | (f[23] << 8)
    root = (1 + 2 * fat_sec) * 512
    e = f[root:root + 32]
    size = struct.unpack("<I", e[28:32])[0]
    cl = e[26] | (e[27] << 8)
    fat_off = 512 + fat_sec * 512
    buf = bytearray()
    steps = 0
    while steps < 2000:
        buf += f[(14 + (cl - 2) * 2) * 512:(14 + (cl - 2) * 2) * 512 + 1024]
        if len(buf) >= size: break
        off = cl + (cl >> 1)
        v = f[fat_off + off] | (f[fat_off + off + 1] << 8)
        v = (v >> 4) if (cl & 1) else (v & 0xFFF)
        if v < 2 or v >= 0xFF0:
            print("CHAIN BREAK at", cl, hex(v)); return 1
        cl = v; steps += 1
    buf = buf[:size]
    print("size", size, "clusters", steps + 1, "magic", hex(buf[0]), hex(buf[1]), "OK" if (buf[0] == 0x60 and buf[1] == 0x1A) else "BAD")
    return 0 if (buf[0] == 0x60 and buf[1] == 0x1A) else 1

sys.exit(check(sys.argv[1]))