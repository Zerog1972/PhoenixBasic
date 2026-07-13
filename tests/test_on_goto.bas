' ============================================================
' Tests ON x GOTO / ON x GOSUB — GFA Basic 3.5
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

PRINT "=== Tests ON GOTO/GOSUB ==="
PRINT ""

' --- 1. ON GOTO index=1 -> premier label ---
result = 0
x = 1
ON x GOTO lab1, lab2, lab3
result = 99
GOTO test1_end
lab1:
result = 1
GOTO test1_end
lab2:
result = 2
GOTO test1_end
lab3:
result = 3
test1_end:
check_cond = result = 1
check_num = 1
GOSUB check

' --- 2. ON GOTO index=2 -> deuxieme label ---
x = 2
ON x GOTO lab2_1, lab2_2, lab2_3
result = 99
GOTO test2_end
lab2_1:
result = 1
GOTO test2_end
lab2_2:
result = 2
GOTO test2_end
lab2_3:
result = 3
test2_end:
check_cond = result = 2
check_num = 2
GOSUB check

' --- 3. ON GOTO index=3 -> troisieme label ---
x = 3
ON x GOTO lab3_1, lab3_2, lab3_3
result = 99
GOTO test3_end
lab3_1:
result = 1
GOTO test3_end
lab3_2:
result = 2
GOTO test3_end
lab3_3:
result = 3
test3_end:
check_cond = result = 3
check_num = 3
GOSUB check

' --- 4. ON GOSUB index=1 ---
result = 0
x = 1
ON x GOSUB lab4_1, lab4_2, lab4_3
GOTO test4_end
lab4_1:
result = 1
RETURN
lab4_2:
result = 2
RETURN
lab4_3:
result = 3
RETURN
test4_end:
check_cond = result = 1
check_num = 4
GOSUB check

' --- 5. ON GOSUB index=2 ---
x = 2
ON x GOSUB lab5_1, lab5_2, lab5_3
GOTO test5_end
lab5_1:
result = 1
RETURN
lab5_2:
result = 2
RETURN
lab5_3:
result = 3
RETURN
test5_end:
check_cond = result = 2
check_num = 5
GOSUB check

' --- 6. ON GOSUB index=3 ---
x = 3
ON x GOSUB lab6_1, lab6_2, lab6_3
GOTO test6_end
lab6_1:
result = 1
RETURN
lab6_2:
result = 2
RETURN
lab6_3:
result = 3
RETURN
test6_end:
check_cond = result = 3
check_num = 6
GOSUB check

' --- 7. ON GOTO avec expression ---
x = 1 + 1
ON x GOTO lab7_1, lab7_2, lab7_3
result = 99
GOTO test7_end
lab7_1:
result = 1
GOTO test7_end
lab7_2:
result = 2
GOTO test7_end
lab7_3:
result = 3
test7_end:
check_cond = result = 2
check_num = 7
GOSUB check

' --- 8. ON GOSUB avec expression complexe ---
x = 3 * 1
ON x GOSUB lab8_1, lab8_2, lab8_3
GOTO test8_end
lab8_1:
result = 1
RETURN
lab8_2:
result = 2
RETURN
lab8_3:
result = 3
RETURN
test8_end:
check_cond = result = 3
check_num = 8
GOSUB check

PRINT ""
PRINT "================================"
PRINT "Resultats : "
PRINT passed
PRINT " / "
PRINT passed + failed
PRINT " tests OK"
IF passed = 8
  PRINT "*** TOUS LES TESTS OK ***"
ELSE
  PRINT "*** ECHECS ***"
ENDIF
PRINT "=== Tests ON GOTO/GOSUB termines ==="
END
