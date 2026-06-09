# Reverse engineering de l'interpreteur GFA Basic 3.5 pour Atari ST

Interpréteur compatible **GFA Basic 3.5 pour Atari ST**, écrit en **C ANSI (C89)**.

Analyse, compile et exécute des programmes `.bas` GFA Basic 3.5. Émule les appels TOS (GEMDOS, BIOS, XBIOS, AES, VDI) et fournit un environnement console avec primitives graphiques.

## Démarrage rapide

```bash
make          # compiler l'émulateur
./build/gfabasic demo_complete.bas   # exécuter un programme
./build/gfabasic                     # mode interactif (REPL)
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
| **Contrôle de flux** | IF/THEN/ELSE/ENDIF (multi-lignes + inline), FOR/NEXT (STEP), WHILE/WEND, REPEAT/UNTIL, SELECT/CASE/ENDSELECT, GOTO, GOSUB/RETURN |
| **Procédures/Fonctions** | PROCEDURE, FUNCTION, RETURN expr, ENDFUNC, LOCAL, VAR (passage par référence), DEFFN/FN |
| **Entrées/Sorties** | PRINT, INPUT, LINE INPUT, CLS, LOCATE, INKEY$, BEEP, SOUND |
| **Fichiers** | OPEN, CLOSE, OPENW, CLOSEW (modes I/O/R/A/U) |
| **Mathématiques** | SIN, COS, TAN, ATN, ASIN, ACOS, SINQ, COSQ, SINH, COSH, TANH, EXP, LOG, LOG10, SQR, ABS, SGN, INT, FRAC, FIX, ROUND, CEIL, TRUNC, MIN, MAX, EVEN, ODD, PRED, SUCC, FACT, COMBIN, VARIAT, RND, DEG, RAD, CFLOAT, CINT, PI, TRUE, FALSE |
| **Chaînes** | LEN, ASC, CHR$, VAL, LEFT$, RIGHT$, MID$, INSTR, RINSTR, UPPER$, LCASE$, LOWER$, TRIM$, STR$, BIN$, HEX$, OCT$, SPACE$, STRING$ |
| **Opérateurs** | `+` `-` `*` `/` `^` `=` `<` `>` `<=` `>=` `<>` AND OR XOR NOT EQV IMP MOD DIV |
| **Mémoire** | PEEK, POKE, DPEEK, DPOKE, LPEEK, LPOKE, DIM, ERASE, CLEAR, OPTION BASE, ARRAYFILL, MALLOC, MFREE |
| **Données** | DATA, READ, RESTORE |
| **Graphisme** | COLOR, LINE, CIRCLE, BOX, PBOX, PCIRCLE, DEFFILL, DEFLINE, DEFTEXT, DEFMOUSE, DEFMARK |
| **Événements** | EVERY, AFTER, ON ERROR, ON BREAK, ERROR, ERR |
| **Debug** | TRON, TROFF, STOP, CONT, END, QUIT |

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                  main.c (frontend)                   │
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
- **Runtime** : machine virtuelle à pile, call stack (256 frames), sauvegarde/restauration de portée locale

## Limitations connues

| Limitation | Détail |
|-----------|--------|
| Appel procédure bare | `maProc 1, 2` non supporté — utiliser `GOSUB maProc` ou `result = func(args)` |
| Mots-clés comme noms | `add`, `double`, `val`, `inc` réservés — préfixer (`myAdd`, `myDouble`) |
| PRINT# / INPUT# | Sortie/entrée vers fichiers non implémentée |
| Fichiers binaires | BLOAD, BSAVE, BGET, BPUT non implémentés |
| Format flottant | IEEE-754 au lieu du format GFA 8 octets (mantisse 48 bits) |
| Graphisme réel | Primitives VDI en placeholder ANSI (pas encore de backend SDL2) |
| GEM AES | Non implémenté |
| TOS complet | GEMDOS/BIOS/XBIOS partiels |

## Phases futures

| Phase | Contenu |
|-------|---------|
| 3 | Types DEFxxx, tableaux multi-D, MAT |
| 4 | Fichiers complets (BLOAD, FIELD, FSFIRST, SEEK, GET/PUT) |
| 5 | Fonctions intégrées restantes (POINT, PTST, EOF, LOF, LOC, MKI$...CVD, TYPE...) |
| 6 | Graphisme VDI complet (driver SDL2, primitives réelles) |
| 7 | GEM AES (APPL_INIT, MENU_BAR, FORM_DO, OBJC_*) |
| 8 | TOS complet (GEMDOS 90 fns, BIOS 20, XBIOS 50) |
| 9 | Mode interactif amélioré (LOAD, SAVE, MERGE, LIST, RENUM, AUTO) |
| 10 | Tests de compatibilité, optimisation, documentation |

## Licence

Projet éducatif — émulation du GFA Basic 3.5 pour Atari ST.
