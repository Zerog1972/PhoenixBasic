REM test_onerror.bas - ON ERROR GOTO, ERROR, FATAL, RESUME NEXT, ERR
REM Verifie le pipeline complet de gestion d'erreur:
REM   - ERROR n        -> handler, ERR = n
REM   - division par 0 -> handler, ERR = 11
REM   - RESUME NEXT    -> reprend l'instruction suivante
REM   - ERR = 0        -> apres RESUME
REM   - FATAL n        -> bloque le handler, arret du programme
PRINT "--- ON ERROR / RESUME / FATAL ---"
x = 0

REM 1. ERROR explicite
ON ERROR GOTO err1
ERROR 7
PRINT "AUCUN1"
err1:
IF ERR = 7 THEN x = x + 1 ELSE PRINT "KO ERR1"

REM 2. Division par zero (code 11)
ON ERROR GOTO err2
y = 10 / 0
PRINT "AUCUN2"
err2:
IF ERR = 11 THEN x = x + 1 ELSE PRINT "KO ERR2"

REM 3. RESUME NEXT : reprend apres l'instruction fautive
ON ERROR GOTO err3
z = 100 / 0
w = 42
IF w = 42 THEN x = x + 1 ELSE PRINT "KO RESNEXT"
GOTO after3
err3:
RESUME NEXT
after3:

REM 4. ERR remise a zero apres RESUME
IF ERR = 0 THEN x = x + 1 ELSE PRINT "KO ERRCLR"

IF x = 4 THEN PRINT "TOUT OK ON ERROR" ELSE PRINT "ECHEC ON ERROR"

REM 5. FATAL : le handler ne doit PAS etre appele (arret)
ON ERROR GOTO err7
FATAL 99
PRINT "AUCUN7"
err7:
PRINT "KO FATAL"
END
