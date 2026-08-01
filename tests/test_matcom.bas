' ============================================================
' Tests complets maths — GFA Basic 3.5
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

PRINT "=== Tests complets fonctions maths ==="
PRINT ""

' --- 1. ABS ---
check_cond = ABS(-42) = 42
check_num = 1
GOSUB check

' --- 2. SGN ---
check_cond = SGN(-5) = -1
check_num = 2
GOSUB check

' --- 3. SGN(0) ---
check_cond = SGN(0) = 0
check_num = 3
GOSUB check

' --- 4. SGN(7) ---
check_cond = SGN(7) = 1
check_num = 4
GOSUB check

' --- 5. INT ---
check_cond = INT(3.7) = 3
check_num = 5
GOSUB check

' --- 6. INT negatif ---
check_cond = INT(-3.7) = -4
check_num = 6
GOSUB check

' --- 7. FRAC ---
a = FRAC(3.7)
check_cond = a > 0.699
check_num = 7
GOSUB check

' --- 8. FIX ---
check_cond = FIX(-3.7) = -3
check_num = 8
GOSUB check

' --- 9. ROUND ---
check_cond = ROUND(3.5) = 4
check_num = 9
GOSUB check

' --- 10. TRUNC ---
check_cond = TRUNC(-3.7) = -3
check_num = 10
GOSUB check

' --- 11. SQR ---
check_cond = SQR(16) = 4
check_num = 11
GOSUB check

' --- 12. SQR(0) ---
check_cond = SQR(0) = 0
check_num = 12
GOSUB check

' --- 13. EXP(0) ---
check_cond = EXP(0) = 1
check_num = 13
GOSUB check

' --- 14. LOG(1) ---
check_cond = LOG(1) = 0
check_num = 14
GOSUB check

' --- 15. LOG10(1) ---
check_cond = LOG10(1) = 0
check_num = 15
GOSUB check

' --- 16. LOG10(10) ---
check_cond = LOG10(10) > 0.999
check_num = 16
GOSUB check

' --- 17. SIN(0) ---
b = SIN(0)
check_cond = b > -0.0001
check_num = 17
GOSUB check

' --- 18. COS(0) ---
check_cond = COS(0) > 0.999
check_num = 18
GOSUB check

' --- 19. TAN(0) ---
check_cond = TAN(0) > -0.0001
check_num = 19
GOSUB check

' --- 20. ATN(0) ---
check_cond = ATN(0) > -0.0001
check_num = 20
GOSUB check

' --- 21. ASIN(0) ---
check_cond = ASIN(0) > -0.0001
check_num = 21
GOSUB check

' --- 22. ACOS(1) ---
check_cond = ACOS(1) > -0.0001
check_num = 22
GOSUB check

' --- 23. SINH(0) ---
check_cond = SINH(0) > -0.0001
check_num = 23
GOSUB check

' --- 24. COSH(0) ---
check_cond = COSH(0) > 0.999
check_num = 24
GOSUB check

' --- 25. TANH(0) ---
check_cond = TANH(0) > -0.0001
check_num = 25
GOSUB check

' --- 26. SINQ(0) ---
check_cond = SINQ(0) > -0.0001
check_num = 26
GOSUB check

' --- 27. COSQ(0) ---
check_cond = COSQ(0) > 0.999
check_num = 27
GOSUB check

' --- 28. MIN ---
check_cond = MIN(3, 7) = 3
check_num = 28
GOSUB check

' --- 29. MAX ---
check_cond = MAX(3, 7) = 7
check_num = 29
GOSUB check

' --- 30. FACT ---
check_cond = FACT(5) = 120
check_num = 30
GOSUB check

' --- 31. COMBIN ---
check_cond = COMBIN(5, 2) = 10
check_num = 31
GOSUB check

' --- 32. VARIAT ---
check_cond = VARIAT(5, 2) = 20
check_num = 32
GOSUB check

' --- 33. EVEN ---
check_cond = EVEN(4) AND (NOT EVEN(5))
check_num = 33
GOSUB check

' --- 34. ODD ---
check_cond = ODD(3) AND (NOT ODD(4))
check_num = 34
GOSUB check

' --- 35. DEG ---
c = DEG(3.14159265)
check_cond = c > 179.9
check_num = 35
GOSUB check

' --- 36. RAD ---
d = RAD(180)
check_cond = d > 3.141
check_num = 36
GOSUB check

' --- 37. CFLOAT ---
check_cond = CFLOAT(5) = 5.0
check_num = 37
GOSUB check

' --- 38. PRED ---
check_cond = PRED(5) = 4
check_num = 38
GOSUB check

' --- 39. SUCC ---
check_cond = SUCC(5) = 6
check_num = 39
GOSUB check

' --- 40. RND ---
e = RND(1)
check_cond = (e >= 0) AND (e <= 1)
check_num = 40
GOSUB check

PRINT ""
PRINT "================================"
PRINT "Resultats : "
PRINT passed
PRINT " / "
PRINT passed + failed
PRINT " tests OK"
IF passed = 40
  PRINT "*** TOUS LES TESTS OK ***"
ELSE
  PRINT "*** ECHECS ***"
ENDIF
PRINT "=== Tests maths complets termines ==="
END
