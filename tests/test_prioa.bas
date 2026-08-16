' ============================================================
' test_prioa.bas - Tests Priorite A (fonctions runtime GFA)
'   ==, VAL?, DIR$, DFREE, TYPE, PAUSE, DELAY, RANDOMIZE,
'   TIMER, DATE$/TIME$, EXIST, ~, BYTE{}/CARD{}/WORD{}/
'   LONG{}/SINGLE{}/DOUBLE{}, PEEK/POKE (vmem), KEY*, HIMEM,
'   GEMDOS()/BIOS()/XBIOS()
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

PRINT "=== Tests Priorite A ==="
PRINT ""

' --- 1. == (egalite approximative) : vrai ---
x = 1.00000000001
y = 1.0
check_cond = (x == y)
check_num = 1
GOSUB check

' --- 2. = strict : faux sur la meme valeur (x <> y) ---
check_cond = (x <> y)
check_num = 2
GOSUB check

' --- 3. PEEK/POKE round-trip ---
POKE 100, 42
check_cond = (PEEK(100) = 42)
check_num = 3
GOSUB check

' --- 4. DPEEK/DPOKE round-trip (big-endian) ---
DPOKE 200, 1234
check_cond = (DPEEK(200) = 1234)
check_num = 4
GOSUB check

' --- 5. LPEEK/LPOKE round-trip ---
LPOKE 300, 123456789
check_cond = (LPEEK(300) = 123456789)
check_num = 5
GOSUB check

' --- 6. SPOKE (variante GFA de POKE) ---
SPOKE 400, 200
check_cond = (PEEK(400) = 200)
check_num = 6
GOSUB check

' --- 7. SDPOKE / SLPOKE ---
SDPOKE 500, 65535
SLPOKE 600, -1
check_cond = (DPEEK(500) = 65535) AND (LPEEK(600) = -1)
check_num = 7
GOSUB check

' --- 8. BYTE{} lit un octet (LPOKE 300 = 123456789 = 0x075BCD15) ---
check_cond = (BYTE{300} = 7) AND (BYTE{303} = 21)
check_num = 8
GOSUB check

' --- 9. CARD{} lit un mot non signe ---
check_cond = (CARD{300} = 1883) AND (CARD{302} = 52501)
check_num = 9
GOSUB check

' --- 10. WORD{} lit un mot signe ---
check_cond = (WORD{300} = 1883) AND (WORD{302} = -13035)
check_num = 10
GOSUB check

' --- 11. LONG{} lit un long signe ---
check_cond = LONG{300} = 123456789
check_num = 11
GOSUB check

' --- 12. LONG{} / BYTE{} sur valeur negative (-1) ---
check_cond = (LONG{600} = -1) AND (BYTE{600} = 255) AND (WORD{600} = -1)
check_num = 12
GOSUB check

' --- 13. SINGLE{} lit un float IEEE 754 (1.0 = 0x3F800000) ---
POKE 700, 63
POKE 701, 128
POKE 702, 0
POKE 703, 0
check_cond = SINGLE{700} = 1.0
check_num = 13
GOSUB check

' --- 14. DOUBLE{} lit un double IEEE 754 (1.0 = 0x3FF0...) ---
POKE 800, 63
POKE 801, 240
POKE 802, 0
POKE 803, 0
POKE 804, 0
POKE 805, 0
POKE 806, 0
POKE 807, 0
check_cond = DOUBLE{800} = 1.0
check_num = 14
GOSUB check

' --- 15. Wrap d'adresse (16 Mo sur hote) ---
POKE 0, 77
check_cond = PEEK(16777216) = 77
check_num = 15
GOSUB check

' --- 16. VAL? compte les caracteres convertibles ---
check_cond = VAL?("123abc") = 3
check_num = 16
GOSUB check

' --- 17. DIR$ trouve un fichier connu ---
d$ = DIR$("Makefile")
check_cond = d$ = "Makefile"
check_num = 17
GOSUB check

' --- 18. DFREE renvoie un nombre ---
df = DFREE()
check_cond = df >= 0
check_num = 18
GOSUB check

' --- 19. TYPE : 0 = numerique, 1 = chaine ---
check_cond = (TYPE(1) = 0) AND (TYPE("abc") = 1)
check_num = 19
GOSUB check

' --- 20. EXIST sur fichier present ---
check_cond = EXIST("Makefile") = -1
check_num = 20
GOSUB check

' --- 21. EXIST sur fichier absent ---
check_cond = EXIST("zzz_neexiste_pas_zzz.bas") = 0
check_num = 21
GOSUB check

' --- 22. ~ (NOT bit a bit) ---
check_cond = ~5 = -6
check_num = 22
GOSUB check

' --- 23. TIMER >= 0 ---
check_cond = TIMER() >= 0
check_num = 23
GOSUB check

' --- 24. DATE$ / TIME$ non vides ---
dd$ = DATE$()
tt$ = TIME$()
check_cond = LEN(dd$) >= 8
check_num = 24
GOSUB check

check_cond = LEN(tt$) = 8
check_num = 25
GOSUB check

' --- 26. KEYPRESS -> KEYTEST ---
KEYPRESS(65)
check_cond = KEYTEST() = 1
check_num = 26
GOSUB check

' --- 27. KEYLOOK renvoie la touche en tete ---
check_cond = KEYLOOK() = 65
check_num = 27
GOSUB check

' --- 28. KEYPAD test la touche donnee ---
check_cond = (KEYPAD(65) = 1) AND (KEYPAD(66) = 0)
check_num = 28
GOSUB check

' --- 29. KEYGET consomme la touche ---
k = KEYGET()
check_cond = (k = 65) AND (KEYTEST() = 0)
check_num = 29
GOSUB check

' --- 30. PAUSE 0 renvoie immediatement (0 = timeout instantane) ---
k = PAUSE(0)
check_cond = k = 0
check_num = 30
GOSUB check

' --- 31. PAUSE consomme le tampon emule ---
KEYPRESS(72)
k = PAUSE(100)
check_cond = k = 72
check_num = 31
GOSUB check

' --- 32. DELAY (court, non bloquant ici) ---
DELAY(10)
check_cond = 1
check_num = 32
GOSUB check

' --- 33. RANDOMIZE + RND dans [0,1[ ---
RANDOMIZE 12345
r = RND(1)
check_cond = (r >= 0) AND (r < 1)
check_num = 33
GOSUB check

' --- 34. HIMEM = taille de la region emulee ---
check_cond = HIMEM() = 16777216
check_num = 34
GOSUB check

' --- 35. STE? / TT? = 0 (pas de TT/Falcon) ---
check_cond = (STE?() = 0) AND (TT?() = 0)
check_num = 35
GOSUB check

' --- 36. OB_X/OB_Y/OB_W/OB_H = 0 (pas d'AES) ---
check_cond = (OB_X(0) = 0) AND (OB_Y(0) = 0) AND (OB_W(0) = 0) AND (OB_H(0) = 0)
check_num = 36
GOSUB check

' --- 37. _C/_X/_Y (etat graphique) ---
c = _C()
check_cond = c >= 0
check_num = 37
GOSUB check

' --- 38. VOID / INPUT$(0) ---
check_cond = INPUT$(0) = ""
check_num = 38
GOSUB check

' --- 39. MOUSE/STICK = 0 (pas de peripherique en console) ---
check_cond = (MOUSEX() = 0) AND (MOUSEY() = 0) AND (STICK(0) = 0)
check_num = 39
GOSUB check

' --- 40. INPMID$ (substrate depuis position) ---
s = INPMID$("hello world", "world")
check_cond = s = "world"
check_num = 40
GOSUB check

' --- 41. GEMDOS() cablée (CCONOUT renvoie 0) ---
g = GEMDOS(2, 65, 0)
check_cond = g = 0
check_num = 41
GOSUB check

' --- 42. BIOS()/XBIOS() cablées (retour numérique) ---
b = BIOS(0)
x = XBIOS(0)
check_cond = (b = 0) AND (x = 0)
check_num = 42
GOSUB check

' --- Resume ---
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
