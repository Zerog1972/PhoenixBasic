REM test_chain.bas - SYSTEM, EXEC et CHAIN
REM SYSTEM/EXEC : execute une commande shell et continue.
REM CHAIN "prog" : execute un autre programme (le remplace).

tgt$ = "build/chaintest_tgt.bas"

REM --- Nettoyer un eventuel residu d'un crash precedent ---
KILL tgt$

REM --- Creer le programme cible (s'autodetruit en fin) ---
OPEN "O", #1, tgt$
PRINT #1, "PRINT ""CIBLE_OK"""
PRINT #1, "KILL """ + tgt$ + """"
PRINT #1, "END"
CLOSE #1

REM --- SYSTEM : la commande s'execute, le programme continue ---
SYSTEM "echo SYST_OK"

REM --- EXEC : idem, continue le programme ---
EXEC "echo EXEC_OK"

REM --- CHAIN : remplace le programme courant par le cible.
REM     Les instructions apres CHAIN ne doivent PAS s'executer.
CHAIN tgt$
PRINT "NE_DOIT_PAS_Passer"
END
