' ============================================================
' Tests exhaustifs operateurs — GFA Basic 3.5
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

PRINT "=== Tests exhaustifs operateurs ==="
PRINT ""

' --- 1. Addition ---
check_cond = (2 + 3) = 5
check_num = 1
GOSUB check

' --- 2. Soustraction ---
check_cond = (10 - 3) = 7
check_num = 2
GOSUB check

' --- 3. Multiplication ---
check_cond = (4 * 5) = 20
check_num = 3
GOSUB check

' --- 4. Division ---
check_cond = (20 / 4) = 5
check_num = 4
GOSUB check

' --- 5. Puissance ---
check_cond = (2 ^ 3) = 8
check_num = 5
GOSUB check

' --- 6. DIV ---
check_cond = (17 DIV 5) = 3
check_num = 6
GOSUB check

' --- 7. MOD ---
check_cond = (17 MOD 5) = 2
check_num = 7
GOSUB check

' --- 8. AND bitwise ---
check_cond = (3 AND 5) = 1
check_num = 8
GOSUB check

' --- 9. OR bitwise ---
check_cond = (3 OR 5) = 7
check_num = 9
GOSUB check

' --- 10. XOR bitwise ---
check_cond = (3 XOR 5) = 6
check_num = 10
GOSUB check

' --- 11. EQV ---
check_cond = (3 EQV 5) = -7
check_num = 11
GOSUB check

' --- 12. IMP ---
check_cond = (3 IMP 5) = -3
check_num = 12
GOSUB check

' --- 13. Precedence * avant + ---
check_cond = (2 + 3 * 4) = 14
check_num = 13
GOSUB check

' --- 14. Parentheses ---
check_cond = ((2 + 3) * 4) = 20
check_num = 14
GOSUB check

' --- 15. NOT ---
check_cond = (NOT 0) = -1
check_num = 15
GOSUB check

' --- 16. Comparaisons ---
a = (5 > 3)
b = (2 > 10)
check_cond = a AND (NOT b)
check_num = 16
GOSUB check

' --- 17. Comparaison chaine ---
c = 10
check_cond = (5 < c) OR (c = 0)
check_num = 17
GOSUB check

' --- 18. Negation unaire ---
check_cond = (-(5 + 3)) = -8
check_num = 18
GOSUB check

' --- 19. Expression imbriquee ---
d = ((10 + 5) * 2 - 8) / 3
e = (d - 7.333333)
IF e < 0
  e = -e
ENDIF
check_cond = e < 0.001
check_num = 19
GOSUB check

' --- 20. Comparaisons longues ---
e = 100
check_cond = (50 < e) AND (e < 200)
check_num = 20
GOSUB check

PRINT ""
PRINT "================================"
PRINT "Resultats : "
PRINT passed
PRINT " / "
PRINT passed + failed
PRINT " tests OK"
IF passed = 20
  PRINT "*** TOUS LES TESTS OK ***"
ELSE
  PRINT "*** ECHECS ***"
ENDIF
PRINT "=== Tests operateurs termines ==="
END
