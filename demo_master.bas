' ============================================================
' DEMO_MASTER.BAS — Démo PhoenixBasic (GFA Basic 3.5)
' ============================================================
' Syntaxes 100% validées par les 440+ tests.
' Nouveau : appels PROCEDURE sans parentheses !
' ============================================================

DIM scores(10)

DATA "Un programmeur contourne des bugs."
DATA "Le debug, c'est etre un detective."
DATA "10 types de personnes : le binaire."
RESTORE

' ============================================================
' MENU PRINCIPAL
' ============================================================

PROCEDURE menu_principal
  LOCAL choix

  REPEAT
    CLS
    COLOR 3
    PRINT "=============================="
    PRINT "  PHOENIXBASIC — MASTER DEMO"
    PRINT "=============================="
    COLOR 2
    PRINT ""
    PRINT "  1 — Tours de Hanoi (recursif)"
    PRINT "  2 — Maths et scientifique"
    PRINT "  3 — Manipulation de chaines"
    PRINT "  4 — Fichiers"
    PRINT "  5 — Sons"
    PRINT "  6 — Quitter"
    PRINT ""
    INPUT choix

    IF choix = 1
      GOSUB jeu_hanoi
    ELSE
      IF choix = 2
        GOSUB demo_maths
      ELSE
        IF choix = 3
          GOSUB demo_chaines
        ELSE
          IF choix = 4
            GOSUB demo_fichiers
          ELSE
            IF choix = 5
              GOSUB demo_sons
            ELSE
              IF choix = 6
                PRINT "Au revoir !"
              ELSE
                COLOR 4
                PRINT "Invalide !"
                BEEP
              ENDIF
            ENDIF
          ENDIF
        ENDIF
      ENDIF
    ENDIF

    IF choix <> 6
      PRINT ""
      PRINT "ENTREE pour continuer..."
      INPUT attente$
    ENDIF
  UNTIL choix = 6
RETURN

' ============================================================
' 1. TOURS DE HANOÏ (récursif, appels nus)
' ============================================================

PROCEDURE hanoi_r(n, depart, arrivee, inter)
  IF n > 0
    hanoi_r n - 1, depart, inter, arrivee
    total = total + 1
    PRINT "Disque ";
    PRINT n;
    PRINT " : ";
    PRINT depart;
    PRINT " -> ";
    PRINT arrivee
    hanoi_r n - 1, inter, arrivee, depart
  ENDIF
RETURN

PROCEDURE jeu_hanoi
  LOCAL disques

  CLS
  COLOR 2
  PRINT "=== TOURS DE HANOÏ (RECURSIF) ==="
  PRINT ""
  PRINT "Disques (1-6) : ";
  INPUT disques

  IF disques < 1 OR disques > 6
    PRINT "Entre 1 et 6"
    RETURN
  ENDIF

  total = 0
  PRINT "Resolution :"
  PRINT ""

  hanoi_r disques, 1, 3, 2

  PRINT ""
  PRINT "Total : ";
  PRINT total;
  PRINT " mouvements (minimum : ";
  PRINT (2 ^ disques) - 1;
  PRINT ")"
  BEEP
RETURN

' ============================================================
' 2. DÉMO MATHS
' ============================================================

PROCEDURE demo_maths
  LOCAL x, i, s

  CLS
  COLOR 2
  PRINT "=== MATHS ET SCIENTIFIQUE ==="
  PRINT ""

  PRINT "--- Fonctions unaires ---"
  PRINT "SIN(0) = ";
  PRINT SIN(0)
  PRINT "COS(0) = ";
  PRINT COS(0)
  PRINT "TAN(0) = ";
  PRINT TAN(0)
  PRINT "EXP(0) = ";
  PRINT EXP(0)
  PRINT "LOG(1) = ";
  PRINT LOG(1)
  PRINT "SQR(16) = ";
  PRINT SQR(16)
  PRINT "ABS(-5) = ";
  PRINT ABS(-5)
  PRINT "INT(3.7) = ";
  PRINT INT(3.7)
  PRINT "FRAC(3.7) = ";
  PRINT FRAC(3.7)
  PRINT "ROUND(3.5) = ";
  PRINT ROUND(3.5)
  PRINT "SGN(-5) = ";
  PRINT SGN(-5)
  PRINT "FACT(5) = ";
  PRINT FACT(5)

  PRINT ""
  PRINT "--- Fonctions binaires ---"
  PRINT "MIN(3,7) = ";
  PRINT MIN(3, 7)
  PRINT "MAX(3,7) = ";
  PRINT MAX(3, 7)

  PRINT ""
  PRINT "--- Boucle FOR/NEXT ---"
  s = 0
  FOR i = 1 TO 10
    s = s + i
  NEXT i
  PRINT "Somme 1..10 = ";
  PRINT s

  PRINT ""
  PRINT "--- Boucle WHILE/WEND ---"
  i = 1
  s = 0
  WHILE i <= 5
    s = s + i
    i = i + 1
  WEND
  PRINT "Somme 1..5 = ";
  PRINT s

  PRINT ""
  PRINT "--- Tests AND/OR/NOT ---"
  x = 5
  IF x > 3 AND x < 10
    PRINT x;
    PRINT " est entre 3 et 10 : OK"
  ENDIF
  IF NOT (x = 0)
    PRINT "NOT(x=0) est VRAI : OK"
  ENDIF
  IF x < 3 OR x > 4
    PRINT x;
    PRINT " est > 4 (OR) : OK"
  ENDIF

  PRINT ""
  PRINT "--- DATA/READ/RESTORE ---"
  RESTORE
  FOR i = 1 TO 3
    READ citation$
    PRINT i;
    PRINT ". ";
    PRINT citation$
  NEXT i

  BEEP
RETURN

' ============================================================
' 3. DÉMO CHAÎNES
' ============================================================

PROCEDURE demo_chaines
  LOCAL t$, i, p, m$

  CLS
  COLOR 2
  PRINT "=== MANIPULATION DE CHAINES ==="
  PRINT ""

  t$ = "  GFA Basic sur Atari ST  "
  PRINT "Original : [";
  PRINT t$;
  PRINT "] (";
  PRINT LEN(t$);
  PRINT " cars)"

  t$ = TRIM$(t$)
  PRINT "TRIM$ : [";
  PRINT t$;
  PRINT "] (";
  PRINT LEN(t$);
  PRINT " cars)"

  PRINT ""
  PRINT "UPPER$ : ";
  PRINT UPPER$(t$)
  PRINT "LCASE$ : ";
  PRINT LCASE$(t$)

  PRINT ""
  PRINT "LEFT$(8) : [";
  PRINT LEFT$(t$, 8);
  PRINT "]"
  PRINT "RIGHT$(5) : [";
  PRINT RIGHT$(t$, 5);
  PRINT "]"
  PRINT "MID$(5,4) : [";
  PRINT MID$(t$, 5, 4);
  PRINT "]"

  p = INSTR(t$, "Basic")
  PRINT "INSTR('Basic') = ";
  PRINT p

  PRINT ""
  PRINT "STR$(123.456) = [";
  PRINT STR$(123.456);
  PRINT "]"
  PRINT "VAL('3.14') = ";
  PRINT VAL("3.14")

  PRINT ""
  PRINT "CHR$(65)='";
  PRINT CHR$(65);
  PRINT "'  ASC('A')=";
  PRINT ASC("A")

  PRINT ""
  PRINT "SPACE$(8) : [";
  PRINT SPACE$(8);
  PRINT "]"
  PRINT "STRING$(5,'*') : [";
  PRINT STRING$(5, "*");
  PRINT "]"

  PRINT ""
  PRINT "BIN$(5) = ";
  PRINT BIN$(5)
  PRINT "HEX$(255) = ";
  PRINT HEX$(255)

  PRINT ""
  PRINT "--- Decoupage avec WHILE ---"
  t$ = "GFA Basic Atari ST"
  PRINT "Phrase : ";
  PRINT t$
  p = INSTR(t$, " ")
  WHILE p > 0
    m$ = LEFT$(t$, p - 1)
    PRINT "  [";
    PRINT m$;
    PRINT "]"
    t$ = MID$(t$, p + 1, LEN(t$) - p)
    p = INSTR(t$, " ")
  WEND
  IF LEN(t$) > 0
    PRINT "  [";
    PRINT t$;
    PRINT "]"
  ENDIF

  BEEP
RETURN

' ============================================================
' 4. DÉMO FICHIERS
' ============================================================

PROCEDURE demo_fichiers
  LOCAL a$, a, b

  CLS
  COLOR 2
  PRINT "=== FICHIERS ==="
  PRINT ""

  PRINT "Ecriture dans demo_test.tmp..."
  OPEN "O", #1, "demo_test.tmp"
  PRINT #1, "Hello depuis PhoenixBasic !"
  PRINT #1, 12345
  PRINT #1, 3.14159
  CLOSE #1
  PRINT "OK : fichier ecrit"
  PRINT ""

  PRINT "Lecture de demo_test.tmp..."
  OPEN "I", #1, "demo_test.tmp"
  INPUT #1, a$
  PRINT "Chaine : ";
  PRINT a$
  INPUT #1, a
  PRINT "Entier : ";
  PRINT a
  INPUT #1, b
  PRINT "Reel :   ";
  PRINT b
  CLOSE #1
  PRINT "OK : fichier lu"
  PRINT ""

  IF a$ = "Hello depuis PhoenixBasic !"
    PRINT "Verification : OK"
  ELSE
    PRINT "Verification : ECHEC"
  ENDIF

  BEEP
RETURN

' ============================================================
' 5. DÉMO SONS
' ============================================================

PROCEDURE demo_sons
  CLS
  COLOR 2
  PRINT "=== SONS ==="
  PRINT ""

  PRINT "BEEP..."
  BEEP

  PRINT ""
  PRINT "Gamme :"
  SOUND 0, 262, 15, 12, 1
  SOUND 0, 294, 15, 12, 1
  SOUND 0, 330, 15, 12, 1
  SOUND 0, 349, 15, 12, 1
  SOUND 0, 392, 15, 12, 1
  SOUND 0, 440, 15, 12, 1
  SOUND 0, 494, 15, 12, 1
  SOUND 0, 523, 30, 14, 2

  PRINT ""
  PRINT "Arpege :"
  SOUND 0, 523, 8, 10, 1
  SOUND 0, 659, 8, 10, 1
  SOUND 0, 784, 8, 10, 1
  SOUND 0, 1047, 20, 14, 2

  BEEP
RETURN

' ============================================================
' DÉMARRAGE
' ============================================================

CLS
COLOR 3
PRINT "=============================="
PRINT "  PHOENIXBASIC — DEMONSTRATION"
PRINT "=============================="
COLOR 2
PRINT ""
PRINT "NOUVEAU : appels PROCEDURE sans parentheses !"
PRINT "Exemple : hanoi_r 3, 1, 3, 2"
PRINT ""
PRINT "Fonctionnalites demontees :"
PRINT "  - PROCEDURE recursive avec arguments nus"
PRINT "  - FOR/NEXT/STEP, WHILE/WEND, REPEAT/UNTIL"
PRINT "  - IF/THEN/ELSE/ENDIF"
PRINT "  - AND, OR, NOT sans parentheses"
PRINT "  - 35 fonctions mathematiques"
PRINT "  - 18 fonctions chaines"
PRINT "  - Fichiers OPEN/CLOSE/PRINT#/INPUT#"
PRINT "  - DATA/READ/RESTORE"
PRINT "  - BEEP, SOUND"
PRINT ""
PRINT "ENTREE pour commencer..."
INPUT attente$

GOSUB menu_principal

CLS
COLOR 2
PRINT "Merci d'avoir utilise PhoenixBasic !"
END
