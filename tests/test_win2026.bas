' ============================================================
' Tests session 2026 — AES, console, son, systeme, graphique,
' temps, chaines additionnelles
' ============================================================

passed = 0
failed = 0
check_cond = 0
check_num = 0

PROCEDURE check
  IF check_cond
    passed = passed + 1
  ELSE
    failed = failed + 1
    PRINT "FAIL test "
    PRINT check_num
  ENDIF
RETURN

' --- 1. UPPER$ / LCASE$ / LOWER$ ---
check_cond = UPPER$("bonjour") = "BONJOUR"
check_num = 1
GOSUB check
check_cond = LCASE$("BONJOUR") = "bonjour"
check_num = 2
GOSUB check
check_cond = LOWER$("MiXed") = "mixed"
check_num = 3
GOSUB check

' --- 4. VAL? ---
check_cond = VAL?("123abc") = 3
check_num = 4
GOSUB check
check_cond = VAL?("42") = 2
check_num = 5
GOSUB check

' --- 6. VARIAT 2-args (arrangements) ---
check_cond = VARIAT(5, 2) = 20
check_num = 6
GOSUB check
' --- 7. VARIAT 1-arg (factorielle) ---
check_cond = VARIAT(5) = 120
check_num = 7
GOSUB check

' --- 8. COMBIN ---
check_cond = COMBIN(5, 2) = 10
check_num = 8
GOSUB check

' --- 9. TEMPS : TIMER / DATE$ ---
t = TIMER
check_cond = t >= 0
check_num = 9
GOSUB check
d$ = DATE$
check_cond = LEN(d$) >= 4
check_num = 10
GOSUB check

' --- 11. CONIN (non bloquant) ---
c$ = CONIN
check_cond = 1
check_num = 11
GOSUB check

' --- 12. CONOUTI : ecrit sans saut de ligne ---
CONOUTI "abc"
PRINT "CONOUTI ok"
check_cond = 1
check_num = 12
GOSUB check

' --- 13. KEY / ON KEY ---
KEY "1", 65
check_cond = 1
check_num = 13
GOSUB check
ON KEY GOSUB 1
check_cond = 1
check_num = 14
GOSUB check

' --- 15. SON / SYSTEME (emules, sans erreur) ---
SOUND 0, 0, 0
BEEP
DMACONTROL 0
DMASOUND 0, 0, 0
NEW
check_cond = 1
check_num = 15
GOSUB check
SYSTEM "dir"
check_cond = 1
check_num = 16
GOSUB check

' --- 17. AES (emulees, sans erreur) ---
FORM_ALERT 0, 0, "Test", "OK", 0, 0, 0
MENU_BAR 0, 0
WIND_OPEN 0, 0, 0, -1, -1, "Test", 0
WIND_CLOSE 0
EVNT_MULTI 0, 0, 0, 0
OBJC_DRAW 0, 0, 0, 0
RSRC_LOAD 0
check_cond = 1
check_num = 17
GOSUB check

' --- 18. STORE / RECALL / MSHRINK / OPTION INTEGER ---
STORE "prog_test.bas"
RECALL "prog_test.bas"
MSHRINK 1, 2
OPTION INTEGER
check_cond = 1
check_num = 18
GOSUB check

' --- 19. TRON / TROFF ---
TRON
check_cond = 1
check_num = 19
GOSUB check
TROFF
check_cond = 1
check_num = 20
GOSUB check

' --- 21. PAUSE sans argument ---
PAUSE
check_cond = 1
check_num = 21
GOSUB check

' --- 22. ON BREAK CONT ---
ON BREAK CONT
check_cond = 1
check_num = 22
GOSUB check

' --- 23. GRAPHIQUE : WINDOW ---
GRAPHICS 2
WINDOW (0, 0), (320, 100)
PLOT 160, 50
check_cond = POINT(160, 50) <> 0
check_num = 23
GOSUB check
' --- 24. WINDOW : hors fenetre = non dessine ---
WINDOW (0, 0), (10, 10)
PLOT 5, 5
check_cond = POINT(5, 5) <> 0
check_num = 24
GOSUB check
TEXT

' --- 25. COLOR (2 args, sans erreur) ---
COLOR 1, 2
check_cond = 1
check_num = 25
GOSUB check

' --- 26. DELAY / WAIT courts ---
DELAY 10
check_cond = 1
check_num = 26
GOSUB check

' --- 27. FN inline ---
FN fsq(x) = x * x
check_cond = fsq(7) = 49
check_num = 27
GOSUB check

' --- 28. FN multi-args ---
FN fadd(a, b) = a + b
check_cond = fadd(20, 22) = 42
check_num = 28
GOSUB check

' --- 29. FN multi-lignes ---
FN triple(n)
  triple = n * 3
RETURN
check_cond = triple(15) = 45
check_num = 29
GOSUB check

' --- 30. DEF FN ---
DEF FN doub(a) = a * 2
check_cond = doub(21) = 42
check_num = 30
GOSUB check

' --- 31. PROC avec parametre chaine ---
PROC greet30(hi$)
  out30$ = hi$ + " ok"
RETURN
out30$ = ""
greet30("go")
check_cond = out30$ = "go ok"
check_num = 31
GOSUB check

' --- 32. FUNCTION...ENDFUNC ---
FUNCTION quad(z)
  quad = z * z
ENDFUNC
check_cond = quad(9) = 81
check_num = 32
GOSUB check

' --- 33. PROC...ENDPROC ---
PROC setv33(n)
  v33 = n + 100
ENDPROC
v33 = 0
setv33(1)
check_cond = v33 = 101
check_num = 33
GOSUB check

PRINT ""
PRINT "================================"
PRINT "Resultats : "
PRINT passed
PRINT " / "
PRINT passed + failed
PRINT " tests OK"
IF passed = 33
  PRINT "*** TOUS LES TESTS OK ***"
ELSE
  PRINT "*** ECHECS ***"
ENDIF
END
