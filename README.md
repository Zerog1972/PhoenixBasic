# GFA Basic 3.5 — Emulateur pour Atari ST
# Reverse engineering de l'interpreteur GFA Basic 3.5 pour Atari ST
# GFA Basic 3.5 pour Atari ST (rétro ingénierie)

Interpréteur compatible **GFA Basic 3.5 pour Atari ST**, écrit en **C ANSI (C89)**.

Analyse, compile et exécute des programmes `.bas` GFA Basic 3.5. Émule les appels TOS (GEMDOS, BIOS, XBIOS, AES, VDI) et fournit un environnement console avec primitives graphiques.

## Démarrage rapide

```bash
make          # compiler l'émulateur
./build/gfabasic demo_complete.bas   # exécuter la démo
./build/gfabasic                     # mode interactif (REPL)
```

## Mode interactif (REPL)

L'éditeur intégré fonctionne en deux modes :

- **Mode édition** (prompt `n]`) : chaque ligne tapée est ajoutée au programme courant
- **Mode commande** (prompt `>`) : les commandes d'édition et d'exécution

Une ligne vide (Entrée) bascule entre les deux modes.

### Commandes

| Commande | Description |
|----------|-------------|
| `LIST [from[-to]]` | Liste les lignes du programme |
| `EDIT n` | Édite la ligne n (éditeur en place : ← → Home End Backspace Del) |
| `DELETE n` ou `n-m` | Supprime la ligne n ou les lignes n à m |
| `INSERT n` | Insère une ligne avant la position n (0 = début) |
| `RUN ["fichier"]` | Exécute le programme (ou charge + exécute) |
| `LOAD "fichier"` | Charge un fichier `.bas` |
| `SAVE [A,] "fichier"` | Sauvegarde le programme |
| `NEW` | Efface le programme |
| `CLS` | Efface l'écran |
| `QUIT` / `bye` | Quitte |

### Exemple

```
1] PRINT "hello"
2]                          ← Entrée vide → commande
> LIST
1] PRINT "hello"
> EDIT 1
PRINT "hello"               ← curseur positionné pour édition
← taper " world" à la fin
> RUN

hello world
```

## Tests

```bash
make test-all   # 236 tests unitaires (100%)
```

```bash
./build/gfabasic tests/test_if.bas   # 31 tests IF/THEN/ELSE
```

## Fonctionnalités implémentées

| Catégorie | Instructions |
|-----------|-------------|
| **Contrôle de flux** | IF/THEN/ELSE/ENDIF (multi-lignes + inline), FOR/NEXT (STEP), WHILE/WEND, REPEAT/UNTIL, SELECT/CASE/ENDSELECT, GOTO, GOSUB/RETURN, @proc |
| **Procédures/Fonctions** | PROCEDURE, FUNCTION, RETURN expr, ENDFUNC, LOCAL, VAR (passage par référence), DEFFN/FN, récursion, appels imbriqués |
| **Entrées/Sorties** | PRINT, PRINT #, INPUT, INPUT #, LINE INPUT, CLS, LOCATE, INKEY$, BEEP, SOUND |
| **Fichiers** | OPEN/CLOSE (modes I/O/R/A/U), OPENW/CLOSEW, PRINT#/INPUT# fichiers |
| **Mathématiques** | SIN, COS, TAN, ATN, ASIN, ACOS, SINQ, COSQ, SINH, COSH, TANH, EXP, LOG, LOG10, SQR, ABS, SGN, INT, FRAC, FIX, ROUND, CEIL, TRUNC, MIN, MAX, EVEN, ODD, PRED, SUCC, FACT, COMBIN, VARIAT, RND, DEG, RAD, CFLOAT, CINT, PI, TRUE, FALSE |
| **Chaînes** | LEN, ASC, CHR$, VAL, LEFT$, RIGHT$, MID$, INSTR, RINSTR, UPPER$, LCASE$, LOWER$, TRIM$, STR$, BIN$, HEX$, OCT$, SPACE$, STRING$ |
| **Opérateurs** | `+` `-` `*` `/` `^` `=` `<` `>` `<=` `>=` `<>` AND OR XOR NOT EQV IMP MOD DIV |
| **Mémoire** | PEEK/POKE/DPEEK/DPOKE/LPEEK/LPOKE, DIM, ERASE, CLEAR, OPTION BASE, ARRAYFILL, MALLOC, MFREE, BMOVE |
| **Données** | DATA, READ, RESTORE, _DATA |
| **Graphisme** | COLOR, LINE, CIRCLE, BOX, PBOX, PCIRCLE, DEFFILL, DEFLINE, DEFTEXT, DEFMOUSE, DEFMARK |
| **Événements/Erreurs** | EVERY, AFTER, ON ERROR, ON BREAK, ERROR, ERR, FATAL |
| **Debug** | TRON, TROFF, STOP, CONT, END, QUIT |

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                  main.c (frontend + REPL)            │
├──────────┬──────────┬──────────┬────────────────────┤
│  lexer/  │ parser/  │ codegen/ │     runtime/       │
│ (tokens) │  (AST)   │(bytecode)│  (VM + call stack) │
├──────────┴──────────┴──────────┴────────────────────┤
│  builtins/  │ memory/ │ io/ │ events/ │ sound/ │ tos/│
├─────────────────────────────────────────────────────┤
│                  utils/os_layer (abstraction OS)     │
└─────────────────────────────────────────────────────┘
```

- **Lexer** : 490 mots-clés, recherche dichotomique, insensible à la casse, nombres (&H, &X, &O), chaînes, commentaires, tokens EOL
- **Parser** : récursif descendant LL(1), construction AST, résolution de labels 2 passes
- **Codegen** : compilation AST → bytecode (JMP, CALL, arithmétique, comparaisons, builtins)
- **Runtime** : machine virtuelle à pile, 1024 niveaux de pile, call stack (256 frames), sauvegarde/restauration de portée locale avec OP_SAVE_LOCAL/OP_BIND_REF

## Limitations connues

| Limitation | Détail |
|-----------|--------|
| Appel procédure bare | `maProc 1, 2` non supporté — utiliser `GOSUB maProc` ou `result = func(args)` |
| Mots-clés comme noms | `add`, `double`, `val`, `inc` réservés — préfixer (`myAdd`, `myDouble`) |
| Fichiers binaires | BLOAD, BSAVE, BGET, BPUT non implémentés |
| Format flottant | IEEE-754 au lieu du format GFA 8 octets (mantisse 48 bits) |
| Graphisme réel | Primitives VDI en placeholder ANSI (pas de backend SDL2) |
| GEM AES / TOS complets | Non implémentés (~200 fonctions) |

## Phases futures

| Phase | Contenu |
|-------|---------|
| 3 | Types DEFxxx, tableaux multi-D, MAT |
| 4 | Fichiers complets (BLOAD, FIELD, FSFIRST, SEEK, GET/PUT) |
| 5 | Fonctions intégrées restantes (POINT, PTST, EOF, LOF, LOC, MKI$...CVD...) |
| 6 | Graphisme VDI + SDL2 (modes ST, palette, primitives réelles) |
| 7 | GEM AES (APPL_INIT, MENU_BAR, FORM_DO, OBJC_*) |
| 8 | TOS complet (GEMDOS 90 fns, BIOS 20, XBIOS 50) |
| 9 | Stabilisation et compatibilité |

## Licence

Projet éducatif — émulation du GFA Basic 3.5 pour Atari ST.
