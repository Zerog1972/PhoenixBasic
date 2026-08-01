# TODO — GFA Basic 3.5 (C89)

> Mis à jour : 12 juin 2026. 21 bugs résolus, tableaux 1D OK, formatage/indentation OK.

---

## ✅ Résolus (21 bugs)

- [x] B1-B15 : bugs initiaux (lexer, parser, runtime, keywords, EOL, AST, mémoire)
- [x] B16 : IF/ENDIF multi-lignes (EOL manquant dans lexer)
- [x] B17 : OP_ADD discard(2) → discard(1) (écrasait opérandes récursifs)
- [x] B18 : EQV/IMP manquants dans switch codegen
- [x] B19 : DIV manquant → OP_INT_DIV
- [x] B20 : IF/THEN/ELSE inline
- [x] B21 : PRINT#/INPUT# (OP_PRINT_CHAN/OP_INPUT_FILE)

## ✅ Implémenté

### Contrôle de flux
IF/ELSE/ENDIF (inline + multi), FOR/NEXT/STEP, WHILE/WEND, REPEAT/UNTIL, DO/LOOP, SELECT/CASE/ENDSELECT, GOTO/GOSUB/RETURN/@, ON x GOTO/GOSUB

### Procédures et fonctions
PROCEDURE/FUNCTION avec args, RETURN expr, ENDFUNC, LOCAL, VAR (by-ref), DEFFN/FN, récursion, appels imbriqués, portée locale (OP_SAVE_LOCAL/OP_RET restore)

### Tableaux 1D
DIM a(n), a(i) = expr, PRINT a(i), expressions avec indices, OP_ARRAY_LOAD/OP_ARRAY_STORE, ARRAYFILL, DIM?, ERASE

### Opérateurs
+ - * / ^ = <> < > <= >= AND OR XOR NOT EQV IMP MOD DIV

### Entrées / Sorties
PRINT, PRINT # (fichier), PRINT AT, PRINT USING, INPUT, INPUT #, LINE INPUT, INKEY$, CLS, LOCATE, HTAB, VTAB

### Fichiers
OPEN/CLOSE (I/O/R/A/U), OPENW/CLOSEW, PRINT#/INPUT#

### Fonctions intégrées
Maths (35) : SIN/COS/TAN/ATN/ASIN/ACOS/SINQ/COSQ/SINH/COSH/TANH/EXP/LOG/LOG10/SQR/ABS/SGN/INT/FRAC/FIX/ROUND/CEIL/TRUNC/MIN/MAX/EVEN/ODD/PRED/SUCC/FACT/COMBIN/VARIAT/RND/DEG/RAD/CFLOAT/CINT + TRUE/FALSE/PI
Chaînes (18) : LEN/ASC/CHR$/VAL/LEFT$/RIGHT$/MID$/INSTR/RINSTR/UPPER$/LCASE$/LOWER$/TRIM$/STR$/BIN$/HEX$/OCT$/SPACE$/STRING$

### Mémoire
DIM/ERASE/CLEAR/OPTION BASE/ARRAYFILL/DIM?, PEEK/POKE/DPEEK/DPOKE/LPEEK/LPOKE, MALLOC/MFREE, DATA/READ/RESTORE

### Édition (REPL)
Modes édition/commande (bascule par ligne vide), LIST avec indentation et indices, EDIT en place (curseur ← → Home End Bksp Del), INSERT n (avec saisie directe), DELETE n/n-m, LOAD/SAVE, NEW/CLS, format_line (mots-clés en majuscules à la saisie)

### Graphisme (ANSI)
COLOR, LINE, CIRCLE, BOX, PBOX, PCIRCLE, DEFFILL/DEFLINE/DEFTEXT/DEFMOUSE/DEFMARK (no-op)

### Autres
BEEP/SOUND, EVERY/AFTER, ON ERROR/ON BREAK/ERROR/ERR/FATAL, TRON/TROFF

---

## 🟡 Limites

| Limite | Détail |
|--------|--------|
| Appel procédure bare | `maProc 1,2` non supporté |
| Mots-clés comme noms | `add`, `val`, `double`, `inc` réservés |
| Fichiers binaires | BLOAD/BSAVE/BGET/BPUT |
| FIELD / GET# / PUT# | Mode Random |
| Graphisme VDI réel | Pas de backend SDL2 |
| GEM AES / TOS | Non implémenté |

---

## 🧪 Tests

| Test | Résultat |
|------|----------|
| test_os | **102/102** ✅ |
| test_rt | **72/72** ✅ |
| test_lex | **39/39** ✅ |
| test_par | **23/23** ✅ |
| test_if.bas | **31/31** ✅ |
| Total | **267 — 100%** ✅ |
