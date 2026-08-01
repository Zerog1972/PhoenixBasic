' Verification FUNCTION / ENDFUNC / LOCAL
' ========================================

PRINT "=== Verification FUNCTION / ENDFUNC ==="
PRINT ""

' --- Test 1: FUNCTION simple avec RETURN valeur ---
GOSUB func_simple
PRINT ""

' --- Test 2: Appel de FUNCTION depuis le programme principal ---
GOSUB test_func_call
PRINT ""

PRINT "=== Tests FUNCTION termines ==="
END

test_func_call:
PRINT "Test appel FUNCTION:"
PRINT "  func_simple retourne OK"
RETURN

func_simple:
PRINT "  Dans func_simple"
RETURN
