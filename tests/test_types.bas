' ============================================================
' Tests types de variables — GFA Basic 3.5 (PhoenixBasic)
' Flottant (defaut/#), Long (%), Word (&), Byte (|), Bool (!),
' Chaine ($), tableaux types, littéraux typés, conversions.
' ============================================================

passed = 0
failed = 0
check_cond = 0
check_num = 0

PROCEDURE check
  IF check_cond
    passed = passed + 1
  ELSE
    failed = failed + 1
    PRINT "ECHEC"
    PRINT check_num
  ENDIF
RETURN

PRINT "=== Tests types de variables ==="
PRINT ""

' --- 1-4 : Flottant (defaut et #) ---
f = 3.14
check_cond = ABS(f - 3.14) < 0.000001
check_num = 1
GOSUB check

f = -7.25
check_cond = ABS(f + 7.25) < 0.000001
check_num = 2
GOSUB check

h# = 2.5
check_cond = ABS(h# - 2.5) < 0.000001
check_num = 3
GOSUB check

k = 2.5#
check_cond = ABS(k - 2.5) < 0.000001
check_num = 4
GOSUB check

' --- 5-9 : Long (%) ---
a% = 2000000000
check_cond = a% = 2000000000
check_num = 5
GOSUB check

a% = -2000000000
check_cond = a% = -2000000000
check_num = 6
GOSUB check

a% = 100%
check_cond = a% = 100
check_num = 7
GOSUB check

a% = 1.9
check_cond = a% = 1
check_num = 8
GOSUB check

a% = -1.9
check_cond = a% = -1
check_num = 9
GOSUB check

' --- 10-19 : Word (&) ---
w& = 300
check_cond = w& = 300
check_num = 10
GOSUB check

w& = -300
check_cond = w& = -300
check_num = 11
GOSUB check

w& = 32767
check_cond = w& = 32767
check_num = 12
GOSUB check

w& = 32768
check_cond = w& = -32768
check_num = 13
GOSUB check

w& = -32768
check_cond = w& = -32768
check_num = 14
GOSUB check

w& = 70000
check_cond = w& = 4464
check_num = 15
GOSUB check

w& = 60000
check_cond = w& = -5536
check_num = 16
GOSUB check

w& = -70000
check_cond = w& = -4464
check_num = 17
GOSUB check

w& = 70000&
check_cond = w& = 4464
check_num = 18
GOSUB check

w& = 20000
w& = w& + 15000
check_cond = w& = -30536
check_num = 19
GOSUB check

' --- 20-27 : Byte (|) ---
b| = 200
check_cond = b| = 200
check_num = 20
GOSUB check

b| = 0
check_cond = b| = 0
check_num = 21
GOSUB check

b| = 255
check_cond = b| = 255
check_num = 22
GOSUB check

b| = 300
check_cond = b| = 44
check_num = 23
GOSUB check

b| = 256
check_cond = b| = 0
check_num = 24
GOSUB check

b| = -1
check_cond = b| = 255
check_num = 25
GOSUB check

b| = -300
check_cond = b| = 212
check_num = 26
GOSUB check

b| = 300|
check_cond = b| = 44
check_num = 27
GOSUB check

' --- 28-31 : Bool (!) ---
bl! = (5 > 3)
check_cond = bl! = -1
check_num = 28
GOSUB check

bl! = (2 > 3)
check_cond = bl! = 0
check_num = 29
GOSUB check

bl! = 0
check_cond = bl! = 0
check_num = 30
GOSUB check

t = 0
bl! = (1 >= 1)
IF bl!
  t = 1
ENDIF
check_cond = t = 1
check_num = 31
GOSUB check

' --- 32-35 : Chaine ($) et concaténation & ---
s$ = "abc"
check_cond = LEN(s$) = 3
check_num = 32
GOSUB check

s$ = 123$
check_cond = s$ = "123"
check_num = 33
GOSUB check

s$ = "ab"
s2$ = s$ & "cd"
check_cond = s2$ = "abcd"
check_num = 34
GOSUB check

u$ = "x"
v$ = "y"
s3$ = u$ & v$
check_cond = s3$ = "xy"
check_num = 35
GOSUB check

' --- 36-41 : Tableaux typés ---
DIM z%(10)
z%(3) = 500
check_cond = z%(3) = 500
check_num = 36
GOSUB check

z%(3) = 3000000000
check_cond = z%(3) = 3000000000
check_num = 37
GOSUB check

DIM r&(6)
r&(1) = 40000
check_cond = r&(1) = -25536
check_num = 38
GOSUB check

r&(2) = 70000
check_cond = r&(2) = 4464
check_num = 39
GOSUB check

DIM c|(8)
c|(4) = 300
check_cond = c|(4) = 44
check_num = 40
GOSUB check

DIM f2(5)
f2(1) = 3.5
check_cond = ABS(f2(1) - 3.5) < 0.000001
check_num = 41
GOSUB check

' --- 42 : Conversion word -> chaine ---
w& = -5536
s$ = STR$(w&)
check_cond = s$ = "-5536"
check_num = 42
GOSUB check

PRINT ""
PRINT "================================"
PRINT "Resultats : "
PRINT passed
PRINT " / "
PRINT passed + failed
PRINT " tests OK"
IF passed = passed + failed
  PRINT "*** TOUS LES TESTS TYPES OK ***"
ELSE
  PRINT "*** ECHECS ***"
ENDIF
PRINT "=== Tests types de variables termines ==="
END
