' ============================================================
' Tests complets fonctions chaines — GFA Basic 3.5
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

PRINT "=== Tests complets fonctions chaines ==="
PRINT ""

' --- 1. ASC ---
check_cond = ASC("A") = 65
check_num = 1
GOSUB check

' --- 2. CHR$ ---
a$ = CHR$(66)
check_cond = a$ = "B"
check_num = 2
GOSUB check

' --- 3. LEN ---
check_cond = LEN("Test") = 4
check_num = 3
GOSUB check

' --- 4. LEFT$ ---
b$ = LEFT$("phoenix", 3)
check_cond = b$ = "pho"
check_num = 4
GOSUB check

' --- 5. RIGHT$ ---
c$ = RIGHT$("phoenix", 3)
check_cond = c$ = "nix"
check_num = 5
GOSUB check

' --- 6. MID$ ---
d$ = MID$("Bonjour", 3, 4)
check_cond = d$ = "njou"
check_num = 6
GOSUB check

' --- 7. INSTR ---
e = INSTR("hello world", "world")
check_cond = e = 7
check_num = 7
GOSUB check

' --- 8. RINSTR ---
f = RINSTR("banana", "na")
check_cond = f = 5
check_num = 8
GOSUB check

' --- 9. STR$ ---
g$ = STR$(3.14)
check_cond = LEFT$(g$, 3) = "3.1"
check_num = 9
GOSUB check

' --- 10. VAL ---
h = VAL("42.5")
check_cond = h = 42.5
check_num = 10
GOSUB check

' --- 11. BIN$ ---
i$ = BIN$(5)
check_cond = LEN(i$) > 0
check_num = 11
GOSUB check

' --- 12. HEX$ ---
j$ = HEX$(255)
check_cond = LEN(j$) > 0
check_num = 12
GOSUB check

' --- 13. OCT$ ---
k$ = OCT$(8)
check_cond = LEN(k$) > 0
check_num = 13
GOSUB check

' --- 14. SPACE$ ---
l$ = SPACE$(3)
check_cond = l$ = "   "
check_num = 14
GOSUB check

' --- 15. STRING$ ---
m$ = STRING$(4, "X")
check_cond = m$ = "XXXX"
check_num = 15
GOSUB check

' --- 16. UPPER$ ---
n$ = UPPER$("lower")
check_cond = n$ = "LOWER"
check_num = 16
GOSUB check

' --- 17. LCASE$ ---
o$ = LCASE$("UPPER")
check_cond = o$ = "upper"
check_num = 17
GOSUB check

' --- 18. TRIM$ ---
p$ = TRIM$("  xx  ")
check_cond = p$ = "xx"
check_num = 18
GOSUB check

PRINT ""
PRINT "================================"
PRINT "Resultats : "
PRINT passed
PRINT " / "
PRINT passed + failed
PRINT " tests OK"
IF passed = 18
  PRINT "*** TOUS LES TESTS OK ***"
ELSE
  PRINT "*** ECHECS ***"
ENDIF
PRINT "=== Tests chaines complets termines ==="
END
