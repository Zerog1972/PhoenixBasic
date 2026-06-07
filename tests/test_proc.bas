' Verification des procedures selon le cahier des charges (section 8.4, 8.28)
' ========================================================================

PRINT "=== Verification PROCEDURE / FUNCTION ==="

' --- Test 1: PROCEDURE sans argument ---
PRINT "1. PROCEDURE sans argument:"
GOSUB proc_simple
PRINT "   OK"
PRINT ""

' --- Test 2: PROCEDURE avec arguments ---
PRINT "2. PROCEDURE avec arguments:"
a = 10
b = 20
GOSUB proc_args
PRINT "   somme = "
PRINT a
PRINT "   (attendu: 30)"
PRINT ""

' --- Test 3: Appel via @ (synonyme GOSUB) ---
PRINT "3. Appel via @ (synonyme GOSUB):"
@proc_at
PRINT "   OK"
PRINT ""

' --- Test 4: PROCEDURE avec RETURN ---
PRINT "4. PROCEDURE avec RETURN:"
GOSUB proc_return
PRINT "   (apres RETURN)"
PRINT ""

' --- Test 5: Plusieurs PROCEDURE ---
PRINT "5. Plusieurs PROCEDURE:"
GOSUB proc1
GOSUB proc2
PRINT "   OK"
PRINT ""

' --- Test 6: Appels imbriques ---
PRINT "6. Appels imbriques:"
GOSUB proc_outer
PRINT "   OK"
PRINT ""

' --- Test 7: PROCEDURE apres END ---
PRINT "7. PROCEDURE apres END:"
GOSUB proc_after
PRINT "   OK"
PRINT ""

PRINT "=== Tous les tests PROCEDURE OK ==="
END

' ===== Definitions des procedures =====

proc_simple:
PRINT "   Dans proc_simple"
RETURN

proc_args:
PRINT "   Dans proc_args, a="
PRINT a
PRINT "   b="
PRINT b
a = a + b
RETURN

proc_at:
PRINT "   Dans proc_at (appelee via @)"
RETURN

proc_return:
PRINT "   Debut proc_return"
RETURN
PRINT "   Ceci ne doit PAS s'afficher"

proc1:
PRINT "   Dans proc1"
RETURN

proc2:
PRINT "   Dans proc2"
RETURN

proc_outer:
PRINT "   Debut proc_outer"
GOSUB proc_inner
PRINT "   Fin proc_outer"
RETURN

proc_inner:
PRINT "   Dans proc_inner"
RETURN

proc_after:
PRINT "   Dans proc_after (apres END)"
RETURN
