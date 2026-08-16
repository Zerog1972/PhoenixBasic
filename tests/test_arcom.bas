' ============================================================
' Tests complets tableaux — GFA Basic 3.5
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

PRINT "=== Tests complets tableaux ==="
PRINT ""

' --- 1. DIM et assignation ---
DIM tarr(5)
tarr(0) = 10
tarr(1) = 20
tarr(2) = 30
tarr(3) = 40
tarr(4) = 50
check_cond = (tarr(0) + tarr(4)) = 60
check_num = 1
GOSUB check

' --- 2. Lecture/ecriture tableau ---
DIM arr(10)
arr(5) = 99
b = arr(5)
check_cond = (b = 99)
check_num = 2
GOSUB check

' --- 3. Expression avec indices ---
DIM varr(3)
varr(0) = 1
varr(1) = 2
varr(2) = 3
check_cond = (varr(0) + varr(1) + varr(2) = 6)
check_num = 3
GOSUB check

' --- 4. Grand tableau ---
DIM farr(4)
farr(0) = 7.5
farr(1) = 7.5
farr(2) = 7.5
farr(3) = 7.5
check_cond = (farr(0) + farr(3) = 15)
check_num = 4
GOSUB check

' --- 5. Grand tableau ---
DIM bigarr(50)
bigarr(25) = 777
x = bigarr(25)
check_cond = (x = 777)
check_num = 5
GOSUB check

' --- 6. Indices calcules ---
DIM arrcalc(10)
arrcalc(1 + 2) = 999
g = arrcalc(3)
check_cond = (g = 999)
check_num = 6
GOSUB check

' --- 7. ERASE ---
DIM arrera(5)
ERASE arrera
DIM arrera(3)
arrera(0) = 123
h = arrera(0)
check_cond = (h = 123)
check_num = 7
GOSUB check

' --- 8. Tableau dans expression ---
DIM arrp(2)
arrp(0) = 5
arrp(1) = 10
arrp(2) = 15
i = arrp(0) + arrp(1) + arrp(2)
check_cond = (i = 30)
check_num = 8
GOSUB check

' --- 9. OPTION BASE 1 (indice bas = 1) ---
OPTION BASE 1
DIM bbase(10)
bbase(1) = 42
bbase(10) = 8
q = bbase(1) + bbase(10)
check_cond = (q = 50)
check_num = 9
GOSUB check

PRINT ""
PRINT "================================"
PRINT "Resultats : "
PRINT passed
PRINT " / "
PRINT passed + failed
PRINT " tests OK"
IF passed = 9
  PRINT "*** TOUS LES TESTS OK ***"
ELSE
  PRINT "*** ECHECS ***"
ENDIF
PRINT "=== Tests tableaux complets termines ==="
END
