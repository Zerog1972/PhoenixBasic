import os, struct, sys

def make_floppy(src, dst):
    with open(src, "rb") as f:
        p = f.read()
    size = len(p)
    img = bytearray([0xE5]) * 737280
    # Boot sector
    img[0:3] = b"\xEB\xFE\x90"
    img[3:11] = b"GFABASIC"
    img[11:13] = struct.pack("<H", 512)
    img[13] = 2
    img[14:16] = struct.pack("<H", 1)
    img[16] = 2
    img[17:19] = struct.pack("<H", 112)
    img[19:21] = struct.pack("<H", 1440)
    img[21] = 0xF0
    img[22:24] = struct.pack("<H", 3)
    img[24:26] = struct.pack("<H", 9)
    img[26:28] = struct.pack("<H", 2)
    img[54:62] = b"FAT12   "
    img[510:512] = b"\x55\xAA"
    # FAT (cluster 2 = EOF, fichier au cluster 2)
    fat = bytearray(3 * 512)
    fat[0:3] = b"\xF0\xFF\xFF"
    fat[3:6] = b"\xFF\xFF\xFF"
    img[512:2048] = fat + fat
    # Root dir entry
    root = 7 * 512
    e = bytearray(32)
    e[0:11] = b"GFABASIC  PRG"
    e[11] = 0x20
    e[26:28] = struct.pack("<H", 2)
    e[28:32] = struct.pack("<I", size)
    img[root:root+32] = e
    # Data (cluster 2 = secteur 14)
    img[14*512:14*512+size] = p
    with open(dst, "wb") as f:
        f.write(img)
    print("OK", dst, len(img))

if __name__ == "__main__":
    make_floppy(sys.argv[1], sys.argv[2])