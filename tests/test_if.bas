' ============================================================
' Tests IF / THEN / ELSE / ENDIF — GFA Basic 3.5
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

PRINT "=== Tests IF/THEN/ELSE/ENDIF ==="
PRINT ""

' --- 1. IF simple (vrai) ---
result = 0
IF 1 = 1
  result = 1
ENDIF
check_cond = result = 1
check_num = 1
GOSUB check

' --- 2. IF simple (faux) ---
result = 1
IF 1 = 2
  result = 0
ENDIF
check_cond = result = 1
check_num = 2
GOSUB check

' --- 3. IF/ELSE (branche IF) ---
result = 0
IF 5 > 3
  result = 1
ELSE
  result = 2
ENDIF
check_cond = result = 1
check_num = 3
GOSUB check

' --- 4. IF/ELSE (branche ELSE) ---
result = 0
IF 5 < 3
  result = 1
ELSE
  result = 2
ENDIF
check_cond = result = 2
check_num = 4
GOSUB check

' --- 5. Comparaison = ---
result = 0
IF 10 = 10
  result = 1
ENDIF
check_cond = result = 1
check_num = 5
GOSUB check

' --- 6. Comparaison <> ---
result = 0
IF 10 <> 5
  result = 1
ENDIF
check_cond = result = 1
check_num = 6
GOSUB check

' --- 7. Comparaison < ---
result = 0
IF 3 < 10
  result = 1
ENDIF
check_cond = result = 1
check_num = 7
GOSUB check

' --- 8. Comparaison > ---
result = 0
IF 10 > 3
  result = 1
ENDIF
check_cond = result = 1
check_num = 8
GOSUB check

' --- 9. Comparaison <= ---
result = 0
IF 5 <= 5
  result = 1
ENDIF
check_cond = result = 1
check_num = 9
GOSUB check

' --- 10. Comparaison >= ---
result = 0
IF 5 >= 5
  result = 1
ENDIF
check_cond = result = 1
check_num = 10
GOSUB check

' --- 11. IF avec variable ---
x = 7
result = 0
IF x > 5
  result = 1
ENDIF
check_cond = result = 1
check_num = 11
GOSUB check

' --- 12. IF avec expression ---
a = 3
b = 4
result = 0
IF a + b > 6
  result = 1
ENDIF
check_cond = result = 1
check_num = 12
GOSUB check

' --- 13. IF imbrique ---
result = 0
IF 5 > 0
  IF 10 > 5
    result = 1
  ENDIF
ENDIF
check_cond = result = 1
check_num = 13
GOSUB check

' --- 14. IF/ELSE imbrique ---
result = 0
IF 1 = 0
  result = 99
ELSE
  IF 2 = 2
    result = 1
  ENDIF
ENDIF
check_cond = result = 1
check_num = 14
GOSUB check

' --- 15. TRUE constante ---
result = 0
IF TRUE
  result = 1
ENDIF
check_cond = result = 1
check_num = 15
GOSUB check

' --- 16. FALSE constante ---
result = 1
IF FALSE
  result = 0
ENDIF
check_cond = result = 1
check_num = 16
GOSUB check

' --- 17. IF avec 0 (faux) ---
result = 1
IF 0
  result = 0
ENDIF
check_cond = result = 1
check_num = 17
GOSUB check

' --- 18. IF avec nombre non nul (vrai) ---
result = 0
IF 42
  result = 1
ENDIF
check_cond = result = 1
check_num = 18
GOSUB check

' --- 19. IF avec negatif (vrai en GFA) ---
result = 0
IF -1
  result = 1
ENDIF
check_cond = result = 1
check_num = 19
GOSUB check

' --- 20. IF avec AND ---
result = 0
IF (5 > 3) AND (10 > 5)
  result = 1
ENDIF
check_cond = result = 1
check_num = 20
GOSUB check

' --- 21. IF avec OR ---
result = 0
IF (1 > 10) OR (5 > 3)
  result = 1
ENDIF
check_cond = result = 1
check_num = 21
GOSUB check

' --- 22. IF avec NOT ---
result = 0
IF NOT (1 > 10)
  result = 1
ENDIF
check_cond = result = 1
check_num = 22
GOSUB check

' --- 23. IF chaines egales ---
result = 0
IF "hello" = "hello"
  result = 1
ENDIF
check_cond = result = 1
check_num = 23
GOSUB check

' --- 24. IF chaines differentes ---
result = 1
IF "hello" = "world"
  result = 0
ENDIF
check_cond = result = 1
check_num = 24
GOSUB check

' --- 25. IF multi-lignes ELSE ---
result = 0
x = 3
IF x > 10
  result = 99
  x = 0
ELSE
  result = 1
  x = 42
ENDIF
check_cond = result = 1
check_num = 25
GOSUB check

' --- 26. THEN sur une ligne ---
result = 0
IF 1 = 1 THEN result = 1
check_cond = result = 1
check_num = 26
GOSUB check

' --- 27. THEN/ELSE sur une ligne ---
result = 0
IF 1 = 2 THEN result = 99 ELSE result = 1
check_cond = result = 1
check_num = 27
GOSUB check

' --- 28. IF/ELSEIF simule ---
result = 0
valeur = 20
IF valeur < 10
  result = 1
ELSE
  IF valeur < 30
    result = 2
  ELSE
    result = 3
  ENDIF
ENDIF
check_cond = result = 2
check_num = 28
GOSUB check

' --- 29. IF avec appel fonction ---
FUNCTION isEven(n)
  IF n MOD 2 = 0
    RETURN 1
  ELSE
    RETURN 0
  ENDIF
ENDFUNC

result = 0
IF isEven(4)
  result = 1
ENDIF
check_cond = result = 1
check_num = 29
GOSUB check

' --- 30. IF avec NOT fonction ---
result = 1
IF NOT isEven(3)
  result = 0
ENDIF
check_cond = result = 0
check_num = 30
GOSUB check

' --- 31. IF imbrique profond ---
result = 0
IF 1 = 1
  IF 2 = 2
    IF 3 = 3
      result = 1
    ENDIF
  ENDIF
ENDIF
check_cond = result = 1
check_num = 31
GOSUB check

' --- Résumé ---
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
