' ============================================================
' Tests operations chaines / concatenation — GFA Basic 3.5
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

PRINT "=== Tests operations chaines ==="
PRINT ""

' --- 1. Concatenation + ---
a$ = "Hello"
b$ = " World"
c$ = a$ + b$
check_cond = c$ = "Hello World"
check_num = 1
GOSUB check

' --- 2. Concatenation chaine vide ---
d$ = "" + "OK"
check_cond = d$ = "OK"
check_num = 2
GOSUB check

' --- 3. Comparaison chaines = ---
e$ = "ABC"
f$ = "ABC"
check_cond = e$ = f$
check_num = 3
GOSUB check

' --- 4. Comparaison chaines <> ---
g$ = "Hello"
h$ = "World"
check_cond = g$ <> h$
check_num = 4
GOSUB check

' --- 5. Chaine + STR$ ---
i$ = "Val: " + STR$(42)
check_cond = i$ = "Val: 42"
check_num = 5
GOSUB check

' --- 6. LEN apres concatenation ---
j$ = "abc" + "def" + "ghi"
check_cond = LEN(j$) = 9
check_num = 6
GOSUB check

' --- 7. LEFT$ apres concat ---
k$ = "1234" + "5678"
l$ = LEFT$(k$, 5)
check_cond = l$ = "12345"
check_num = 7
GOSUB check

' --- 8. RIGHT$ apres concat ---
m$ = RIGHT$(k$, 5)
check_cond = m$ = "45678"
check_num = 8
GOSUB check

' --- 9. MID$ apres concat ---
n$ = MID$(k$, 2, 6)
check_cond = n$ = "234567"
check_num = 9
GOSUB check

' --- 10. UPPER$ avec concat ---
o$ = UPPER$("ab" + "cd")
check_cond = o$ = "ABCD"
check_num = 10
GOSUB check

' --- 11. LCASE$ ---
p$ = LCASE$("XYZ")
check_cond = p$ = "xyz"
check_num = 11
GOSUB check

' --- 12. TRIM$ ---
q$ = TRIM$("  test  ")
check_cond = q$ = "test"
check_num = 12
GOSUB check

PRINT ""
PRINT "================================"
PRINT "Resultats : "
PRINT passed
PRINT " / "
PRINT passed + failed
PRINT " tests OK"
IF passed = 12
  PRINT "*** TOUS LES TESTS OK ***"
ELSE
  PRINT "*** ECHECS ***"
ENDIF
PRINT "=== Tests chaines termines ==="
END
