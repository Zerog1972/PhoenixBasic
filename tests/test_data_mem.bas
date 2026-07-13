' ============================================================
' Tests DATA / READ / RESTORE / PEEK / POKE — GFA Basic 3.5
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

DATA 10, 20, 30, 40, 50

PRINT "=== Tests DATA/READ/RESTORE ==="
PRINT ""

' --- 1. READ sequentiel ---
READ a, b, c
check_cond = a = 10
check_num = 1
GOSUB check

READ d, e
check_cond = d = 40
check_num = 2
GOSUB check

' --- 2. RESTORE reset DATA ---
RESTORE
READ f, g
check_cond = f = 10
check_num = 3
GOSUB check

' --- 3. READ dans variable Long ---
RESTORE
READ h
check_cond = h = 10
check_num = 4
GOSUB check

' --- 4. RESTORE et READ complet ---
RESTORE
READ i1, i2, i3, i4, i5
check_cond = i3 = 30
check_num = 5
GOSUB check

' --- 5. DATA supplementaires ---
DATA 100, 200
READ j1, j2
check_cond = j1 = 100
check_num = 6
GOSUB check

' --- 6. PEEK ne crashe pas ---
p = PEEK(0)
check_cond = 1 = 1
check_num = 7
GOSUB check

' --- 7. DPEEK ne crashe pas ---
q = DPEEK(0)
check_cond = 1 = 1
check_num = 8
GOSUB check

' --- 8. LPEEK ne crashe pas ---
r = LPEEK(0)
check_cond = 1 = 1
check_num = 9
GOSUB check

' --- 9. POKE ne crashe pas ---
POKE 65536, 42
check_cond = 1 = 1
check_num = 10
GOSUB check

' --- 10. DPOKE et LPOKE ne crashent pas ---
DPOKE 65536, 1234
LPOKE 65536, 56789
check_cond = 1 = 1
check_num = 11
GOSUB check

' --- 11. SPOKE ne crashe pas ---
SPOKE 65536, 255
check_cond = 1 = 1
check_num = 12
GOSUB check

' --- 12. SDPOKE ne crashe pas ---
SDPOKE 65536, 32767
check_cond = 1 = 1
check_num = 13
GOSUB check

' --- 13. SLPOKE ne crashe pas ---
SLPOKE 65536, 1234567
check_cond = 1 = 1
check_num = 14
GOSUB check

PRINT ""
PRINT "================================"
PRINT "Resultats : "
PRINT passed
PRINT " / "
PRINT passed + failed
PRINT " tests OK"
IF passed = 14
  PRINT "*** TOUS LES TESTS OK ***"
ELSE
  PRINT "*** ECHECS ***"
ENDIF
PRINT "=== Tests DATA/Memoire termines ==="
END
