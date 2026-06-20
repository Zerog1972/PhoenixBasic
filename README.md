# GFA Basic 3.5 — Émulateur pour Atari ST

Interpréteur compatible **GFA Basic 3.5 pour Atari ST**, écrit en **C ANSI (C89)**.

Analyse, compile et exécute des programmes `.bas` GFA Basic 3.5. Émule les appels TOS (GEMDOS, BIOS, XBIOS, AES, VDI) et fournit un environnement console avec primitives graphiques.

## Démarrage rapide

```bash
make          # compiler l'émulateur
./build/gfabasic demo_complete.bas   # exécuter la démo
./build/gfabasic                     # mode interactif (REPL)
```

## Tests

```bash
make test-all   # 267 tests C + 9 tests BASIC (440+ total, 100%)
./build/gfabasic tests/test_if.bas   # 31 tests IF/THEN/ELSE
```

## Mode interactif (REPL)

Deux modes : **édition** (prompt `n]`) et **commande** (prompt `>`). Une ligne vide bascule.

| Commande | Description |
|----------|-------------|
| `LIST` | Liste le programme avec indentation automatique |
| `EDIT n` | Édite la ligne n (curseur ← → Home End Bksp Del) |
| `DELETE n` ou `n-m` | Supprime une ligne ou une plage |
| `INSERT n` | Insère une ligne avant la position n |
| `RUN ["fichier"]` | Exécute le programme (ou charge + exécute) |
| `LOAD "fichier"` | Charge un fichier `.bas` |
| `SAVE [A,] "fichier"` | Sauvegarde le programme |
| `NEW` | Efface le programme |
| `CLS` / `QUIT` | Efface l'écran / quitte |

Les mots-clés sont automatiquement mis en majuscules à la saisie.

## Fonctionnalités implémentées

| Catégorie | Détail |
|-----------|--------|
| **Contrôle de flux** | IF/THEN/ELSE/ENDIF (inline + multi-lignes), FOR/NEXT (STEP), WHILE/WEND, REPEAT/UNTIL, SELECT/CASE, GOTO/GOSUB/RETURN/@, DO/LOOP |
| **Procédures/Fonctions** | PROCEDURE, FUNCTION, RETURN expr, LOCAL, VAR (by-ref), DEFFN/FN, récursion, appels sans parenthèses (`maProc 3, 7`) |
| **Tableaux 1D** | DIM, `a(i)=expr`, `PRINT a(i)`, OP_ARRAY_LOAD/STORE |
| **Opérateurs** | `+` `-` `*` `/` `^` `=` `<>` `<` `>` `<=` `>=` AND OR XOR NOT EQV IMP MOD DIV |
| **Mathématiques** | 35 fonctions : trigo, log, exp, racine, abs, min/max, factorielle, combinaisons, aléatoire |
| **Chaînes** | 18 fonctions : LEN, ASC, CHR$, VAL, LEFT$/RIGHT$/MID$, INSTR, UPPER$/LCASE$, TRIM$, STR$/BIN$/HEX$/OCT$, SPACE$ |
| **Entrées/Sorties** | PRINT, PRINT #, INPUT, INPUT #, LINE INPUT, INKEY$, CLS, LOCATE |
| **Fichiers** | OPEN/CLOSE (I/O/R/A/U), OPENW/CLOSEW, PRINT#/INPUT# |
    | **Mémoire** | PEEK/POKE/DPEEK/LPEEK, DIM/ERASE/CLEAR, DATA/READ/RESTORE, MALLOC/MFREE, BLOAD/BSAVE/BGET/BPUT |
| **Son** | BEEP, SOUND ch, freq, dur, vol, env |
| **Événements** | EVERY, AFTER, ON ERROR, ON BREAK, ERROR, ERR |
| **Édition** | LIST avec indentation, EDIT en place (← → Home End Bksp Del), INSERT, DELETE, LOAD/SAVE |

## Architecture

```
main.c ──┬── lexer/     (490 keywords, tokens, EOL)
         ├── parser/    (LL(1), AST, labels 2-pass)
         ├── codegen/   (AST → bytecode)
         └── runtime/   (VM pile, call stack, builtins)
              ├── memory/    (symboles, tableaux)
              ├── builtins/  (maths 35/35, chaînes 18/18)
              ├── io/        (fichiers, BLOAD/BSAVE/BGET/BPUT)
              ├── events/    (EVERY, AFTER, ON ERROR)
              ├── sound/     (BEEP, SOUND)
              ├── tos/       (GEMDOS, BIOS, XBIOS, AES)
              └── graphics/  (SDL2, COLOR, LINE, BOX, CIRCLE)
 utils/os_layer   (abstraction fichiers, console, temps)
```

## Limitations

| Limitation | Détail |
|-----------|--------|
| Mots-clés comme noms | `add`, `val`, `double`, `inc` réservés (lexer non contextuel) |
| Format flottant | IEEE-754, pas le format GFA 8 octets |
| GEM AES complet | Menus, fenêtres, événements souris partiellement implémentés |
| RESTORE label | Parse OK mais restauration DATA globale (label ignoré) |
| ON x GOTO/GOSUB | Parse OK, non implémenté dans le runtime |
