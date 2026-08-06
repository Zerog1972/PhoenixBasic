@echo off
set ROOT=%~dp0
if not exist "%ROOT%build\atari\GFABASIC.PRG" goto err_prg
if not exist "%ROOT%tools\hatari\hatari.exe" goto err_hatari
if not exist "%ROOT%tools\hatari\tos.img" goto err_tos
REM Creer l'image disquette si absente
if not exist "%ROOT%build\atari\GFABASIC.ST" (
    python "%ROOT%make_floppy.py" "%ROOT%build\atari\GFABASIC.PRG" "%ROOT%build\atari\GFABASIC.ST"
)
REM Lancer Hatari avec l'image disquette montee dans A: via --disk-a
"%ROOT%tools\hatari\hatari.exe" --tos "%ROOT%tools\hatari\tos.img" --disk-a "%ROOT%build\atari\GFABASIC.ST" --auto "A:\GFABASIC.PRG" --machine st --memsize 4
echo.
echo Hatari termine.
goto end
:err_prg
echo ERREUR: GFABASIC.PRG introuvable - lancez build_atari.bat
goto end
:err_hatari
echo ERREUR: Hatari introuvable - dezippez Hatari dans tools\hatari\
echo Telechargement : https://framagit.org/hatari/hatari/-/releases
goto end
:err_tos
echo ERREUR: ROM TOS introuvable - placez tos.img dans tools\hatari\
echo EmuTOS (ROM libre) : https://emutos.sourceforge.io/
:end