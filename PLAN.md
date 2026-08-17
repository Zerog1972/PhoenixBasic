# Plan de Développement — PhoenixBasic

> **Dernière mise à jour : 2026-08-15**
>
> Analyse complète des fonctionnalités GFA Basic 3.5 implémentées vs manquantes,
> avec plan de travail priorisé.

---

## Vue d'ensemble

```
make test-all : ✅ 369+ tests, 100%
```

PhoenixBasic est un interpréteur **GFA Basic 3.5** (langage BASIC historique de l'Atari ST) écrit en C ANSI (C89). Le code traverse tout le pipeline : **tokenisation → parsing → AST → codegen → bytecode → runtime**.

| Priorité | Nb items | Effort estimé | Description |
|----------|----------|---------------|-------------|
| **✅ Déjà implémenté** | ~150 | — | Flux, procédures, maths, chaînes, bits, fichiers, graphismes, TOS partiel, vmem (PEEK/POKE réels) |
| **✅ Priorité A** | 22/22 ✅ | fait | Runtime `case` : VAL?, INPUT$, PAUSE, MOUSE, TIMER, DATE$/TIME$, `==`, RAND(n), PEEK/POKE via vmem… |
| **🟠 Priorité B** | 35 | ~5-7 jours | Parser + codegen + runtime : graphismes avancés (PLOT, DRAW, TEXT, POLYLINE, FILL, BITBLT…), SETTIME, QSORT, INSERT/DELETE |
| **🟢 Priorité C** | 10 | ~4-8 semaines | Nouveaux sous-systèmes : GEM AES, VDI, MAT, FIELD, SHEL, CHAIN |

**Total restant : 45 items** (Priorité A terminée le 2026-08-15)

---

## ✅ Déjà implémenté

### Contrôle de flux
IF/THEN/ELSE/ENDIF, FOR/NEXT/STEP, WHILE/WEND, REPEAT/UNTIL, DO/LOOP [WHILE|UNTIL], EXIT IF, SELECT/CASE/ENDSELECT, GOTO, GOSUB/RETURN, ON GOTO/GOSUB, STOP, END, QUIT

> `FOR … DOWNTO` : implémentée (flag de sens dans l'AST, pas -1 par défaut).
> `QUIT [n]` : implémentée (code de sortie optionnel, `OP_QUIT`).

### Procédures/fonctions
PROCEDURE/FUNCTION, RETURN expr, ENDFUNC, LOCAL, VAR (by-ref), DEFFN/FN, récursion, appels sans parenthèses (`maProc 3, 7`)

### Opérateurs
+, -, *, /, ^, =, <>, <, >, <=, >=, AND, OR, XOR, NOT, EQV, IMP, MOD, DIV, & (concaténation)

### PRINT/INPUT
PRINT, PRINT # (fichier), PRINT AT(x,y), PRINT USING (format #, ##.##, **, $$, +, -, ^^^^, virgules), INPUT, INPUT #, INKEY$, CLS, LOCATE

> `LINE INPUT` (console et `#canal`), `HTAB`, `VTAB` : implémentés.

### Fonctions mathématiques (35)
SIN, COS, TAN, ATN, ASIN, ACOS, SINQ, COSQ, SINH, COSH, TANH, EXP, LOG, LOG10, SQR, ABS, SGN, INT, FRAC, FIX, ROUND, CEIL, TRUNC, MIN, MAX, EVEN, ODD, PRED, SUCC, FACT, COMBIN, VARIAT, RND, DEG, RAD, CFLOAT, CINT

### Fonctions chaînes (28)
LEN, ASC, CHR$, VAL, VAL?, LEFT$, RIGHT$, MID$, INSTR, RINSTR, UPPER$, LCASE$, LOWER$, TRIM$, STR$, BIN$, HEX$, OCT$, SPACE$, STRING$, MKI$, MKL$, MKS$, MKF$, MKD$, CVI, CVL, CVS, CVF, CVD

### Opérations sur les bits
BTST, BSET, BCLR, BCHG, SHL, SHR, ROL, ROR

### Tableaux 1D
DIM, OPTION BASE 0/1 (base des indices des tableaux DIM ultérieurs)

> `ERASE`, `ARRAYFILL`, `DIM?` : implémentés (2026-08-16, cf. commits types/arrays).

### Mémoire
PEEK, POKE, DPEEK, DPOKE, LPEEK, LPOKE (via vmem), MALLOC (partiel), DATA, READ, RESTORE

> `MFREE` (vmem_free), `BLOAD`, `BGET`, `BPUT`, `BSAVE` : implémentés sur vmem.
> `BLOAD "f", addr` (addr 0 = $8000) et `BSAVE "f", debut, fin` copient des
> octets entre vmem et fichiers (tampon 64 Ko max).

### Fichiers
OPEN/CLOSE (I/O/R/A/U), OPENW/CLOSEW, PRINT#, INPUT#, BLOAD, BSAVE, BGET, BPUT

### Graphismes SDL2
COLOR (palette 16 couleurs), LINE, BOX, PBOX, CIRCLE, PCIRCLE

### Événements
EVERY, AFTER, ON ERROR, ERROR, ERR

> `ON BREAK` : parsé, sans effet (no-op au codegen). Limitation acceptée :
> pas de touche Break en environnement headless (Ctrl+C = SIGINT/signal
> console, non lisible comme touche).

### Son
BEEP, SOUND ch, freq, dur, vol, env

### TOS partiel
GEMDOS (~15 fonctions : Fopen, Fclose, Fread, Fwrite, Fdelete, Fseek, Malloc, Mfree, Cconin, Cconout, Cconws, Cnecin, Dgetdrv, Dsetdrv, Super, Fversion, Pterm), BIOS (Bconin, Bconout), XBIOS (Getrez, Physbase, Random, Gettime, etc.)

> **Fonctions BASIC `GEMDOS()`, `BIOS()`, `XBIOS()`** : câblées depuis le 2026-08-15
> (opcodes `OP_GEMDOS/BIOS/XBIOS` émis par le codegen ; avant, elles chutaient dans le
> default du `OP_CALL_BUILTIN`). La couche C `gfa_gemdos/gfa_bios/gfa_xbios` est testée
> dans `tests/test_gfx.c`.

### Définitions (no-op)
DEFFILL, DEFLINE, DEFTEXT, DEFMOUSE, DEFMARK

### Debug
TRON, TROFF

### REPL/Éditeur
LIST (avec indentation), EDIT (curseur ← → Home End Bksp Del), DELETE, INSERT, LOAD/SAVE, RUN, NEW, CLS, format_line

---

## ✅ Priorité A — Quick Wins (22 items) — TERMINÉ le 2026-08-15

> **Statut : 22/22 implémentés et testés** (`tests/test_prioa.bas` = 40/40 OK).
> Les 22 items traversaient déjà le pipeline (token → parser → AST → codegen) ;
> il a fallu ajouter les `case` runtime + quelques correctifs transverses ci-dessous.

### Correctifs transverses apportés pendant la Priorité A

1. **Bug `==` (approximate equality)** : le codegen générait `OP_ADD` pour `TOK_APPROX_EQ`.
   Corrigé : `OP_APPROX_EQ` (`fabs(a-b)<1e-10`) dans `codegen.c` + `runtime.c`.
2. **Pile de la VM** : les appels-statement laissaient leur résultat sur la pile, ce qui
   faisait diverger la détection du nb d'arguments par `sp` dans les builtins.
   Correctifs : (a) le codegen émet `OP_POP` après un builtin en position statement ;
   (b) le codegen passe le nb d'arguments dans `operand2` d'`OP_CALL_BUILTIN` ;
   (c) le runtime garantit après chaque builtin que la pile a consommé exactement
   ces arguments et porte exactement UN résultat.
3. **Mémoire virtuelle** : nouveau module `src/runtime/vmem.{h,c}` (16 Mo hôte / 256 Ko MINT,
   big-endian 68k, wrap d'adresse). `PEEK/DPEEK/LPEEK/POKE/DPOKE/LPOKE/SPOKE/SDPOKE/SLPOKE`
   et `BYTE{}/CARD{}/WORD{}/LONG{}/SINGLE{}/DOUBLE{}` y accèdent vraiment (fin des stubs).
   `HIMEM` renvoie la taille de la vmem.
4. **Lecteur de touches** : tampon `keybuf[32]` dans `gfa_runtime` (+ `gfa_keybuf_pop()`)
   pour `KEYGET/KEYLOOK/KEYTEST/KEYPAD` ; `os_con_key_available()` (non-consommant) dans `os_layer`.
5. **Lexer** : ajout des accolades `{}` (`TOK_LBRACE/RBRACE`) pour `BYTE{}` & co, du suffixe `?`
   (`VAL?/STE?/TT?`), et un correctif critique : les tokens mots-clés laissaient une
   `ident_name` obsolète (`GOSUB sub` renvoyait un label faux → boucle infinie +
   débordement de call stack). `free_token_value()` avant chaque scan.
6. **`PAUSE`** attend réellement une touche (timeout optionnel) et renvoie son code.
7. **`RANDOM(n) / RAND(n)`** : ajouté à `parse_primary` + case runtime (entier dans [0, n-1]).

### Correctifs de la 2e vague (2026-08-15) — boucles, OPTION BASE, TOS

1. **`DO … LOOP [WHILE|UNTIL]`** : était un **no-op silencieux** (le corps ne s'exécutait
   jamais, sans aucun message). Implémenté au codegen : `cg_do_loop` avec post-test de
   condition (WHILE = continuer si vrai, UNTIL = continuer si faux) et saut arrière patché.
   Le parser marque WHILE/UNTIL dans `node->value.int_val`.
2. **`EXIT IF` / `EXIT`** : le lexer tokenisait `EXIT` → `TOK_EXIT_IF` + `IF` séparé
   (erreur de parse). Ajout de `parse_exit_if()` (dispatch statement + dans `DO … LOOP`)
   et d'une **pile de sorties de boucles** au codegen (`cg_loop_enter/leave`,
   `exit_patches[]`) : `EXIT IF` sort de la boucle la plus interne (DO, WHILE, FOR, REPEAT).
3. **`OPTION BASE 0/1`** : la syntaxe réelle échouait au parse (le handler ne consommait
   pas la ligne). Nouveau parse dédié (`BASE` optionnel) + champ `base` dans la structure
   de tableau (`gfa_var_array_create`, `OP_ARRAY_LOAD/STORE` appliquent la base avec
   contrôle de bornes quand base ≠ 0). Les tableaux sans OPTION BASE sont inchangés.
4. **`GEMDOS()/BIOS()/XBIOS()`** : les opcodes `OP_GEMDOS/BIOS/XBIOS` existaient déjà au
   runtime mais n'étaient **jamais émis** par le codegen. `cg_call` les émet désormais
   (le runtime attend `[fn] [arg1] [arg2]` sur la pile).

**Tests ajoutés** : `test_flow.bas` 16→22 (DO/LOOP ×2, EXIT IF dans DO/WHILE/FOR, EXIT
imbriqué), `test_arcom.bas` 8→9 (OPTION BASE 1), `test_prioa.bas` 40→42 (GEMDOS/BIOS/XBIOS).
Toutes les 23 suites `.bas` + 5 suites C passent.

### Détail des 22 items (terminés)

| # | Fonction | Token | Parser | Codegen | Runtime | Effort | Fichier |
|---|----------|-------|--------|---------|---------|--------|---------|
| 1 | **==** (approx. equality) | ✅ TOK_APPROX_EQ | ✅ AST | ✅ OP_APPROX_EQ | ✅ | Petit | `runtime.h`, `runtime.c`, `codegen.c` |
| 2 | VAL?() | ✅ TOK_VAL_COUNT | ✅ | ✅ | ✅ | Infime | `runtime.c` |
| 3 | INPUT$() | ✅ TOK_INPUT_TOK | ✅ | ✅ | ✅ | Infime | `runtime.c` |
| 4 | INPMID$() | ✅ TOK_INPMID | ✅ | ✅ | ✅ | Infime | `runtime.c` |
| 5 | DIR$() | ✅ TOK_DIR_TOK | ✅ | ✅ | ✅ | Petit | `os_layer.c/h`, `runtime.c` |
| 6 | DFREE() | ✅ TOK_DFREE | ✅ | ✅ | ✅ | Infime | `os_layer.c/h`, `runtime.c` |
| 7 | TYPE() | ✅ TOK_TYPE_TOK | ✅ | ✅ | ✅ | Infime | `runtime.c` |
| 8 | PAUSE | ✅ TOK_PAUSE | ✅ | ✅ | ✅ | Infime | `runtime.c` |
| 9 | DELAY | ✅ TOK_DELAY | ✅ | ✅ | ✅ | Infime | `runtime.c` |
| 10 | RANDOMIZE | ✅ TOK_RANDOMIZE | ✅ | ✅ | ✅ | Petit | `runtime.c`, `gfamath.c/h` |
| 11 | MOUSE/MOUSEX/MOUSEY/MOUSEK/SETMOUSE | ✅ | ✅ | ✅ | ✅ | Petit | `runtime.c`, `gfx.c/h` |
| 12 | STICK/STRIG/PAD*/LPEN*/TOUCH/SPRITE | ✅ | ✅ | ✅ | ✅ | Petit | `runtime.c` |
| 13 | KEYDEF/KEYGET/KEYLOOK/KEYTEST/KEYPRESS/KEYPAD | ✅ | ✅ | ✅ | ✅ | Petit | `runtime.c` |
| 14 | TIMER/DATE$/TIME$ | ✅ | ✅ | ✅ | ✅ | Petit | `runtime.c`, `os_layer.c/h` |
| 15 | _C/_X/_Y | ✅ | ✅ | ✅ | ✅ | Infime | `runtime.c` |
| 16 | BYTE{}/CARD{}/WORD{}/LONG{}/SINGLE{}/DOUBLE{} | ✅ | ✅ | ✅ | ✅ | Petit | `runtime.c` |
| 17 | HIMEM/FRE() | ✅ | ✅ | ✅ | ✅ | Infime | `runtime.c` |
| 18 | EXIST() | ✅ TOK_EXIST | ✅ | ✅ | ✅ | Infime | `runtime.c` |
| 19 | ~ (tilde, NOT bitwise) | ✅ | ✅ | ✅ | ✅ | Infime | `runtime.c` |
| 20 | VOID | ✅ | ✅ | ✅ | ✅ | Infime | `runtime.c` |
| 21 | STE/TT | ✅ | ✅ | ✅ | ✅ | Infime | `runtime.c` |
| 22 | OB_X/OB_Y/OB_W/OB_H | ✅ | ✅ | ✅ | ✅ | Infime | `runtime.c` |

---

## 🟠 Priorité B — Moyen (40 items, ~5-7 jours)

Nécessitent parser + codegen + runtime + implémentation réelle (backend SDL2 pour les graphismes).

### B1 — Accès mémoire étendu — ✅ TERMINÉ (fait pendant la Priorité A, via `vmem.c`)

| # | Fonction | Description | Effort |
|---|----------|-------------|--------|
| 1 | SPOKE addr, byte | Écrire un octet en mémoire (`OP_SPOKE`) | ✅ |
| 2 | SDPOKE addr, word | Écrire un mot (16-bit) en mémoire (`OP_SDPOKE`) | ✅ |
| 3 | SLPOKE addr, long | Écrire un long (32-bit) en mémoire (`OP_SLPOKE`) | ✅ |

### B2 — Graphismes avancés (backend SDL2)

| # | Fonction | Description | Effort |
|---|----------|-------------|--------|
| 1 | PLOT x,y | Allumer un pixel | Petit |
| 2 | POINT(x,y) | Lire la couleur d'un pixel (fonction) | Petit |
| 3 | PTST(x,y) | Tester si pixel allumé | Petit |
| 4 | DRAW x,y | Tracer une ligne depuis la position courante | Petit |
| 5 | CURVE x1,y1,x2,y2,x3,y3 | Courbe de Bézier | Moyen |
| 6 | ALINE x1,y1,x2,y2,color | Ligne absolue avec couleur | Petit |
| 7 | HLINE x1,x2,y | Ligne horizontale rapide | Petit |
| 8 | ARECT x1,y1,x2,y2 | Rectangle plein | Petit |
| 9 | PRBOX x1,y1,x2,y2 | Rectangle avec motif | Petit |
| 10 | PELLIPSE x1,y1,x2,y2 | Ellipse avec motif | Moyen |
| 11 | RBOX x1,y1,x2,y2,rx,ry | Rectangle aux coins arrondis | Petit |
| 12 | SETDRAW mode,style,pattern,color | Configurer le style de trait | Petit |
| 13 | FILL x,y,border | Remplissage par inondation (flood fill) | Petit |
| 14 | CLIP x1,y1,x2,y2 | Rectangle de clipping | Petit |
| 15 | ACLIP array | Clipping via tableau | Petit |
| 16 | BOUNDARY x1,y1,x2,y2 | Définir la zone de remplissage | Petit |
| 17 | TEXT x,y,string$ | Texte graphique | Moyen (nécessite font) |
| 18 | ATEXT array,x,y | Texte depuis tableau | Moyen |
| 19 | ACHAR array | Caractères depuis tableau | Moyen |
| 20 | POLYLINE array | Ligne brisée (polygon outline) | Moyen |
| 21 | POLYFILL array | Polygone plein | Moyen |
| 22 | POLYMARK array | Marqueurs aux sommets | Petit |
| 23 | APOLY array | Polygone depuis tableau | Moyen |

### B3 — Couleurs / Palette

| # | Fonction | Description | Effort |
|---|----------|-------------|--------|
| 24 | SETCOLOR color,r,g,b | Définir une entrée de palette | Petit |
| 25 | VSETCOLOR color,r,g,b | Palette VDI (identique) | Petit |
| 26 | MODE n | Mode vidéo (résolution) | Moyen |
| 27 | HARDCOPY | Copie d'écran vers imprimante | Infime (stub) |

### B4 — Bitmap / Blitter

| # | Fonction | Description | Effort |
|---|----------|-------------|--------|
| 28 | BITBLT sx,sy,w,h TO dx,dy | Copie de zone rectangulaire | Moyen |
| 29 | GET x1,y1,x2,y2,array | Capturer zone écran vers tableau | Moyen |
| 30 | PUT x,y,array | Restaurer zone écran depuis tableau | Moyen |

### B5 — Instructions diverses

| # | Fonction | Parser actuel | Travail | Effort |
|---|----------|---------------|---------|--------|
| 31 | SETTIME date$,time$ | ❌ Token seulement | Ajouter `parse_statement()` + runtime | Petit |
| 32 | RESUME label | ❌ Token seulement | Ajouter parser (similaire GOTO) + runtime | Petit |
| 33 | FATAL message$ | ❌ Token seulement | Ajouter parser + runtime (print + exit) | Petit |
| 34 | QSORT array() | ❌ Token seulement | Ajouter parser 1 arg + `qsort()` C | Moyen |
| 35 | SSORT array() | ❌ Token seulement | Ajouter parser 1 arg + Shell sort | Moyen |
| 36 | SWAP a,b | ✅ Fonctionne (test_swap.bas) | — | — |
| 37 | RANDOM(n) | ✅ Fait pendant la Priorité A | — | — |
| 38 | DELETE x(i) | ❌ Token seulement | Suppression d'élément dans tableau | Moyen |
| 39 | INSERT x(i), val | ❌ Token seulement | Insertion d'élément dans tableau | Moyen |

---

## 🟢 Priorité C — Lourd / Déférable (10 items, ~4-8 semaines)

Nécessitent de nouveaux sous-systèmes, des dépendances externes, ou une architecture complexe.

### C1 — GEM AES complet (~45 fonctions)

Tous les tokens existent et sont parsés comme builtins, mais le runtime retourne 0.

**Fonctions concernées :**
- **FORM_** : `FORM_ALERT`, `FORM_BUTTON`, `FORM_CENTER`, `FORM_DIAL`, `FORM_DO`, `FORM_ERROR`, `FORM_KEYBD`, `FORM_INPUT`
- **MENU_** : `MENU_BAR`, `MENU_ICHECK`, `MENU_IENABLE`, `MENU_REGISTER`, `MENU_TEXT`, `MENU_TNORMAL`, `MENU`, `MENU_KILL`, `MENU_OFF`
- **OBJC_** : `OBJC_ADD`, `OBJC_CHANGE`, `OBJC_DELETE`, `OBJC_DRAW`, `OBJC_EDIT`, `OBJC_FIND`, `OBJC_OFFSET`, `OBJC_ORDER`
- **OB_** : `OB_X`, `OB_Y`, `OB_W`, `OB_H`, `OB_HEAD`, `OB_TAIL`, `OB_NEXT`, `OB_TYPE`, `OB_FLAGS`, `OB_STATE`, `OB_SPEC`, `OB_ADR`
- **RSRC_** : `RSRC_LOAD`, `RSRC_FREE`, `RSRC_GADDR`, `RSRC_SADDR`, `RSRC_OBFIX`
- **APPL_** : `APPL_INIT`, `APPL_EXIT`
- **WIND_** : `WIND_OPEN`, `WIND_CLOSE`, `WIND_DELETE`, `WIND_FIND`, `WIND_CREATE`, `WIND_CALC`, `WIND_GET`, `WIND_SET`, `WIND_UPDATE`
- **W_** : `W_HAND`, `W_INDEX`, `MW_OUT`
- **EVNT_** : `EVNT_MULTI`, `EVNT_MESAG`, `EVNT_KEYBD`, `EVNT_MOUSE`, `EVNT_BUTTON`, `EVNT_TIMER`, `EVNT_DCLICK`
- **GRAF_** : `GRAF_DRAGBOX`, `RC_COPY`, `RC_INTERSECT`, `RCALL`
- **SCRP_** : `SCRP_READ`, `SCRP_WRITE`
- **SHOWM** / **HIDEM**
- **ALERT**, **FILESELECT**, **FSEL_INPUT**

**Stratégie :** Créer `src/gem/aes.c` + `src/gem/object.c` avec backend SDL2 (fenêtres, événements). Architecture objet GEM (arbres OBJECT).

**Effort :** Très gros (semaines)

### C2 — ON MENU * (6 variantes)

`ON MENU GOSUB/BUTTON/KEY/IBOX/MESSAGE/OBOX` — tokens seulement. Dépend du sous-système AES complet.

**Effort :** Gros

### C3 — VDI (Virtual Device Interface, ~15 fonctions)

`CONTRL`, `INTIN`, `INTOUT`, `PTSIN`, `PTSOUT`, `GINTIN`, `GINTOUT`, `WORK_OUT`, `ADDRIN`, `ADDROUT`, et les fonctions `V_*`.

Tous parsés comme builtins, runtime retourne 0. L'interface VDI est un tableau de paramètres pour des appels système bas niveau.

**Effort :** Très gros

### C4 — Opérations matricielles MAT (19 fonctions)

- **I/O** : `MAT READ`, `MAT INPUT`, `MAT PRINT`
- **Init** : `MAT SET`, `MAT CLR`, `MAT ONE`, `MAT CPY`, `MAT XCPY`
- **Arithmétique** : `MAT ADD`, `MAT SUB`, `MAT MUL` (scalaire et matriciel)
- **Algèbre** : `MAT TRANS`, `MAT INV`, `MAT DET`, `MAT QDET`, `MAT RANG`, `MAT NORM`, `MAT BASE`
- **Unaires** : `MAT ABS`, `MAT NEG`

Nécessite :
1. Nouveau module `src/matrix/` avec fonctions d'algèbre linéaire (C89)
2. Parser pour le mot-clé `MAT` en préfixe d'instruction
3. Nouvelles opcodes `OP_MAT_*`
4. Gestion de tableaux 2D (actuellement seulement 1D)

**Effort :** Gros

### C5 — Fichiers Random (FIELD / GET# / PUT#)

- `FIELD #n, width AS var$, var$...` — définir un buffer d'enregistrement
- `GET# n, record` — lire un enregistrement
- `PUT# n, record` — écrire un enregistrement
- `LSET`, `RSET` — aligner à gauche/droite dans le buffer
- `RECORD` — position dans le fichier

Nécessite un système de buffers d'enregistrement (record I/O layer).

**Effort :** Gros

### C6 — Shell / Processus (SHEL_*, EXEC, CHAIN)

- `SHEL_READ cmd$, tail$` — lire la ligne de commande
- `SHEL_WRITE doex, isgr, iscr, cmd$, tail$` — lancer un programme
- `SHEL_GET`, `SHEL_PUT`, `SHEL_FIND`, `SHEL_ENVRN` — variables d'environnement
- `EXEC cmd$` — exécuter un programme externe
- `CHAIN filename$` — charger et exécuter un autre programme BASIC
- `MERGE filename$` — fusionner un fichier BASIC dans le programme courant

Nécessite fonction `os_shell_exec()`, `os_getenv()`, mécanisme CHAIN (remplacement de programme tout en préservant les variables COMMON).

**Effort :** Gros

### C7 — W: et L: préfixes (passage par référence typé)

`W:func(arg)` = passer arg comme word (16-bit) par référence.
`L:func(arg)` = passer arg comme long (32-bit) par référence.

Nécessite modification du mécanisme d'appel de procédure (AST, codegen, runtime call frames).

**Effort :** Moyen-Gros

### C8 — TOS (GEMDOS/BIOS/XBIOS) — amélioration des stubs

État actuel :
- **GEMDOS** : ~15 fonctions réelles (Fopen, Fclose, Fread, Fwrite, Fdelete, Fseek, Malloc, Mfree, Cconin, Cconout, Cconws, Cnecin, Dgetdrv, Dsetdrv, Super, Fversion, Pterm)
- ~50+ fonctions retournent -32 (EINVFN = invalid function)

À améliorer :
- Ajouter des stubs explicites avec messages de debug
- Pour les fonctions triviales : implémenter (Fgetdta, Fsetdta, Fsfirst, Fsnext avec `os_layer`)
- `Pexec` (lancer processus) — optionnel
- XBIOS : `Initmouse`, `Setscreen`, `Setpalette`, `Floprd`, `Flopwr`

**Effort :** Petit à Gros selon le nombre de fonctions réellement implémentées

### C9 — Itération de répertoire (FSFIRST / FSNEXT)

**FAIT.** `FSFIRST "masque"` / `FSNEXT` (statements) + `FNAME`, `FATTR`, `FPOS`, `SIZE` (fonctions), `EOF` sans argument = fin d'énumération. Implémenté dans `os_layer` (`os_dir_first`/`os_dir_next` : `FindFirstFileA`/`FindNextFileA` sur Windows, GEMDOS FsFirst/Fsnext sinon) avec handle persistant. Attributs convertis au format GFA (bit 0 = archive, bit 1 = lecture seule). Test : `tests/test_fsfirst.bas`.

**Effort :** Petit

### C10 — CONTRL / INTIN / INTOUT / PTSIN / PTSOUT (tableaux VDI)

Ces 5 tableaux sont des alias pour accéder aux paramètres VDI. Parsés comme des variables (get/set via les mécanismes normaux de variables). À vérifier si le getter/setter fonctionne réellement.

**Effort :** Petit (vérification + debug)

---

## Fichiers les plus touchés par priorité

| Fichier | A | B | C |
|---------|---|---|---|
| `src/runtime/runtime.c` | 22 items | 40 items | 10 items |
| `src/runtime/runtime.h` | #1 (OP_APPROX_EQ) | SPOKE, graphismes, SWAP | W:/L: |
| `src/parser/parser.c` | — | SPOKE, graphismes, SETTIME, RESUME, FATAL, QSORT, SSORT, SWAP | MAT, FIELD, SHEL, CHAIN |
| `src/codegen/codegen.c` | #1 (==) | Tous les B | Tous les C |
| `src/graphics/gfx.c` | #11 (MOUSE) | 20+ fonctions graphiques | — |
| `src/graphics/gfx.h` | #11 | 20+ fonctions graphiques | — |
| `src/os_layer.c` | #5,6,14 | — | SHEL, TOS, FSFIRST |
| `src/os_layer.h` | #5,6,14 | — | SHEL, TOS |
| `src/gfamath.c/h` | #10 | — | — |
| `src/memory/memory.c` | #16 | — | — |
| `src/tos/tos.c` | — | — | TOS améliorations |
| `src/gem/` (nouveau) | — | — | AES complet |
| `src/matrix/` (nouveau) | — | — | MAT complet |
| `src/io/files.c` | — | — | FIELD/GET#/PUT# |
| `Makefile` | — | — | Nouveaux modules |
| `tests/` | #1,2,8,9,10 | Tous | Tous |

---

## Recommandations

1. **Faire la priorité A en premier** — corrige le bug `==` et implémente ~22 fonctions qui « retournent 0 silencieusement » en 2-3 jours
2. **Puis priorité B par groupes** :
   - Groupe 1 : SPOKE/SDPOKE/SLPOKE (1 jour)
   - Groupe 2 : Instructions diverses (SETTIME, RESUME, FATAL, SWAP — 1 jour)
   - Groupe 3 : Graphismes (le plus gros, 3-5 jours)
   - Groupe 4 : RANDOM, QSORT, SSORT, DELETE, INSERT (2 jours)
3. **Déféger la priorité C** sauf pour les petites améliorations (TOS stubs, FSFIRST/FSNEXT)
4. **Prendre une décision explicite sur l'AES** avant de commencer — est-ce que PhoenixBasic doit supporter les fenêtres GEM ou se concentrer sur le mode console ?
