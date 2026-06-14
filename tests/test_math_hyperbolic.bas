' ============================================================
' Tests fonctions hyperboliques : SINH, COSH, TANH
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

PRINT "=== Tests SINH/COSH/TANH ==="
PRINT ""

' --- Test 1 : SINH(0) = 0 ---
check_num = 1
check_cond = (SINH(0) = 0)
GOSUB check

' --- Test 2 : COSH(0) = 1 ---
check_num = 2
check_cond = (COSH(0) = 1)
GOSUB check

' --- Test 3 : TANH(0) = 0 ---
check_num = 3
check_cond = (TANH(0) = 0)
GOSUB check

' --- Test 4 : SINH(1) > 1 ---
check_num = 4
check_cond = (SINH(1) > 1)
GOSUB check

' --- Test 5 : COSH(1) > 1 ---
check_num = 5
check_cond = (COSH(1) > 1)
GOSUB check

' --- Test 6 : TANH(1) < 1 ---
check_num = 6
check_cond = (TANH(1) < 1)
GOSUB check

' --- Test 7 : SINH(-1) = -SINH(1) (impaire) ---
check_num = 7
check_cond = (SINH(-1) + SINH(1) = 0)
GOSUB check

PRINT ""
PRINT "=== Resultats ==="
PRINT passed
PRINT " tests passes, "
PRINT failed
PRINT " tests echoues"
PRINT ""

IF failed > 0
  PRINT "*** CERTAINS TESTS ECHOUENT ***"
ELSE
  PRINT "*** TOUS LES TESTS ONT REUSSI ***"
ENDIF
PRINT ""

END
