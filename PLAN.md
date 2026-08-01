# Plan de Développement — PhoenixBasic

> **Dernière mise à jour : 2026-07-03**
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
| **✅ Déjà implémenté** | ~130 | — | Flux, procédures, maths, chaînes, bits, fichiers, graphismes SDL2, TOS partiel |
| **🔴 Priorité A** | 22 | ~2-3 jours | Runtime `case` uniquement : VAL?, INPUT$, PAUSE, MOUSE, TIMER, DATE$/TIME$, `==` (bug fix)… |
| **🟠 Priorité B** | 40 | ~5-7 jours | Parser + codegen + runtime : SPOKE, graphismes avancés (PLOT, DRAW, TEXT, POLYLINE, FILL, BITBLT…), SETTIME, SWAP, QSORT |
| **🟢 Priorité C** | 10 | ~4-8 semaines | Nouveaux sous-systèmes : GEM AES, VDI, MAT, FIELD, SHEL, CHAIN |

**Total restant : 72 items**

---

## ✅ Déjà implémenté

### Contrôle de flux
IF/THEN/ELSE/ENDIF, FOR/NEXT/STEP/DOWNTO, WHILE/WEND, REPEAT/UNTIL, DO/LOOP, EXIT IF, SELECT/CASE/ENDSELECT, GOTO, GOSUB/RETURN, ON GOTO/GOSUB, STOP, END, QUIT

### Procédures/fonctions
PROCEDURE/FUNCTION, RETURN expr, ENDFUNC, LOCAL, VAR (by-ref), DEFFN/FN, récursion, appels sans parenthèses (`maProc 3, 7`)

### Opérateurs
+, -, *, /, ^, =, <>, <, >, <=, >=, AND, OR, XOR, NOT, EQV, IMP, MOD, DIV, & (concaténation)

### PRINT/INPUT
PRINT, PRINT # (fichier), PRINT AT(x,y), PRINT USING (format #, ##.##, **, $$, +, -, ^^^^, virgules), INPUT, INPUT #, LINE INPUT, INKEY$, CLS, LOCATE, HTAB, VTAB

### Fonctions mathématiques (35)
SIN, COS, TAN, ATN, ASIN, ACOS, SINQ, COSQ, SINH, COSH, TANH, EXP, LOG, LOG10, SQR, ABS, SGN, INT, FRAC, FIX, ROUND, CEIL, TRUNC, MIN, MAX, EVEN, ODD, PRED, SUCC, FACT, COMBIN, VARIAT, RND, DEG, RAD, CFLOAT, CINT

### Fonctions chaînes (28)
LEN, ASC, CHR$, VAL, VAL?, LEFT$, RIGHT$, MID$, INSTR, RINSTR, UPPER$, LCASE$, LOWER$, TRIM$, STR$, BIN$, HEX$, OCT$, SPACE$, STRING$, MKI$, MKL$, MKS$, MKF$, MKD$, CVI, CVL, CVS, CVF, CVD

### Opérations sur les bits
BTST, BSET, BCLR, BCHG, SHL, SHR, ROL, ROR

### Tableaux 1D
DIM, ERASE, ARRAYFILL, DIM?, OPTION BASE

### Mémoire
PEEK, POKE, DPEEK, DPOKE, LPEEK, LPOKE, MALLOC, MFREE, DATA, READ, RESTORE

### Fichiers
OPEN/CLOSE (I/O/R/A/U), OPENW/CLOSEW, PRINT#, INPUT#, BLOAD (stub), BSAVE (stub), BGET (stub), BPUT (stub)

### Graphismes SDL2
COLOR (palette 16 couleurs), LINE, BOX, PBOX, CIRCLE, PCIRCLE

### Événements
EVERY, AFTER, ON ERROR, ON BREAK, ERROR, ERR

### Son
BEEP, SOUND ch, freq, dur, vol, env

### TOS partiel
GEMDOS (~15 fonctions : Fopen, Fclose, Fread, Fwrite, Fdelete, Fseek, Malloc, Mfree, Cconin, Cconout, Cconws, Cnecin, Dgetdrv, Dsetdrv, Super, Fversion, Pterm), BIOS (Bconin, Bconout), XBIOS (Getrez, Physbase, Random, Gettime, etc.)

### Définitions (no-op)
DEFFILL, DEFLINE, DEFTEXT, DEFMOUSE, DEFMARK

### Debug
TRON, TROFF

### REPL/Éditeur
LIST (avec indentation), EDIT (curseur ← → Home End Bksp Del), DELETE, INSERT, LOAD/SAVE, RUN, NEW, CLS, format_line

---

## 🔴 Priorité A — Quick Wins (22 items, ~2-3 jours)

Ces fonctionnalités traversent déjà tout le pipeline (token → parser → AST → codegen), mais le runtime `OP_CALL_BUILTIN` n'a pas de `case` pour elles. Il suffit d'ajouter un cas dans `src/runtime/runtime.c`.

### Bug critique à corriger (#1)

**`==` (approximate equality)** : le token `TOK_APPROX_EQ` existe dans le parser et crée un nœud AST correct, mais le codegen dans `cg_expression()` n'a pas de `case TOK_APPROX_EQ` — le switch tombe dans `default:` qui émet `OP_ADD`. Tout code BASIC utilisant `==` est donc **silencieusement mal compilé**.

**Correctif :**
1. Ajouter `OP_APPROX_EQ` dans `runtime.h`
2. Ajouter `case TOK_APPROX_EQ: bc_op = OP_APPROX_EQ; break;` dans `cg_expression()` (codegen.c ~ligne 691)
3. Implémenter dans runtime : `ABS(a-b) < 1e-10`
4. Ajouter un test BASIC

### Détail des 22 items

| # | Fonction | Token | Parser | Codegen | Runtime | Effort | Fichier |
|---|----------|-------|--------|---------|---------|--------|---------|
| 1 | **==** (approx. equality) — **BUG** | ✅ TOK_APPROX_EQ | ✅ AST | ❌ génère OP_ADD ! | ❌ | Petit | `runtime.h`, `runtime.c`, `codegen.c` |
| 2 | VAL?() | ✅ TOK_VAL_COUNT | ✅ | ✅ | ❌ | Infime | `runtime.c` |
| 3 | INPUT$() | ✅ TOK_INPUT_TOK | ✅ | ✅ | ❌ | Infime | `runtime.c` |
| 4 | INPMID$() | ✅ TOK_INPMID | ✅ | ✅ | ❌ | Infime | `runtime.c` |
| 5 | DIR$() | ✅ TOK_DIR_TOK | ✅ | ✅ | ❌ | Petit | `os_layer.c/h`, `runtime.c` |
| 6 | DFREE() | ✅ TOK_DFREE | ✅ | ✅ | ❌ | Infime | `os_layer.c/h`, `runtime.c` |
| 7 | TYPE() | ✅ TOK_TYPE_TOK | ✅ | ✅ | ❌ | Infime | `runtime.c` |
| 8 | PAUSE | ✅ TOK_PAUSE | ✅ | ✅ | ❌ | Infime | `runtime.c` |
| 9 | DELAY | ✅ TOK_DELAY | ✅ | ✅ | ❌ | Infime | `runtime.c` |
| 10 | RANDOMIZE | ✅ TOK_RANDOMIZE | ✅ | ✅ | ❌ | Petit | `runtime.c`, `gfamath.c/h` |
| 11 | MOUSE/MOUSEX/MOUSEY/MOUSEK/SETMOUSE | ✅ | ✅ | ✅ | ❌ | Petit | `runtime.c`, `gfx.c/h` |
| 12 | STICK/STRIG/PAD*/LPEN*/TOUCH/SPRITE | ✅ | ✅ | ✅ | ❌ | Petit | `runtime.c` |
| 13 | KEYDEF/KEYGET/KEYLOOK/KEYTEST/KEYPRESS/KEYPAD | ✅ | ✅ | ✅ | ❌ | Petit | `runtime.c` |
| 14 | TIMER/DATE$/TIME$ | ✅ | ✅ | ✅ | ❌ | Petit | `runtime.c`, `os_layer.c/h` |
| 15 | _C/_X/_Y | ✅ | ✅ | ✅ | ❌ | Infime | `runtime.c` |
| 16 | BYTE{}/CARD{}/WORD{}/LONG{}/SINGLE{}/DOUBLE{} | ✅ | ✅ | ✅ | ❌ | Petit | `runtime.c` |
| 17 | HIMEM/FRE() | ✅ | ✅ | ✅ | ❌ | Infime | `runtime.c` |
| 18 | EXIST() | ✅ TOK_EXIST | ✅ | ✅ | ❌ | Infime | `runtime.c` |
| 19 | ~ (tilde, NOT bitwise) | ✅ | ✅ | ✅ | ❌ | Infime | `runtime.c` |
| 20 | VOID | ✅ | ✅ | ✅ | ❌ | Infime | `runtime.c` |
| 21 | STE/TT | ✅ | ✅ | ✅ | ❌ | Infime | `runtime.c` |
| 22 | OB_X/OB_Y/OB_W/OB_H | ✅ | ✅ | ✅ | ❌ | Infime | `runtime.c` |

---

## 🟠 Priorité B — Moyen (40 items, ~5-7 jours)

Nécessitent parser + codegen + runtime + implémentation réelle (backend SDL2 pour les graphismes).

### B1 — Accès mémoire étendu

| # | Fonction | Description | Effort |
|---|----------|-------------|--------|
| 1 | SPOKE addr, byte | Écrire un octet en mémoire (nouvel opcode `OP_SPOKE`) | Petit |
| 2 | SDPOKE addr, word | Écrire un mot (16-bit) en mémoire (`OP_SDPOKE`) | Petit |
| 3 | SLPOKE addr, long | Écrire un long (32-bit) en mémoire (`OP_SLPOKE`) | Petit |

Parser : ajouter dans `parse_statement()` (similaire à POKE). Codegen : émettre le nouvel opcode. Runtime : écrire via `memory.c`.

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
| 36 | SWAP a,b | ⚠️ Parse comme no-op | Remplacer par véritable swap de variables | Petit |
| 37 | RANDOM(n) | ❌ Pas dans parse_primary | Ajouter token dans builtins + runtime | Infime |
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

Parsés comme builtins, runtime retourne 0. À implémenter via `os_layer` en s'appuyant sur les fonctions `opendir`/`readdir`.

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
