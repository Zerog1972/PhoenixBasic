# GFA Basic 3.5 — Émulateur pour Atari ST

Interpréteur compatible **GFA Basic 3.5 pour Atari ST**, écrit en **C ANSI (C89)**.

Analyse, compile et exécute des programmes `.bas` GFA Basic 3.5. Émule les appels TOS (GEMDOS, BIOS, XBIOS, AES, VDI) et fournit un environnement console avec primitives graphiques SDL2.

## Démarrage rapide

```bash
make          # compiler l'émulateur
./build/gfabasic demo_cplt.bas   # exécuter la démo
./build/gfabasic                     # mode interactif (REPL)
```

## Tests

```bash
make test-all   # 369+ tests, 100%
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
| **Contrôle de flux** | IF/THEN/ELSE/ENDIF, FOR/NEXT/STEP/DOWNTO, WHILE/WEND, REPEAT/UNTIL, SELECT/CASE/ENDSELECT, GOTO/GOSUB/RETURN/@, DO/LOOP, EXIT IF, ON x GOTO/GOSUB, STOP, END, QUIT |
| **Procédures/Fonctions** | PROCEDURE, FUNCTION, RETURN expr, ENDFUNC, LOCAL, VAR (by-ref), DEFFN/FN, récursion, appels sans parenthèses (`maProc 3, 7`) |
| **Tableaux 1D** | DIM, ERASE, ARRAYFILL, DIM?, OPTION BASE |
| **Opérateurs** | `+` `-` `*` `/` `^` `=` `<>` `<` `>` `<=` `>=` AND OR XOR NOT EQV IMP MOD DIV, & (concat) |
| **Mathématiques** | 35 fonctions : SIN/COS/TAN/ATN/ASIN/ACOS/SINQ/COSQ/SINH/COSH/TANH, EXP/LOG/LOG10/SQR, ABS/SGN/INT/FRAC/FIX/ROUND/CEIL/TRUNC, MIN/MAX/EVEN/ODD/PRED/SUCC, FACT/COMBIN/VARIAT, RND, DEG/RAD, CFLOAT/CINT |
| **Chaînes** | 28 fonctions : LEN, ASC, CHR$, VAL, VAL?, LEFT$/RIGHT$/MID$, INSTR, RINSTR, UPPER$/LCASE$/LOWER$, TRIM$, STR$, BIN$, HEX$, OCT$, SPACE$, STRING$, MKI$/MKL$/MKS$/MKF$/MKD$, CVI/CVL/CVS/CVF/CVD |
| **Opérateurs bits** | BTST, BSET, BCLR, BCHG, SHL, SHR, ROL, ROR |
| **Print** | PRINT, PRINT #, PRINT AT(x,y), PRINT USING (format #, ##.##, **, $$, +, -, ^^^^, virgules), INPUT, INPUT #, LINE INPUT, INKEY$, CLS, LOCATE, HTAB, VTAB |
| **Fichiers** | OPEN/CLOSE (I/O/R/A/U), OPENW/CLOSEW, PRINT#/INPUT# |
| **Mémoire** | PEEK/POKE/DPEEK/DPOKE/LPEEK/LPOKE, DATA/READ/RESTORE, MALLOC/MFREE, BLOAD/BSAVE/BGET/BPUT |
| **Graphismes SDL2** | COLOR (palette 16 couleurs), LINE, BOX, PBOX, CIRCLE, PCIRCLE |
| **Son** | BEEP, SOUND ch, freq, dur, vol, env |
| **Événements** | EVERY, AFTER, ON ERROR, ON BREAK, ERROR, ERR |
| **Debug** | TRON, TROFF |
| **Édition** | LIST avec indentation, EDIT en place (← → Home End Bksp Del), INSERT, DELETE, LOAD/SAVE |

## Architecture

```
main.c ──┬── lexer/     (490 keywords, tokens, EOL)
         ├── parser/    (LL(1), AST, labels 2-pass)
         ├── codegen/   (AST → bytecode)
         └── runtime/   (VM pile, call stack, builtins)
              ├── memory/    (symboles, tableaux)
              ├── builtins/  (maths 35/35, chaînes 28/28, bits 8/8)
              ├── io/        (fichiers, BLOAD/BSAVE/BGET/BPUT)
              ├── events/    (EVERY, AFTER, ON ERROR)
              ├── sound/     (BEEP, SOUND)
              ├── tos/       (GEMDOS, BIOS, XBIOS, AES)
              └── graphics/  (SDL2, COLOR, LINE, BOX, CIRCLE)
 utils/os_layer   (abstraction fichiers, console, temps)
```

## Plan de développement

Voir [`PLAN.md`](PLAN.md) pour l'inventaire complet des fonctionnalités manquantes
et le plan de travail priorisé.

**Résumé :**

| Priorité | Nb items | Effort | Description |
|----------|----------|--------|-------------|
| **✅ Implémenté** | ~130 | — | Flux, procédures, maths, chaînes, bits, print AT/USING, conversion binaire, fichiers, graphismes SDL2, TOS partiel |
| **🔜 Priorité A** | 22 | ~2-3 jours | Runtime cases uniquement : VAL?, INPUT$, PAUSE, MOUSE, TIMER, DATE$/TIME$, == (bug fix), etc. |
| **🔜 Priorité B** | 40 | ~5-7 jours | Parser + codegen + runtime : SPOKE, graphismes avancés (PLOT, DRAW, TEXT, POLYLINE, FILL, BITBLT…), SETTIME, SWAP, QSORT |
| **⏳ Priorité C** | 10 | ~4-8 semaines | Nouveaux sous-systèmes : GEM AES, VDI, MAT, FIELD, SHEL, CHAIN |

## Limitations

| Limitation | Détail |
|-----------|--------|
| Mots-clés comme noms | Certains mots (`add`, `val`, `double`, `inc`) sont réservés car le lexer n'est pas contextuel |
| Format flottant | IEEE-754 (double précision), pas le format GFA propriétaire 8 octets |
| GEM AES | APPL_INIT/EXIT ok. FORM_ALERT, MENU_BAR, WIND_*, EVNT_* sont des stubs (retournent 0). Voir PLAN.md |
| RESTORE label | Parse OK mais la restauration est globale (le label est ignoré au runtime) |
| `:` séparateur | Non supporté — une ligne = une instruction (conforme au vrai GFA Basic 3.5) |
