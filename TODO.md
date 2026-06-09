# TODO — Implémentation GFA Basic 3.5 (C89)

> Fichier de suivi basé sur le cahier des charges. Mis à jour : 7 juin 2026, 23h45.
> 21 bugs résolus, codegen 100%, mode interactif complet, 236/236 tests OK.

---

## ✅ Résolus (21 bugs critiques)

- [x] **B1** — Compteur de lignes erroné → abréviations désactivées dans le parser
- [x] **B2+B3** — Labels/GOTO/GOSUB → résolution 2 passes + case-insensitive (strieq)
- [x] **B4** — OP_CALL/OP_RET → sauvegarde/restauration IP/SP dans le runtime
- [x] **B5** — Table keywords non triée → re-sort complet des 490 entrées
- [x] **B6** — dispatch WHILE manquant → `case TOK_WHILE:` ajouté
- [x] **B7** — EOL ghost consume → supprimé de tous les parseurs
- [x] **B8** — Alias `$` manquants → 25 entrées ajoutées
- [x] **B9** — INPUT #1 échouait → corrigé par tri correct des keywords
- [x] **B10** — OPEN fichier vide → handler `sp >= 3` + pop conditionnel
- [x] **B11** — MID$ 3 arguments → handler dédié pop LIFO
- [x] **B12** — STR$/BIN$/HEX$/OCT$ → handlers top-level
- [x] **B13** — Double-free data_values → retiré de shutdown
- [x] **B14** — ast_free double-free → child->right = NULL avant récursion
- [x] **B15** — Union int_val/str_val → flags has_str/has_ident
- [x] **B16** — IF/ENDIF multi-lignes → lexer ignorait les `\n`
- [x] **B17** — OP_ADD discard(2) → discard(1) (écrasait opérandes récursifs)
- [x] **B18** — EQV/IMP codegen → manquaient dans le switch → ajoutés
- [x] **B19** — DIV codegen → TOK_DIV_OP manquait → OP_INT_DIV
- [x] **B20** — IF/THEN/ELSE inline → supporté dans parse_if
- [x] **B21** — PRINT#/INPUT# → opcodes OP_PRINT_CHAN/OP_INPUT_FILE émis

---

## ✅ Implémenté

### Contrôle de flux
- IF/THEN/ELSE/ENDIF (multi-lignes + inline), FOR/NEXT (STEP), WHILE/WEND
- REPEAT/UNTIL, DO/LOOP, EXIT IF, SELECT/CASE/ENDSELECT (avec DEFAULT)
- GOTO, GOSUB/RETURN, @proc, ON x GOTO/GOSUB
- STOP, END, QUIT, CONT, PAUSE, DELAY

### Procédures et fonctions
- PROCEDURE/FUNCTION avec arguments, LOCAL, VAR, RETURN expr, ENDFUNC
- DEFFN/FN (fonctions inline)
- Récursion (facto, sumTo, fib) et appels imbriqués
- Portée locale : OP_SAVE_LOCAL sauvegarde/restaure, OP_BIND_REF pour VAR

### Opérateurs
- Arithmétiques : `+` `-` `*` `/` `^`
- Comparaisons : `=` `<>` `<` `>` `<=` `>=`
- Logiques/bitwise : AND OR XOR NOT EQV IMP
- Entiers : MOD DIV

### Entrées / Sorties
- PRINT, PRINT # (console + fichier avec `\n`), PRINT AT, PRINT USING
- INPUT, INPUT #, LINE INPUT (stdin + fichier)
- INKEY$, CLS, LOCATE, HTAB, VTAB

### Fichiers
- OPEN/CLOSE (modes I/O/R/A/U), OPENW/CLOSEW
- PRINT#/INPUT# via opcodes OP_PRINT_CHAN/OP_INPUT_FILE

### Fonctions intégrées (maths)
- SIN, COS, TAN, ATN, ASIN, ACOS, SINQ, COSQ, SINH, COSH, TANH
- EXP, LOG, LOG10, SQR, ABS, SGN, INT, FRAC, FIX, ROUND, CEIL, TRUNC
- MIN, MAX, EVEN, ODD, PRED, SUCC, FACT, COMBIN, VARIAT, RND
- DEG, RAD, CFLOAT, CINT, TRUE, FALSE, PI

### Fonctions intégrées (chaînes)
- LEN, ASC, CHR$, VAL, LEFT$, RIGHT$, MID$, INSTR, RINSTR
- UPPER$, LCASE$, LOWER$, TRIM$, STR$, BIN$, HEX$, OCT$, SPACE$, STRING$

### Mémoire
- DIM, ERASE, CLEAR, OPTION BASE, ARRAYFILL, DIM?
- PEEK/POKE/DPEEK/DPOKE/LPEEK/LPOKE, MALLOC/MFREE, BMOVE
- DATA/READ/RESTORE/_DATA

### Son
- BEEP, SOUND ch, freq, dur, vol, env

### Événements / Erreurs
- EVERY GOSUB, AFTER GOSUB, ON ERROR GOSUB, ERROR, ERR, FATAL, ON BREAK

### Graphisme (ANSI placeholder)
- COLOR, LINE, CIRCLE, BOX, PBOX, PCIRCLE
- DEFFILL, DEFLINE, DEFTEXT, DEFMOUSE, DEFMARK (no-op)

### Debug
- TRON, TROFF

### Mode interactif (REPL)
- Éditeur ligne avec modes édition/commande, bascule par ligne vide
- `n]` : mode édition, `>` : mode commande
- LIST [from[-to]], EDIT n (éditeur en place ← → Home End Bksp Del)
- DELETE n/n-m, INSERT n (avec saisie directe)
- LOAD/SAVE de programmes .bas
- NEW, CLS, QUIT/bye
- Toute ligne non-reconnue est exécutée comme commande immédiate

---

## 🟡 Limites connues

| Limite | Statut | Détail |
|--------|--------|--------|
| Appel procédure bare `nom args` | 🟡 Non supporté | Utiliser `GOSUB nom` ou `result = func(args)` |
| Mots-clés comme noms de fonction | 🟡 Restriction | `add`, `val`, `double`, `inc` réservés → préfixer |
| Format flottant GFA | 🟡 IEEE-754 | Doit être remplacé par format 8 octets GFA |
| Graphisme VDI | 🟡 Placeholder ANSI | Pas encore de backend SDL2 |
| GEM AES / TOS | 🔴 Non implémenté | ~200 fonctions système manquantes |
| Fichiers binaires | 🔴 Non implémenté | BLOAD/BSAVE/BGET/BPUT |
| FIELD / GET# / PUT# | 🔴 Non implémenté | Mode Random non fonctionnel |

---

## 🎯 Prochaines étapes

| Priorité | Tâche | Complexité |
|----------|-------|------------|
| Haute | Fonctions chaînes restantes (MKI$/MKL$/CVI/CVL/INSERT/INPUT$) | ⭐ |
| Haute | Procédure bare `nom args` dans parse_statement | ⭐ |
| Moyenne | Types DEFxxx (DEFBYT, DEFSTR, DEFDBL...) | ⭐⭐ |
| Moyenne | Tableaux multidimensionnels complets (2-7 dims) | ⭐⭐ |
| Basse | MAT opérations matricielles (15 instructions) | ⭐⭐⭐ |
| Basse | Backend SDL2 + primitives VDI réelles | ⭐⭐⭐⭐ |

---

## 🧪 Tests

| Test | Résultat | Commentaire |
|------|----------|-------------|
| test_os_layer | **102/102** ✅ | Couche OS complète |
| test_runtime | **72/72** ✅ | VM, bytecode, builtins, arrays, end-to-end |
| test_lexer | **39/39** ✅ | Tokens, EOL, commentaires, abréviations |
| test_parser | **23/23** ✅ | IF/ELSE, FOR, WHILE, PROCEDURE, file I/O |
| test_if.bas | **31/31** ✅ | IF/THEN/ELSE, comparaisons, logique, imbrication |
| demo_complete.bas | ✅ | Démo 22 sections complète |

**Total : 267 tests — 100%**
