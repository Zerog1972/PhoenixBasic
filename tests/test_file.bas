REM test_files.bas - Test des operations fichiers en BASIC
REM =====================================================
REM Teste OPEN, CLOSE, PRINT#, INPUT# en GFA Basic
REM Le fichier temporaire vit dans build/ (isole du depot)
REM et est nettoye en debut et en fin de test.

f$ = "build/test_bas.tmp"
KILL f$

REM Test 1: Ecriture dans un fichier
OPEN "O", #1, f$
PRINT #1, "Hello"
CLOSE #1
PRINT "OK: Fichier ecrit"

REM Test 2: Lecture du fichier
OPEN "I", #1, f$
INPUT #1, a$
CLOSE #1
PRINT "Lu: "; a$

REM Test 3: Verification
IF a$ = "Hello"
  PRINT "OK: a$ = Hello"
ELSE
  PRINT "ERREUR: a$ <> Hello"
ENDIF

REM Nettoyage
KILL f$

PRINT "FIN: test_files.bas OK"
END
