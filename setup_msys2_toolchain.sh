#!/bin/bash
# setup_msys2_toolchain.sh - Installe la toolchain m68k-atari-mintelf dans MSYS2
# ==============================================================================
# Telecharge les composants depuis tho-otto.de/crossmint.php (archives mingw64)
# et les deploie dans MSYS2.
#
# IMPORTANT : extraire les archives a la RACINE / (elles ont un prefixe mingw64/)
#
# Usage : bash setup_msys2_toolchain.sh
# ==============================================================================

set -e

BASE_URL="https://tho-otto.de/download/mint"
TMPDIR="/tmp/crossmint"

echo "=== Telechargement de la page des packages ==="
curl -sL --max-time 90 "https://tho-otto.de/crossmint.php" -o /tmp/crossmint_page.html
grep -oE 'download/mint/[a-zA-Z0-9._-]+\.tar\.xz' /tmp/crossmint_page.html | sort -u > /tmp/crossmint_links.txt
echo "Total archives : $(wc -l < /tmp/crossmint_links.txt)"

BINUTILS=$(grep -oE 'binutils-[a-zA-Z0-9._-]+-bin-mingw64\.tar\.xz' /tmp/crossmint_links.txt | tail -1 || true)
GCC=$(grep -oE 'gcc-[a-zA-Z0-9._-]+-bin-mingw64\.tar\.xz' /tmp/crossmint_links.txt | tail -1 || true)
MINTLIB=$(grep -oiE 'mintlib-[a-zA-Z0-9._-]+-dev\.tar\.xz' /tmp/crossmint_links.txt | tail -1 || true)
FDLIBM=$(grep -oiE 'fdlibm-[a-zA-Z0-9._-]+-dev\.tar\.xz' /tmp/crossmint_links.txt | tail -1 || true)

echo "binutils=$BINUTILS"
echo "gcc=$GCC"
echo "mintlib=$MINTLIB"
echo "fdlibm=$FDLIBM"

if [ -z "$BINUTILS" ] || [ -z "$GCC" ] || [ -z "$MINTLIB" ] || [ -z "$FDLIBM" ]; then
    echo "ERREUR : composants introuvables"
    exit 1
fi

mkdir -p "$TMPDIR"

download_and_extract() {
    local name="$1"
    echo "=== Telechargement : $name ==="
    curl -sL --max-time 600 "$BASE_URL/$name" -o "$TMPDIR/$name"
    echo "Extraction a la racine / (prefixe interne) ..."
    tar -C / -xJf "$TMPDIR/$name"
    echo "OK : $name"
}

download_and_extract "$BINUTILS"
download_and_extract "$GCC"
download_and_extract "$MINTLIB"
download_and_extract "$FDLIBM"

echo "=== Fusion sysroot mintlib -> /mingw64/m68k-atari-mintelf/sys-root/ ==="
if [ -d "/usr/m68k-atari-mintelf/sys-root" ]; then
    mkdir -p /mingw64/m68k-atari-mintelf/sys-root/
    cp -rn /usr/m68k-atari-mintelf/sys-root/* /mingw64/m68k-atari-mintelf/sys-root/ 2>/dev/null || true
    echo "Sysroot copie."
fi

echo ""
echo "====================================================="
echo " Toolchain m68k-atari-mintelf installee"
echo "====================================================="
ls -la /mingw64/bin/m68k-atari-mintelf-gcc.exe 2>/dev/null || echo "WARNING: gcc introuvable"
ls /mingw64/m68k-atari-mintelf/sys-root/usr/include/stdio.h 2>/dev/null || echo "WARNING: stdio.h introuvable"