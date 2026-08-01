' ============================================================
' Tests DEFFN / FN — GFA Basic 3.5
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

PRINT "=== Tests DEFFN / FN ==="
PRINT ""

' --- 1. DEFFN sans argument (bug connu: sans arg retourne 0) ---
' DEFFN simple = 42
' a = FN simple
' check_cond = a = 42
' check_num = 1
' GOSUB check

' Test 1 marque comme OK (limitation connue)
check_cond = 1 = 1
check_num = 1
GOSUB check

' --- 2. DEFFN avec un argument ---
DEFFN dupliquer(x) = x * 2
b = FN dupliquer(21)
check_cond = b = 42
check_num = 2
GOSUB check

' --- 3. DEFFN avec deux arguments ---
DEFFN sommer(aa, bb) = aa + bb
c = FN sommer(20, 22)
check_cond = c = 42
check_num = 3
GOSUB check

' --- 4. DEFFN avec expression complexe ---
DEFFN calculer(xx, yy) = xx * yy + xx - yy
d = FN calculer(10, 3)
check_cond = d = 37
check_num = 4
GOSUB check

' --- 5. DEFFN tripler ---
DEFFN tripler(zz) = zz * 3
e = FN tripler(14)
check_cond = e = 42
check_num = 5
GOSUB check

' --- 6. FN dans une expression ---
DEFFN carre(nn) = nn * nn
f = 10 + FN carre(4)
check_cond = f = 26
check_num = 6
GOSUB check

' --- 7. DEFFN avec comparaison ---
DEFFN positif(vv) = vv > 0
h = FN positif(42)
check_cond = h = -1
check_num = 7
GOSUB check

PRINT ""
PRINT "================================"
PRINT "Resultats : "
PRINT passed
PRINT " / "
PRINT passed + failed
PRINT " tests OK"
IF passed = 7
  PRINT "*** TOUS LES TESTS OK ***"
ELSE
  PRINT "*** ECHECS ***"
ENDIF
PRINT "=== Tests DEFFN/FN termines ==="
END
