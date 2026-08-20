@echo off
REM ================================================================
REM  run_atari_test.bat - Simulation Hatari automatisee
REM ================================================================
REM  Lance RUNNER.PRG (launcher) qui execute GFABASIC.PRG TEST.BAS,
REM  capture la console VT52 (--conout 2) dans simulation_console.txt
REM  et le log Hatari dans hatari.log.
REM ================================================================
set ROOT=%~dp0

if not exist "%ROOT%build\atari\GFABASIC.PRG" goto err_prg
if not exist "%ROOT%build\atari\RUNNER.PRG" goto err_runner
if not exist "%ROOT%tools\hatari\hatari.exe" goto err_hatari
if not exist "%ROOT%tools\hatari\tos.img" goto err_tos

REM --- Creer l'image disquette multi-fichiers ---
REM Le launcher RUNNER.PRG cherche A:\TEST.BAS (8.3). On copie donc
REM le programme de test sous ce nom exact.
del "%ROOT%build\atari\GFABASIC.ST" 2>nul
copy /y "%ROOT%tests\test_atari.bas" "%ROOT%build\atari\TEST.BAS" >nul
python "%ROOT%make_floppy.py" --multi "%ROOT%build\atari\GFABASIC.ST" "%ROOT%build\atari\GFABASIC.PRG" "%ROOT%build\atari\RUNNER.PRG" "%ROOT%build\atari\TEST.BAS"
if errorlevel 1 goto err_img

REM --- Lancer Hatari (auto RUNNER.PRG, console VT52) ---
REM --benchmark : mode le plus rapide de Hatari (vitesse CPU maximale,
REM   sans synchronisation video). Le runtime GFA en C compile est gros
REM   et le mode cycle-exact du 68000 le rendait trop lent pour un test
REM   automatise.
REM --memsize 14 : RAM suffisante pour le heap mintlib (le gros BSS du
REM   programme + les allocations du runtime).
REM --sound off : l'emulation audio en --benchmark genere des warnings
REM   "system too slow" polluant le log (le test ne joue pas de son).
del "%ROOT%build\atari\hatari.log" 2>nul
del "%ROOT%build\atari\simulation_console.txt" 2>nul
"%ROOT%tools\hatari\hatari.exe" --tos "%ROOT%tools\hatari\tos.img" --disk-a "%ROOT%build\atari\GFABASIC.ST" --auto "A:\RUNNER.PRG" --machine st --memsize 14 --benchmark --sound off --log-file "%ROOT%build\atari\hatari.log" --log-level info --conout 2 --run-vbls 4000 > "%ROOT%build\atari\simulation_console.txt" 2>&1

echo.
echo ============================================================
echo  RESULTATS SIMULATION
echo ============================================================
if exist "%ROOT%build\atari\simulation_console.txt" (
    echo [CONSOLE] Atari ^(simulation_console.txt^)
    REM La console VT52 utilise des CR seuls qui perturbent "type" :
    REM on affiche via PowerShell (Get-Content gere toutes les fins).
    powershell -NoProfile -Command "Get-Content -LiteralPath '%ROOT%build\atari\simulation_console.txt'"
    echo [FIN] console
) else (
    echo Pas de capture console !
)
if exist "%ROOT%build\atari\hatari.log" (
    echo [DIAG] Hatari ^(erreurs/exceptions^)
    REM Le boot EmuTOS 1.4 genere un Bus Error connu au registre PSG
    REM ($ffffa200, PC en ROM $e00d98) : artefact du TOS, pas du programme.
    REM On l'exclut pour ne signaler que les vraies erreurs du runtime GFA.
    findstr /i "error bus exception panic illegal" "%ROOT%build\atari\hatari.log" | findstr /v "ffffa200"
    echo [FIN] diagnostics
)
goto end

:err_prg
echo ERREUR: GFABASIC.PRG introuvable - lancez build_atari.bat
goto end
:err_runner
echo ERREUR: RUNNER.PRG introuvable - recompilez tools\runner.c
goto end
:err_hatari
echo ERREUR: Hatari introuvable dans tools\hatari\
goto end
:err_tos
echo ERREUR: ROM TOS introuvable (tools\hatari\tos.img)
goto end
:err_img
echo ERREUR: creation image disquette impossible
goto end
:end