' ============================================================
' Tests FOR / STEP — GFA Basic 3.5
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

PRINT "=== Tests FOR / STEP ==="
PRINT ""

' --- 1. FOR simple ---
s = 0
FOR i = 1 TO 5
  s = s + i
NEXT i
check_cond = s = 15
check_num = 1
GOSUB check

' --- 2. FOR avec STEP positif ---
s = 0
FOR i = 1 TO 5 STEP 2
  s = s + i
NEXT i
check_cond = s = 9
check_num = 2
GOSUB check

' --- 3. FOR nested ---
r = 0
FOR i = 1 TO 3
  FOR j = 1 TO 2
    r = r + 1
  NEXT j
NEXT i
check_cond = r = 6
check_num = 3
GOSUB check

' --- 4. FOR single iteration ---
cnt = 0
FOR i = 1 TO 1
  cnt = cnt + 1
NEXT i
check_cond = cnt = 1
check_num = 4
GOSUB check

' --- 5. FOR zero iteration ---
cnt = 0
FOR i = 1 TO 0
  cnt = cnt + 1
NEXT i
check_cond = cnt = 0
check_num = 5
GOSUB check

' --- 6. FOR avec pas=3 ---
s = 0
FOR i = 0 TO 9 STEP 3
  s = s + i
NEXT i
check_cond = s = 18
check_num = 6
GOSUB check

PRINT ""
PRINT "================================"
PRINT "Resultats : "
PRINT passed
PRINT " / "
PRINT passed + failed
PRINT " tests OK"
IF passed = 6
  PRINT "*** TOUS LES TESTS OK ***"
ELSE
  PRINT "*** ECHECS ***"
ENDIF
PRINT "=== Tests FOR/STEP termines ==="
END
