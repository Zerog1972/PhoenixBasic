' ============================================================
' Tests : appel de PROCEDURE sans parentheses
' ============================================================

passed = 0
failed = 0
check_cond = 0
check_num = 0

PROCEDURE check
  IF check_cond
    passed = passed + 1
    PRINT "Test ";
    PRINT check_num;
    PRINT " : OK"
  ELSE
    failed = failed + 1
    PRINT "Test ";
    PRINT check_num;
    PRINT " : FAIL"
  ENDIF
RETURN

' --- Test 1 : appel avec 2 entiers ---
PROCEDURE somme(a, b)
  r = a + b
RETURN

somme 3, 7
check_num = 1
check_cond = (r = 10)
GOSUB check

' --- Test 2 : appel avec variables ---
x = 100
y = 200
somme x, y
check_num = 2
check_cond = (r = 300)
GOSUB check

' --- Test 3 : appel avec expressions ---
somme 3 * 4, 5 * 6
check_num = 3
check_cond = (r = 42)
GOSUB check

' --- Test 4 : appel avec chaine ---
PROCEDURE double_str(a$)
  r$ = a$ + a$
RETURN

double_str "Hello"
check_num = 4
check_cond = (r$ = "HelloHello")
GOSUB check

' --- Test 5 : appels imbriques ---
somme 5, 10
res = r
somme res, 100
check_num = 5
check_cond = (r = 115)
GOSUB check

' --- Test 6 : appel avec 3 arguments ---
PROCEDURE add3(a, b, c)
  r = a + b + c
RETURN

add3 1, 2, 3
check_num = 6
check_cond = (r = 6)
GOSUB check

PRINT ""
IF failed > 0
  PRINT "*** CERTAINS TESTS ECHOUENT ***"
ELSE
  PRINT "*** TOUS LES TESTS BARE CALL OK ***"
ENDIF
END
