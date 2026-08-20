' === Tests nouvelles instructions (2026-08) ===
' Chaque bloc affiche "Test N : OK" ou "Test N : FAIL"
passed = 0
failed = 0

' --- 1. FOR...DOWNTO ---
s = 0
FOR i = 5 DOWNTO 1
  s = s + i
NEXT
check = (s = 15)
IF check THEN
  passed = passed + 1
  PRINT "Test 1 : OK"
ELSE
  failed = failed + 1
  PRINT "Test 1 : FAIL (s=" ; s ; ")"
ENDIF

' --- 2. FOR DOWNTO avec STEP ---
s = 0
FOR i = 10 DOWNTO 2 STEP -2
  s = s + i
NEXT
check = (s = 30)
IF check THEN
  passed = passed + 1
  PRINT "Test 2 : OK"
ELSE
  failed = failed + 1
  PRINT "Test 2 : FAIL (s=" ; s ; ")"
ENDIF

' --- 3. QSORT ---
DIM q(5)
q(0) = 5
q(1) = 2
q(2) = 9
q(3) = 1
q(4) = 7
QSORT q(), 0, 4
check = (q(0) = 1 AND q(1) = 2 AND q(2) = 5 AND q(3) = 7 AND q(4) = 9)
IF check THEN
  passed = passed + 1
  PRINT "Test 3 : OK"
ELSE
  failed = failed + 1
  PRINT "Test 3 : FAIL"
ENDIF

' --- 4. SSORT (tri par insertion) ---
DIM s2(4)
s2(0) = 4
s2(1) = 1
s2(2) = 3
s2(3) = 2
SSORT s2(), 0, 3
check = (s2(0) = 1 AND s2(1) = 2 AND s2(2) = 3 AND s2(3) = 4)
IF check THEN
  passed = passed + 1
  PRINT "Test 4 : OK"
ELSE
  failed = failed + 1
  PRINT "Test 4 : FAIL"
ENDIF

' --- 5. INSERT ---
DIM ins(4)
ins(0) = 10
ins(1) = 20
ins(2) = 30
ins(3) = 0
INSERT ins(1), 15
check = (ins(0) = 10 AND ins(1) = 15 AND ins(2) = 20 AND ins(3) = 30)
IF check THEN
  passed = passed + 1
  PRINT "Test 5 : OK"
ELSE
  failed = failed + 1
  PRINT "Test 5 : FAIL"
ENDIF

' --- 6. DELETE ---
DIM del(4)
del(0) = 10
del(1) = 20
del(2) = 30
del(3) = 40
DELETE del(1)
check = (del(0) = 10 AND del(1) = 30 AND del(2) = 40 AND del(3) = 0)
IF check THEN
  passed = passed + 1
  PRINT "Test 6 : OK"
ELSE
  failed = failed + 1
  PRINT "Test 6 : FAIL"
ENDIF

' --- 7. UCASE$ ---
u$ = UCASE$("bonjour")
check = (u$ = "BONJOUR")
IF check THEN
  passed = passed + 1
  PRINT "Test 7 : OK"
ELSE
  failed = failed + 1
  PRINT "Test 7 : FAIL (" ; u$ ; ")"
ENDIF

' --- 8. LSET/RSET ---
l$ = "abcd"
LSET l$ = "xy"
r$ = "abcd"
RSET r$ = "xy"
check = (l$ = "xy" AND r$ = "xy")
IF check THEN
  passed = passed + 1
  PRINT "Test 8 : OK"
ELSE
  failed = failed + 1
  PRINT "Test 8 : FAIL"
ENDIF

' --- 9. MAT SET / CLR / ONE ---
DIM m(2,2)
MAT m = 7
check = (m(0,0) = 7)
MAT CLR m
check = check AND (m(0,0) = 0)
MAT ONE m
check = check AND (m(0,0) = 1 AND m(1,1) = 1 AND m(0,1) = 0)
IF check THEN
  passed = passed + 1
  PRINT "Test 9 : OK"
ELSE
  failed = failed + 1
  PRINT "Test 9 : FAIL"
ENDIF

' --- 10. MAT ADD/SUB/MUL ---
DIM a2(2,2)
DIM b2(2,2)
DIM c2(2,2)
a2(0,0) = 1 : a2(0,1) = 2
a2(1,0) = 3 : a2(1,1) = 4
b2(0,0) = 5 : b2(0,1) = 6
b2(1,0) = 7 : b2(1,1) = 8
MAT c2 = a2 + b2
check = (c2(0,0) = 6 AND c2(1,1) = 12)
IF check THEN
  passed = passed + 1
  PRINT "Test 10 : OK"
ELSE
  failed = failed + 1
  PRINT "Test 10 : FAIL"
ENDIF

' --- 11. MAT TRANS ---
DIM d2(2,2)
MAT d2 = TRN(a2)
check = (d2(0,1) = 3 AND d2(1,0) = 2)
IF check THEN
  passed = passed + 1
  PRINT "Test 11 : OK"
ELSE
  failed = failed + 1
  PRINT "Test 11 : FAIL"
ENDIF

' --- 12. KILL/MKDIR/FILES ---
KILL "build/t_testfile.txt"
RMDIR "build/t_testdir"
MKDIR "build/t_testdir"
check = 1  ' ne doit pas planter
IF check THEN
  passed = passed + 1
  PRINT "Test 12 : OK"
ELSE
  failed = failed + 1
  PRINT "Test 12 : FAIL"
ENDIF

' --- 13. VTAB/HTAB (ne doit pas planter) ---
VTAB 5
HTAB 10
passed = passed + 1
PRINT "Test 13 : OK"

' --- 14. Turtle DRAW (ne doit pas planter) ---
DRAW "PU FD 50 RT 90 FD 50"
check = 1
IF check THEN
  passed = passed + 1
  PRINT "Test 14 : OK"
ELSE
  failed = failed + 1
  PRINT "Test 14 : FAIL"
ENDIF

' --- 15. Fenetres (CLEARW/TITLEW ne doivent pas planter) ---
CLEARW 0
TITLEW 0, "Test"
passed = passed + 1
PRINT "Test 15 : OK"

' --- 16. Graphismes (PLOT/HLINE/PELLIPSE) ---
PLOT 100, 100
HLINE 50, 20, 80
PELLIPSE 200, 200, 30, 20
passed = passed + 1
PRINT "Test 16 : OK"

' --- 17. SHEL_ENVRN ---
e$ = SHEL_ENVRN("PATH")
check = (LEN(e$) > 0)
IF check THEN
  passed = passed + 1
  PRINT "Test 17 : OK"
ELSE
  failed = failed + 1
  PRINT "Test 17 : FAIL"
ENDIF

' --- 18. OPTION BASE 1 (tableau) ---
OPTION BASE 1
DIM ob(3)
ob(1) = 100
ob(3) = 300
check = (ob(1) = 100 AND ob(3) = 300)
OPTION BASE 0
IF check THEN
  passed = passed + 1
  PRINT "Test 18 : OK"
ELSE
  failed = failed + 1
  PRINT "Test 18 : FAIL"
ENDIF

PRINT ""
PRINT "================================"
PRINT "Resultats : "
PRINT passed
PRINT " / "
PRINT passed + failed
PRINT " tests OK"
IF passed = passed + failed THEN
  PRINT "*** TOUS LES TESTS OK ***"
ELSE
  PRINT "*** ECHECS ***"
ENDIF
PRINT "=== Tests nouvelles instructions termines ==="

REM --- Nettoyage residus (crash precedent inclus) ---
KILL "build/t_testfile.txt"
RMDIR "build/t_testdir"

END
