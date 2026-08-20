@echo off
set ROOT=%~dp0
if not exist "%ROOT%build\atari\GFABASIC.PRG" goto err_prg
if not exist "%ROOT%tools\hatari\hatari.exe" goto err_hatari
if not exist "%ROOT%tools\hatari\tos.img" goto err_tos
REM Toujours regenerer l'image (le format 8.3 peut avoir change)
del "%ROOT%build\atari\GFABASIC.ST" 2>nul
python "%ROOT%make_floppy.py" "%ROOT%build\atari\GFABASIC.PRG" "%ROOT%build\atari\GFABASIC.ST"
if errorlevel 1 goto err_img
REM Lancer Hatari avec journalisation des exceptions (Bus Errors...)
del "%ROOT%build\atari\hatari.log" 2>nul
"%ROOT%tools\hatari\hatari.exe" --tos "%ROOT%tools\hatari\tos.img" --disk-a "%ROOT%build\atari\GFABASIC.ST" --auto "A:\GFABASIC.PRG" --machine st --memsize 4 --log-file "%ROOT%build\atari\hatari.log" --log-level debug --trace cpu_exception
echo.
echo Hatari termine.
if exist "%ROOT%build\atari\hatari.log" (
    echo --- Diagnostic Hatari ^(build\atari\hatari.log^) ---
    findstr /i "error bus exception panic" "%ROOT%build\atari\hatari.log"
    echo --- Fin du diagnostic ---
)
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
goto end
:err_img
echo ERREUR: impossible de creer l'image disquette GFABASIC.ST
goto end
:end