REM test_arrfill.bas - ARRAYFILL et DIM?
REM (base 0, convention du projet)
passed = 0
failed = 0

DIM t(4)
ARRAYFILL t, 9
s = 0
FOR i = 0 TO 3
s = s + t(i)
NEXT i
IF s = 36 THEN passed = passed + 1 ELSE failed = failed + 1

DIM u(2,3)
ARRAYFILL u, 5
IF u(1,2) = 5 AND u(0,0) = 5 THEN passed = passed + 1 ELSE failed = failed + 1

REM remplissage partiel apres modification
u(0,0) = 99
ARRAYFILL u, 0
IF u(0,0) = 0 AND u(1,2) = 0 THEN passed = passed + 1 ELSE failed = failed + 1

REM valeur flottante
DIM f(3)
ARRAYFILL f, 2.5
IF f(1) = 2.5 THEN passed = passed + 1 ELSE failed = failed + 1

REM DIM? : tableau 1D
DIM a(7)
d1$ = DIM?(a)
IF d1$ = "7" THEN passed = passed + 1 ELSE failed = failed + 1

REM DIM? : tableau 2D
DIM b(2,4)
d2$ = DIM?(b)
IF d2$ = "2,4" THEN passed = passed + 1 ELSE failed = failed + 1

REM DIM? : scalaire non defini -> "0"
d3$ = DIM?(q)
IF d3$ = "0" THEN passed = passed + 1 ELSE failed = failed + 1

IF failed = 0 THEN PRINT "TOUT OK ARRAYFILL" ELSE PRINT "ECHEC ARRAYFILL " + STR$(failed)
END
