' GFA Basic 3.5 - Demo Complete
' Teste toutes les fonctionnalites implementees
' =============================================

PRINT "========================================"
PRINT " GFA Basic 3.5 - Demo Complete"
PRINT "========================================"
PRINT ""

' --- 1. PRINT et litteraux ---
PRINT "--- 1. PRINT et litteraux ---"
PRINT "Hello, World!"
PRINT 42
PRINT 3.14159
PRINT ""

' --- 2. Variables et assignations ---
PRINT "--- 2. Variables et assignations ---"
a = 10
b = 20
c = a + b
PRINT "10 + 20 = "
PRINT c
d = b - a
PRINT "20 - 10 = "
PRINT d
e = a * b
PRINT "10 * 20 = "
PRINT e
PRINT ""

' --- 3. Operateurs de comparaison ---
PRINT "--- 3. Comparaisons ---"
IF a < b
  PRINT "10 < 20 : OK"
ENDIF
IF b > a
  PRINT "20 > 10 : OK"
ENDIF
IF a = 10
  PRINT "10 = 10 : OK"
ENDIF
PRINT ""

' --- 4. Boucle FOR ---
PRINT "--- 4. Boucle FOR ---"
FOR i = 1 TO 5
  PRINT i
NEXT i
PRINT ""

' --- 5. Boucle FOR avec STEP ---
PRINT "--- 5. FOR avec STEP ---"
FOR i = 1 TO 10 STEP 2
  PRINT i
NEXT i
PRINT ""

' --- 6. Boucle WHILE ---
PRINT "--- 6. Boucle WHILE ---"
x = 1
WHILE x < 4
  PRINT x
  x = x + 1
WEND
PRINT ""

' --- 7. Boucle REPEAT ---
PRINT "--- 7. Boucle REPEAT ---"
x = 10
REPEAT
  PRINT x
  x = x - 3
UNTIL x < 5
PRINT ""

' --- 8. IF / ELSE / ENDIF ---
PRINT "--- 8. IF / ELSE / ENDIF ---"
valeur = 7
IF valeur > 5
  PRINT "> 5 : OK"
ELSE
  PRINT "<= 5 : NOK"
ENDIF
PRINT ""

' --- 9. GOTO ---
PRINT "--- 9. GOTO ---"
GOTO apres_goto
PRINT "Ceci ne doit PAS s'afficher"
apres_goto:
PRINT "GOTO : OK"
PRINT ""

' --- 10. GOSUB / RETURN ---
PRINT "--- 10. GOSUB / RETURN ---"
GOSUB sous_routine
PRINT "Retour du GOSUB : OK"
GOTO fin_gosub_test

sous_routine:
PRINT "Dans le sous-programme"
RETURN

fin_gosub_test:
PRINT ""

' --- 11. PROCEDURE ---
PRINT "--- 11. PROCEDURE ---"
GOSUB ma_procedure
PRINT "Retour procedure : OK"
GOTO apres_proc

ma_procedure:
PRINT "Dans la procedure"
RETURN

apres_proc:
PRINT ""

' --- 12. Fonctions mathematiques ---
PRINT "--- 12. Fonctions mathematiques ---"
PRINT "ABS(-5) = "
PRINT ABS(-5)
PRINT "SQR(16) = "
PRINT SQR(16)
PRINT "SIN(1) = "
PRINT SIN(1)
PRINT "COS(0) = "
PRINT COS(0)
PRINT "EXP(1) = "
PRINT EXP(1)
PRINT "LOG(2.718) = "
PRINT LOG(2.718)
PRINT "INT(3.7) = "
PRINT INT(3.7)
PRINT "ROUND(3.4) = "
PRINT ROUND(3.4)
PRINT "MIN(5,3) = "
PRINT MIN(5,3)
PRINT "MAX(5,3) = "
PRINT MAX(5,3)
PRINT "FACT(5) = "
PRINT FACT(5)
PRINT ""

' --- 13. Fonctions chaines ---
PRINT "--- 13. Fonctions chaines ---"
PRINT "LEN('hello') = "
PRINT LEN("hello")
PRINT "LEFT('hello',2) = "
PRINT LEFT("hello",2)
PRINT "RIGHT('hello',2) = "
PRINT RIGHT("hello",2)
PRINT "MID('hello',2,3) = "
PRINT MID("hello",2,3)
PRINT "UPPER('Hello') = "
PRINT UPPER("Hello")
PRINT "LCASE('Hello') = "
PRINT LCASE("Hello")
PRINT "TRIM('  hi  ') = "
PRINT TRIM("  hi  ")
PRINT "ASC('A') = "
PRINT ASC("A")
PRINT "CHR(66) = "
PRINT CHR(66)
PRINT "STR(42) = "
PRINT STR(42)
PRINT "VAL('123') = "
PRINT VAL("123")
PRINT "SPACE(3)+'x' = "
PRINT SPACE(3) + "x"
PRINT ""

' --- 14. Constantes ---
PRINT "--- 14. Constantes ---"
PRINT "PI = "
PRINT PI
PRINT "TRUE = "
PRINT TRUE
PRINT "FALSE = "
PRINT FALSE
PRINT ""

' --- 15. DATA / READ / RESTORE ---
PRINT "--- 15. DATA / READ / RESTORE ---"
DATA 100,200,300
READ r1
PRINT "READ 1: "
PRINT r1
READ r2
PRINT "READ 2: "
PRINT r2
RESTORE
READ r3
PRINT "RESTORE + READ: "
PRINT r3
PRINT ""

' --- 16. INPUT ---
PRINT "--- 16. INPUT ---"
PRINT "Test INPUT termine."

' --- 17. DIM (tableaux) ---
PRINT "--- 17. DIM (tableaux) ---"
DIM tableau(5)
PRINT "Tableau cree : OK"
PRINT ""

' --- 18. BEEP et SOUND ---
PRINT "--- 18. BEEP et SOUND ---"
BEEP
PRINT "BEEP : OK"
PRINT ""

' --- 19. Conversions ---
PRINT "--- 19. Conversions ---"
PRINT "BIN(10) = "
PRINT BIN(10)
PRINT "HEX(255) = "
PRINT HEX(255)
PRINT "DEG(PI) = "
PRINT DEG(PI)
PRINT "RAD(180) = "
PRINT RAD(180)
PRINT ""

' --- 20. SELECT / CASE ---
PRINT "--- 20. SELECT / CASE ---"
selection = 2
SELECT selection
CASE 1
  PRINT "CASE 1"
CASE 2
  PRINT "CASE 2 : OK"
CASE 3
  PRINT "CASE 3"
DEFAULT
  PRINT "DEFAULT"
ENDSELECT
PRINT ""

' --- 21. Boucle WHILE avec variable ---
PRINT "--- 21. Operations dans boucle ---"
cpt = 0
WHILE cpt < 3
  cpt = cpt + 1
  PRINT cpt
WEND
PRINT ""

' --- 22. Expressions complexes ---
PRINT "--- 22. Expressions complexes ---"
r = (10 + 5) * 2 - 3
PRINT "(10+5)*2-3 = "
PRINT r
PRINT ""

PRINT "========================================"
PRINT " Demo terminee avec succes !"
PRINT "========================================"
END
