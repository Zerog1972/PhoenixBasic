' ============================================================
' Tests FOR/NEXT, WHILE/WEND, REPEAT/UNTIL, DO/LOOP, SELECT/CASE
' GFA Basic 3.5
' ============================================================

passed = 0
failed = 0
check_cond = 0
check_num = 0

PROCEDURE check
  IF check_cond
    passed = passed + 1
    PRINT "Test "
    PRINT check_num
    PRINT " : OK"
  ELSE
    failed = failed + 1
    PRINT "Test "
    PRINT check_num
    PRINT " : FAIL"
  ENDIF
RETURN

PRINT ""
PRINT "=== Tests Flow Control ==="
PRINT ""

' ============================================================
' 1. FOR/NEXT simple (comptage croissant)
' ============================================================
sum = 0
FOR i = 1 TO 5
  sum = sum + i
NEXT i
check_cond = sum = 15
check_num = 1
GOSUB check

' ============================================================
' 2. FOR/NEXT avec STEP=2
' ============================================================
sum = 0
FOR i = 2 TO 6 STEP 2
  sum = sum + i
NEXT i
check_cond = sum = 12
check_num = 2
GOSUB check

' ============================================================
' 3. FOR/NEXT avec TO 1 (un seul passage)
' ============================================================
count = 0
FOR i = 1 TO 1
  count = count + 1
NEXT i
check_cond = count = 1
check_num = 3
GOSUB check

' ============================================================
' 4. FOR/NEXT sans iteration (depart > arrivee, pas positif)
' ============================================================
count = 0
FOR i = 5 TO 1
  count = count + 1
NEXT i
check_cond = count = 0
check_num = 4
GOSUB check

' ============================================================
' 5. WHILE/WEND (condition vraie)
' ============================================================
i = 1
sum = 0
WHILE i <= 5
  sum = sum + i
  i = i + 1
WEND
check_cond = sum = 15
check_num = 5
GOSUB check

' ============================================================
' 6. WHILE/WEND (condition fausse des le depart)
' ============================================================
i = 10
count = 0
WHILE i < 5
  count = count + 1
  i = i + 1
WEND
check_cond = count = 0
check_num = 6
GOSUB check

' ============================================================
' 7. REPEAT/UNTIL (s'execute au moins une fois)
' ============================================================
i = 1
sum = 0
REPEAT
  sum = sum + i
  i = i + 1
UNTIL i > 5
check_cond = sum = 15
check_num = 7
GOSUB check

' ============================================================
' 8. REPEAT/UNTIL (condition immediate vraie)
' ============================================================
count = 0
REPEAT
  count = count + 1
UNTIL count = 1
check_cond = count = 1
check_num = 8
GOSUB check

' ============================================================
' 9. SELECT/CASE avec cas exact
' ============================================================
result = 0
x = 2
SELECT x
  CASE 1
    result = 10
  CASE 2
    result = 20
  CASE 3
    result = 30
ENDSELECT
check_cond = result = 20
check_num = 9
GOSUB check

' ============================================================
' 10. SELECT/CASE avec DEFAULT
' ============================================================
result = 0
x = 99
SELECT x
  CASE 1
    result = 10
  CASE 2
    result = 20
  DEFAULT
    result = 99
ENDSELECT
check_cond = result = 99
check_num = 10
GOSUB check

' ============================================================
' Resume
' ============================================================
PRINT ""
PRINT "================================"
PRINT "Resultats : "
PRINT passed
PRINT " / "
PRINT passed + failed
PRINT " tests OK"

IF failed > 0
  PRINT "*** ECHECS : "
  PRINT failed
  PRINT " ***"
ELSE
  PRINT "*** TOUS LES TESTS OK ***"
ENDIF

END
