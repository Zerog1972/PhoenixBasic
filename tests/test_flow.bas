' ============================================================
' Tests FOR/NEXT, WHILE/WEND, REPEAT/UNTIL, DO/LOOP, EXIT IF,
' SELECT/CASE
' GFA Basic 3.5
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

PRINT ""
PRINT "=== Tests Flow Control ==="
PRINT ""

' ============================================================
' 1. FOR/NEXT simple (comptage croissant)
' ============================================================
sum = 0
FOR i = 1 TO 5
  sum = sum + i
NEXT i
check_cond = sum = 15
check_num = 1
GOSUB check

' ============================================================
' 2. FOR/NEXT avec STEP=2
' ============================================================
sum = 0
FOR i = 2 TO 6 STEP 2
  sum = sum + i
NEXT i
check_cond = sum = 12
check_num = 2
GOSUB check

' ============================================================
' 3. FOR/NEXT avec TO 1 (un seul passage)
' ============================================================
count = 0
FOR i = 1 TO 1
  count = count + 1
NEXT i
check_cond = count = 1
check_num = 3
GOSUB check

' ============================================================
' 4. FOR/NEXT sans iteration (depart > arrivee, pas positif)
' ============================================================
count = 0
FOR i = 5 TO 1
  count = count + 1
NEXT i
check_cond = count = 0
check_num = 4
GOSUB check

' ============================================================
' 5. WHILE/WEND (condition vraie)
' ============================================================
i = 1
sum = 0
WHILE i <= 5
  sum = sum + i
  i = i + 1
WEND
check_cond = sum = 15
check_num = 5
GOSUB check

' ============================================================
' 6. WHILE/WEND (condition fausse des le depart)
' ============================================================
i = 10
count = 0
WHILE i < 5
  count = count + 1
  i = i + 1
WEND
check_cond = count = 0
check_num = 6
GOSUB check

' ============================================================
' 7. REPEAT/UNTIL (s'execute au moins une fois)
' ============================================================
i = 1
sum = 0
REPEAT
  sum = sum + i
  i = i + 1
UNTIL i > 5
check_cond = sum = 15
check_num = 7
GOSUB check

' ============================================================
' 8. REPEAT/UNTIL (condition immediate vraie)
' ============================================================
count = 0
REPEAT
  count = count + 1
UNTIL count = 1
check_cond = count = 1
check_num = 8
GOSUB check

' ============================================================
' 9. SELECT/CASE avec cas exact
' ============================================================
result = 0
x = 2
SELECT x
  CASE 1
    result = 10
  CASE 2
    result = 20
  CASE 3
    result = 30
ENDSELECT
check_cond = result = 20
check_num = 9
GOSUB check

' ============================================================
' 10. SELECT/CASE avec DEFAULT
' ============================================================
result = 0
x = 99
SELECT x
  CASE 1
    result = 10
  CASE 2
    result = 20
  DEFAULT
    result = 99
ENDSELECT
check_cond = result = 99
check_num = 10
GOSUB check

' ============================================================
' 11. FOR imbrique (table de multiplication)
' ============================================================
sum = 0
FOR i = 1 TO 3
  FOR j = 1 TO 4
    sum = sum + i * j
  NEXT j
NEXT i
' sum = 1*1+1*2+1*3+1*4 + 2*1+...+3*4 = 10+20+30 = 60
check_cond = sum = 60
check_num = 11
GOSUB check

' ============================================================
' 12. WHILE imbrique (recherche de paires)
' ============================================================
found = 0
i = 1
WHILE i <= 4 AND found = 0
  j = 1
  WHILE j <= 4 AND found = 0
    IF i * j = 6
      found = i * 10 + j
    ENDIF
    j = j + 1
  WEND
  i = i + 1
WEND
' Premiere paire i*j=6 est i=2,j=3 → found=23
check_cond = found = 23
check_num = 12
GOSUB check

' ============================================================
' 13. REPEAT/UNTIL imbrique avec FOR
' ============================================================
sum = 0
FOR i = 1 TO 3
  j = 1
  REPEAT
    sum = sum + i + j
    j = j + 1
  UNTIL j > 2
NEXT i
' i=1: j=1,2 → sum=1+1+1+2=5
' i=2: j=1,2 → sum=5+2+1+2+2=12
' i=3: j=1,2 → sum=12+3+1+3+2=21
check_cond = sum = 21
check_num = 13
GOSUB check

' ============================================================
' 14. IF imbrique dans FOR
' ============================================================
nb_pairs = 0
FOR i = 1 TO 10
  IF i MOD 2 = 0
    nb_pairs = nb_pairs + 1
  ENDIF
NEXT i
' Pairs de 1 a 10 : 2,4,6,8,10 = 5
check_cond = nb_pairs = 5
check_num = 14
GOSUB check

' ============================================================
' 15. SELECT/CASE dans FOR
' ============================================================
sum_pair = 0
sum_impair = 0
FOR i = 1 TO 5
  SELECT i
    CASE 1
      sum_impair = sum_impair + i
    CASE 2
      sum_pair = sum_pair + i
    CASE 3
      sum_impair = sum_impair + i
    CASE 4
      sum_pair = sum_pair + i
    CASE 5
      sum_impair = sum_impair + i
  ENDSELECT
NEXT i
' Pairs : 2+4=6, Impairs : 1+3+5=9
check_cond = sum_pair = 6 AND sum_impair = 9
check_num = 15
GOSUB check

' ============================================================
' 16. FOR imbrique sur 3 niveaux
' ============================================================
sum = 0
FOR i = 1 TO 2
  FOR j = 1 TO 3
    FOR k = 1 TO 2
      sum = sum + i + j + k
    NEXT k
  NEXT j
NEXT i
' Calcul manuel : pour chaque i,j,k, somme i+j+k
' i=1: j=1→k=1,2: 3+4=7, j=2→k=1,2: 4+5=9, j=3→k=1,2: 5+6=11
'      total i=1 = 7+9+11 = 27
' i=2: j=1→k=1,2: 4+5=9, j=2→k=1,2: 5+6=11, j=3→k=1,2: 6+7=13
'      total i=2 = 9+11+13 = 33
' Total = 27+33 = 60
check_cond = sum = 60
check_num = 16
GOSUB check

' ============================================================
' 17. DO/LOOP WHILE (post-test : s'execute au moins une fois)
' ============================================================
i = 0
DO
  i = i + 1
LOOP WHILE i < 5
check_cond = i = 5
check_num = 17
GOSUB check

' ============================================================
' 18. DO/LOOP UNTIL
' ============================================================
i = 0
DO
  i = i + 1
LOOP UNTIL i >= 5
check_cond = i = 5
check_num = 18
GOSUB check

' ============================================================
' 19. DO/LOOP sans condition (bucle controlee par EXIT IF)
' ============================================================
i = 0
sum = 0
DO
  i = i + 1
  sum = sum + i
  EXIT IF i >= 4
LOOP
check_cond = i = 4 AND sum = 10
check_num = 19
GOSUB check

' ============================================================
' 20. EXIT IF dans WHILE/WEND
' ============================================================
i = 0
WHILE i < 100
  i = i + 1
  EXIT IF i >= 3
WEND
check_cond = i = 3
check_num = 20
GOSUB check

' ============================================================
' 21. EXIT IF dans FOR/NEXT
' ============================================================
sum = 0
FOR i = 1 TO 100
  sum = sum + i
  EXIT IF i >= 4
NEXT i
check_cond = sum = 10
check_num = 21
GOSUB check

' ============================================================
' 22. EXIT ne quitte que la boucle la plus interne
' ============================================================
outer = 0
FOR o = 1 TO 2
  outer = outer + 1
  i = 0
  DO
    i = i + 1
    EXIT IF i >= 3
  LOOP
  outer = outer + i
NEXT o
' o=1: outer=1, boucle interne arrete a i=3, outer=1+3=4
' o=2: outer=4+1=5, i=3, outer=5+3=8
check_cond = outer = 8
check_num = 22
GOSUB check

' ============================================================
' Resume
' ============================================================
PRINT ""
PRINT "================================"
PRINT "Resultats : "
PRINT passed
PRINT " / "
PRINT passed + failed
PRINT " tests OK"

IF failed > 0
  PRINT "*** ECHECS : "
  PRINT failed
  PRINT " ***"
ELSE
  PRINT "*** TOUS LES TESTS OK ***"
ENDIF

END
