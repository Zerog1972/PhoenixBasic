REM Test AND/OR/NOT sans parentheses
REM Verifie la precedence: AND/OR < comparaisons < +/- < */MOD

REM --- Test 1: AND sans parentheses ---
a = 1
b = 1
c = 0
IF a = 1 AND b = 1
  PRINT "Test 1a (AND): OK"
ELSE
  PRINT "Test 1a (AND): ECHEC"
ENDIF

IF a = 1 AND c = 1
  PRINT "Test 1b (AND false): ECHEC"
ELSE
  PRINT "Test 1b (AND false): OK"
ENDIF

REM --- Test 2: OR sans parentheses ---
IF a = 0 OR b = 1
  PRINT "Test 2a (OR): OK"
ELSE
  PRINT "Test 2a (OR): ECHEC"
ENDIF

IF a = 0 OR c = 1
  PRINT "Test 2b (OR false): ECHEC"
ELSE
  PRINT "Test 2b (OR false): OK"
ENDIF

REM --- Test 3: AND/OR combines sans parentheses ---
REM 1=1 AND 0=1 OR 1=1  => (1=1 AND 0=1) OR 1=1 => 0 OR 1 => TRUE
IF a = 1 AND c = 1 OR a = 1
  PRINT "Test 3 (AND/OR combine): OK"
ELSE
  PRINT "Test 3 (AND/OR combine): ECHEC"
ENDIF

REM --- Test 4: NOT sans parentheses ---
REM NOT a haute precedence: NOT a = 0  => (NOT a) = 0
REM a=1 => NOT 1 = -2, donc (-2 = 0) => FALSE
IF NOT a = 0
  PRINT "Test 4a (NOT a=0 haut prec): ECHEC"
ELSE
  PRINT "Test 4a (NOT a=0 haut prec): OK"
ENDIF

REM NOT (a = 0) avec parentheses => NOT FALSE => TRUE
IF NOT (a = 0)
  PRINT "Test 4b (NOT (a=0)): OK"
ELSE
  PRINT "Test 4b (NOT (a=0)): ECHEC"
ENDIF

REM --- Test 5: AND avec comparaison et calcul ---
REM 2*3 = 6 AND 10-4 = 6  => compare les deux resultats
x = 2 * 3
y = 10 - 4
IF x = 6 AND y = 6
  PRINT "Test 5a (AND avec calcul): OK"
ELSE
  PRINT "Test 5a (AND avec calcul): ECHEC"
ENDIF

REM --- Test 6: XOR sans parentheses ---
IF (1 = 1) XOR (0 = 1)
  PRINT "Test 6a (XOR): OK"
ELSE
  PRINT "Test 6a (XOR): ECHEC"
ENDIF

IF (1 = 1) XOR (1 = 1)
  PRINT "Test 6b (XOR false): ECHEC"
ELSE
  PRINT "Test 6b (XOR false): OK"
ENDIF

REM --- Test 7: Precedence comparaison avant AND ---
REM Ceci doit etre analyse comme (5 > 3) AND (2 < 4)
IF 5 > 3 AND 2 < 4
  PRINT "Test 7 (precedence > AND <): OK"
ELSE
  PRINT "Test 7 (precedence > AND <): ECHEC"
ENDIF

REM --- Test 8: AND avec plusieurs comparaisons ---
REM (10 >= 5) AND (5 <= 10) AND (3 <> 4)
IF 10 >= 5 AND 5 <= 10 AND 3 <> 4
  PRINT "Test 8 (AND multiple): OK"
ELSE
  PRINT "Test 8 (AND multiple): ECHEC"
ENDIF

REM --- Test 9: AND/OR avec NOT ---
REM NOT (c=1) AND a=1  => NOT FALSE AND TRUE => TRUE
IF NOT (c = 1) AND a = 1
  PRINT "Test 9 (NOT AND parens): OK"
ELSE
  PRINT "Test 9 (NOT AND parens): ECHEC"
ENDIF

REM NOT c = 1 => (NOT c) = 1 => -1 = 1 => FALSE (haute precedence)
IF NOT c = 1 AND a = 1
  PRINT "Test 9b (NOT haut prec): ECHEC"
ELSE
  PRINT "Test 9b (NOT haut prec): OK"
ENDIF

REM --- Test 10: EQV sans parentheses ---
REM (1=1) EQV (1=1) => TRUE EQV TRUE => TRUE
IF 1 = 1 EQV 1 = 1
  PRINT "Test 10 (EQV): OK"
ELSE
  PRINT "Test 10 (EQV): ECHEC"
ENDIF

REM --- Test 11: IMP sans parentheses ---
REM (0=1) IMP (1=1) => FALSE IMP TRUE => TRUE
IF 0 = 1 IMP 1 = 1
  PRINT "Test 11 (IMP): OK"
ELSE
  PRINT "Test 11 (IMP): ECHEC"
ENDIF

REM --- Test 12: Expression complexe sans parentheses ---
REM 3 + 4 * 2 > 10 AND 15 - 3 * 2 = 9
REM => 3 + 8 > 10 AND 15 - 6 = 9
REM => 11 > 10 AND 9 = 9
REM => TRUE AND TRUE => TRUE
IF 3 + 4 * 2 > 10 AND 15 - 3 * 2 = 9
  PRINT "Test 12 (expression complexe): OK"
ELSE
  PRINT "Test 12 (expression complexe): ECHEC"
ENDIF

PRINT "=== Tests AND/OR/NOT sans parentheses termines ==="

END
