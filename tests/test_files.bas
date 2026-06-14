REM test_files.bas - Test des operations fichiers en BASIC
REM =====================================================
REM Teste OPEN, CLOSE, PRINT#, INPUT# en GFA Basic

REM Test 1: Ecriture dans un fichier
OPEN "O", #1, "test_bas.tmp"
PRINT #1, "Hello"
CLOSE #1
PRINT "OK: Fichier ecrit"

REM Test 2: Lecture du fichier
OPEN "I", #1, "test_bas.tmp"
INPUT #1, a$
CLOSE #1
PRINT "Lu: "; a$

REM Test 3: Verification
IF a$ = "Hello"
  PRINT "OK: a$ = Hello"
ELSE
  PRINT "ERREUR: a$ <> Hello"
ENDIF

PRINT "FIN: test_files.bas OK"
END
