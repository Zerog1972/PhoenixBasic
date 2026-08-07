import struct, sys


def fat12_set(fat, idx, val):
    """Ecrit une entree FAT12 (12 bits)."""
    off = idx + (idx >> 1)
    if idx & 1:
        fat[off] = (fat[off] & 0x0F) | ((val & 0x0F) << 4)
        fat[off + 1] = (val >> 4) & 0xFF
    else:
        fat[off] = val & 0xFF
        fat[off + 1] = (fat[off + 1] & 0xF0) | ((val >> 8) & 0x0F)


def make_floppy(files, dst):
    """
    Cree une image disquette FAT12 720 Ko contenant plusieurs fichiers.

    files : liste de tuples (chemin_local, nom_8_3) — le nom est stocke
            tel quel sur la disquette (ex: "GFABASICPRG", "RUNNER.PRG"
            est converti en 8.3 "RUNNERPRG").
    """
    SECTORS = 1440       # 720 Ko
    SPC = 2              # secteurs/cluster (cluster = 1 Ko)
    BPC = SPC * 512
    FAT_SECTORS = 3
    ROOT_SECTORS = 7
    RESERVED = 1
    DATA_START = RESERVED + FAT_SECTORS * 2 + ROOT_SECTORS  # = 14
    MAX_CLUSTERS = 714   # (1440 - 14*2) / 2... laisse de la marge

    # Charger tous les fichiers
    entries = []
    for src, name83 in files:
        with open(src, "rb") as f:
            data = f.read()
        if len(data) == 0:
            raise ValueError("fichier vide: %s" % src)
        entries.append((name83, data))

    # Nombre de clusters total
    total_size = sum(len(d) for _, d in entries)
    nclusters = (total_size + BPC - 1) // BPC
    if nclusters <= 0:
        nclusters = 1
    if nclusters > MAX_CLUSTERS:
        raise ValueError("disquette pleine: %d clusters > %d" % (nclusters, MAX_CLUSTERS))

    img = bytearray([0xE5]) * (SECTORS * 512)

    # Boot sector FAT12
    img[0:3] = b"\xEB\xFE\x90"
    img[3:11] = b"GFABASIC"
    img[11:13] = struct.pack("<H", 512)
    img[13] = SPC
    img[14:16] = struct.pack("<H", RESERVED)
    img[16] = 2
    img[17:19] = struct.pack("<H", 112)
    img[19:21] = struct.pack("<H", SECTORS)
    img[21] = 0xF0
    img[22:24] = struct.pack("<H", FAT_SECTORS)
    img[24:26] = struct.pack("<H", 9)
    img[26:28] = struct.pack("<H", 2)
    img[54:62] = b"FAT12   "
    img[510:512] = b"\x55\xAA"

    # FAT : chaine de clusters (2 copies)
    fat = bytearray(FAT_SECTORS * 512)
    fat12_set(fat, 0, 0xFFF)
    fat12_set(fat, 1, 0xFFF)
    for i in range(nclusters):
        c = i + 2
        if c < nclusters + 1:
            fat12_set(fat, c, c + 1)
        else:
            fat12_set(fat, c, 0xFFF)
    img[512:512 + FAT_SECTORS * 512] = fat
    img[512 + FAT_SECTORS * 512:512 + 2 * FAT_SECTORS * 512] = fat

    # Root directory : un slot de 32 octets par fichier
    root = (RESERVED + FAT_SECTORS * 2) * 512
    cluster = 2
    for i, (name83, data) in enumerate(entries):
        e = bytearray(32)
        e[0:11] = name83.encode("ascii").ljust(11, b" ")
        e[11] = 0x20
        e[26:28] = struct.pack("<H", cluster)
        e[28:32] = struct.pack("<I", len(data))
        img[root + i * 32:root + i * 32 + 32] = e
        # Donnees : a partir du cluster courant
        data_off = (DATA_START + (cluster - 2) * SPC) * 512
        img[data_off:data_off + len(data)] = data
        cluster += (len(data) + BPC - 1) // BPC

    with open(dst, "wb") as f:
        f.write(img)
    print("OK", dst, len(img), "(", nclusters, "clusters,", total_size, "octets)")


def to_83(name):
    """Convertit un nom de fichier en entree 8.3 FAT (11 octets).

    Le format FAT stocke la base sur 8 octets (pad d'espaces) puis
    l'extension sur 3 octets. Exemple : "TEST.BAS" -> "TEST    BAS".
    """
    base, dot, ext = name.partition(".")
    base = base[:8].upper().ljust(8, " ")
    ext = ext[:3].upper().ljust(3, " ")
    return base + ext


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "--multi":
        # Usage : make_floppy.py --multi <image> <src1> <src2> ...
        # Le nom 8.3 est derive du nom de fichier local.
        dst = sys.argv[2]
        files = []
        for src in sys.argv[3:]:
            name = to_83(src.rsplit("/", 1)[-1].rsplit("\\", 1)[-1])
            files.append((src, name))
        make_floppy(files, dst)
    elif len(sys.argv) == 3:
        # Compatibilite : un seul fichier
        src = sys.argv[1]
        dst = sys.argv[2]
        name = to_83(src.rsplit("/", 1)[-1].rsplit("\\", 1)[-1])
        make_floppy([(src, name)], dst)
    else:
        print("Usage:")
        print("  make_floppy.py <fichier> <image>")
        print("  make_floppy.py --multi <image> <src1> <src2> ...")
        sys.exit(1)
