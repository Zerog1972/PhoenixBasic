' test_approx_eq.bas — Tests pour l'operateur == approxime
PRINT "=== Tests operateur == approxime ==="
PRINT

' Test 1: 1.00000000009 == 1.0 devrait etre TRUE (-1) car diff < epsilon
a = 1.00000000009
r% = (a == 1.0)
PRINT "Test 1 (1.00000000009 == 1.0): ";
IF r% = -1
  PRINT "OK (TRUE)"
ELSE
  PRINT "FAIL: ";r%;" (attendu: -1)"
ENDIF

' Test 2: 1.0 == 1.0 TRUE
r% = (1.0 == 1.0)
PRINT "Test 2 (1.0 == 1.0): ";
IF r% = -1
  PRINT "OK (TRUE)"
ELSE
  PRINT "FAIL: ";r%;" (attendu: -1)"
ENDIF

' Test 3: 3.14159 == 3.14159 TRUE
r% = (3.14159 == 3.14159)
PRINT "Test 3 (3.14159 == 3.14159): ";
IF r% = -1
  PRINT "OK (TRUE)"
ELSE
  PRINT "FAIL: ";r%;" (attendu: -1)"
ENDIF

' Test 4: 1.0000001 == 1.0 FALSE (0) car diff > 1e-10
r% = (1.0000001 == 1.0)
PRINT "Test 4 (1.0000001 == 1.0): ";
IF r% = 0
  PRINT "OK (FALSE)"
ELSE
  PRINT "FAIL: ";r%;" (attendu: 0)"
ENDIF

' Test 5: 42 == 42.5 FALSE
r% = (42 == 42.5)
PRINT "Test 5 (42 == 42.5): ";
IF r% = 0
  PRINT "OK (FALSE)"
ELSE
  PRINT "FAIL: ";r%;" (attendu: 0)"
ENDIF

' Test 6: 0.0 == 0.0 TRUE
r% = (0.0 == 0.0)
PRINT "Test 6 (0.0 == 0.0): ";
IF r% = -1
  PRINT "OK (TRUE)"
ELSE
  PRINT "FAIL: ";r%;" (attendu: -1)"
ENDIF

' Test 7: -1.0 == -1.0 TRUE
r% = (-1.0 == -1.0)
PRINT "Test 7 (-1.0 == -1.0): ";
IF r% = -1
  PRINT "OK (TRUE)"
ELSE
  PRINT "FAIL: ";r%;" (attendu: -1)"
ENDIF

' Test 8: -0.00000000009 == 0.0 TRUE (signe indifferent dans la tolerance)
r% = (-0.00000000009 == 0.0)
PRINT "Test 8 (-0.00000000009 == 0.0): ";
IF r% = -1
  PRINT "OK (TRUE)"
ELSE
  PRINT "FAIL: ";r%;" (attendu: -1)"
ENDIF

' Test 9: Epsilon boundary — exactly at 1e-10 should be FALSE (0)
r% = (1.0000000001 == 1.0)
PRINT "Test 9 (1.0000000001 == 1.0, diff=1e-10): ";
IF r% = 0
  PRINT "OK (FALSE)"
ELSE
  PRINT "FAIL: ";r%;" (attendu: 0)"
ENDIF

' Test 10: Grands nombres — 1e15 vs 1e15+1 devrait etre FALSE (0)
r% = (1.0e15 == (1.0e15 + 1.0))
PRINT "Test 10 (1e15 == 1e15+1): ";
IF r% = 0
  PRINT "OK (FALSE)"
ELSE
  PRINT "FAIL: ";r%;" (attendu: 0)"
ENDIF

PRINT
PRINT "=== Tests == approxime termines ==="
