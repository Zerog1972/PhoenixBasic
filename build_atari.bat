@echo off
REM ================================================================
REM  build_atari.bat - Compilation croisee Atari ST (Windows)
REM ================================================================
REM  Installe MSYS2 + la toolchain m68k-atari-mintelf si necessaire,
REM  puis compile GFABASIC.PRG pour Atari ST / TOS.
REM ================================================================
setlocal

REM --- Chemins -------------------------------
set MSYS2_ROOT=C:\msys64
set BASH=%MSYS2_ROOT%\usr\bin\bash.exe
set MAKE=%MSYS2_ROOT%\usr\bin\make.exe
set CC=%MSYS2_ROOT%\mingw64\bin\m68k-atari-mintelf-gcc.exe

echo.
echo ================================================================
echo  GFA Basic 3.5 - Compilation Atari ST (m68k-atari-mintelf)
echo ================================================================
echo.

REM --- Verifier MSYS2 ------------------------
if not exist "%BASH%" (
    echo [1/3] MSYS2 n'est pas installe. Installation via winget...
    winget install --id MSYS2.MSYS2 --accept-source-agreements --accept-package-agreements --silent
    if errorlevel 1 (
        echo ERREUR: Impossible d'installer MSYS2.
        echo Installez-le manuellement : https://www.msys2.org/
        exit /b 1
    )
    echo MSYS2 installe. Veuillez relancer ce script.
    exit /b 0
)
echo [1/3] MSYS2 trouve : %MSYS2_ROOT%

REM --- Installer la toolchain croisee --------
if not exist "%CC%" (
    echo [2/3] Installation de la toolchain m68k-atari-mintelf ^(la 1ere fois^) ...
    "%BASH%" -lc "cd /c/Users/Thierry/Dev/PhoenixBasic && bash setup_msys2_toolchain.sh"
    if errorlevel 1 (
        echo ERREUR: Installation de la toolchain echouee.
        exit /b 1
    )
) else (
    echo [2/3] Toolchain m68k-atari-mintelf deja installee.
)

REM --- Compiler ------------------------------
echo [3/3] Compilation du .PRG...
"%BASH%" -lc "export PATH='/mingw64/bin:/usr/bin:$PATH'; cd '/c/Users/Thierry/Dev/PhoenixBasic' && make -f Makefile.atari clean && make -f Makefile.atari"

if errorlevel 1 (
    echo.
    echo ERREUR de compilation. Voir les messages ci-dessus.
    exit /b 1
)

echo.
echo ================================================================
echo  SUCCES : build\atari\GFABASIC.PRG
echo  Copiez ce fichier sur une disquette/CF puis lancez-le
echo  sur un Atari ST (ou dans Hatari/Steem).
echo ================================================================
endlocal