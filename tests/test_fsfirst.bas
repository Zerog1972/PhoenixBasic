REM test_fsfirst.bas - FSFIRST/FSNEXT/FNAME/FATTR/FPOS/SIZE
REM Enumere des fichiers du repertoire build/ et verifie les fonctions.
a$ = "build/fsf_aa.bas"
b$ = "build/fsf_bb.bas"
c$ = "build/fsf_cc.dat"
KILL a$
KILL b$
KILL c$
OPEN "O", #1, a$
PRINT #1, "x"
CLOSE #1
OPEN "O", #2, b$
PRINT #2, "y"
CLOSE #2
OPEN "O", #3, c$
PRINT #3, "z"
CLOSE #3

REM --- 1. Enumeration .BAS : 2 fichiers ---
n = 0
FSFIRST "build\*.BAS"
IF EOF = 1 THEN
  PRINT "KO1"
  GOTO 900
END IF
DO
  n = n + 1
  nm$ = UPPER$(FNAME)
  IF n = 1 THEN
    IF nm$ <> "FSF_AA.BAS" AND nm$ <> "FSF_BB.BAS" THEN
      PRINT "KO2 " + FNAME
      GOTO 900
    END IF
    IF FPOS <> 1 THEN PRINT "KO3 " + STR$(FPOS) : GOTO 900
  ELSE
    IF nm$ <> "FSF_AA.BAS" AND nm$ <> "FSF_BB.BAS" THEN
      PRINT "KO4 " + FNAME
      GOTO 900
    END IF
    IF FPOS <> 2 THEN PRINT "KO5 " + STR$(FPOS) : GOTO 900
  END IF
  FSNEXT
LOOP UNTIL EOF
IF n <> 2 THEN PRINT "KO6 " + STR$(n) : GOTO 900
IF EOF <> 1 THEN PRINT "KO7" : GOTO 900

REM --- 2. SIZE sur .DAT ---
FSFIRST "build\*.DAT"
IF EOF = 1 THEN PRINT "KO8" : GOTO 900
nm$ = UPPER$(FNAME)
sz = SIZE
IF nm$ <> "FSF_CC.DAT" THEN PRINT "KO9 " + FNAME : GOTO 900
IF sz < 1 THEN PRINT "KO10 " + STR$(sz) : GOTO 900

REM --- 3. FATTR : archive (bit 0) ---
atrv = FATTR
IF (atrv AND 1) <> 1 THEN PRINT "KO11 " + STR$(atrv) : GOTO 900

REM --- 4. Aucun fichier ---
FSFIRST "build\*.NOPE"
IF EOF <> 1 THEN PRINT "KO12" : GOTO 900
IF FNAME <> "" THEN PRINT "KO13" : GOTO 900
IF FPOS <> 0 THEN PRINT "KO14" : GOTO 900
IF SIZE <> 0 THEN PRINT "KO15" : GOTO 900

REM --- 5. FSNEXT sans FSFIRST ---
x = FSNEXT
IF x = 0 THEN PRINT "KO16" : GOTO 900

REM --- Nettoyage ---
KILL a$
KILL b$
KILL c$
PRINT "TOUT OK FSFIRST"
END
900
KILL a$
KILL b$
KILL c$
END
