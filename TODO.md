# TODO — Implémentation GFA Basic 3.5 (C89)

> Fichier de suivi basé sur le cahier des charges. Mis à jour : 7 juin 2026, 22h00.
> 20 bugs résolus, codegen 100%, PROCEDURE/FUNCTION/LOCAL/VAR/DEFFN implémentés, IF inline OK.
> 236/236 tests unitaires + 31 tests IF = 100%.

---

## ✅ Résolus (bugs critiques)

- [x] **B1** — Compteur de lignes erroné → abréviations désactivées dans le parser
- [x] **B2+B3** — Labels/GOTO/GOSUB → résolution 2 passes + case-insensitive (strieq)
- [x] **B4** — OP_CALL/OP_RET → sauvegarde/restauration IP/SP dans le runtime
- [x] **B5** — Table keywords non triée → re-sort complet des 490 entrées
- [x] **B6** — dispatch WHILE manquant → `case TOK_WHILE:` rajouté dans parse_statement
- [x] **B7** — EOL ghost consume → `gfa_lexer_next(/* EOL */)` supprimé de tous les parseurs
- [x] **B8** — Alias `$` manquants → 25 entrées ajoutées
- [x] **B9** — INPUT #1 échouait → corrigé par le tri correct de la table keywords
- [x] **B10** — OPEN fichier vide → handler `sp >= 3` avec pop conditionnel pour reclen
- [x] **B11** — MID$ 3 arguments → handler dédié avec pop LIFO correct
- [x] **B12** — STR$/BIN$/HEX$/OCT$ retournaient 0 → déplacés en handlers top-level
- [x] **B13** — Double-free data_values → supprimé de gfa_runtime_shutdown
- [x] **B14** — ast_free double-free → parcours left→right avec `child->right = NULL`
- [x] **B15** — Union int_val/str_val conflit → flags `has_str`/`has_ident` dans ast_node
- [x] **B16** — IF/ENDIF multi-lignes → le lexer ignorait les `\n` dans `skip_whitespace()`, `TOK_EOL` jamais produit. Corrigé : `scan_token()` retourne `TOK_EOL` pour `\n`/`\r`, `skip_whitespace()` ne les consomme plus.
- [x] **B17** — OP_ADD `discard(2)` → un `pop`+`peek` suivi de `discard(2)` jetait 3 valeurs au lieu de 2, écrasant l'opérande gauche dans les appels récursifs. Corrigé → `discard(1)`.
- [x] **B18** — EQV/IMP codegen → `TOK_EQV_OP`/`TOK_IMP_OP` manquaient dans le switch, tombaient sur `default: OP_ADD`. Ajoutés → `OP_EQV`/`OP_IMP`.
- [x] **B19** — DIV codegen → `TOK_DIV_OP` manquait, tombait sur `default: OP_ADD`. Ajouté → `OP_INT_DIV`.
- [x] **B20** — IF/THEN/ELSE inline → `IF cond THEN stmt ELSE stmt` non supporté. Ajouté dans `parse_if`.

---

## 🔴 Codegen — Instructions critiques (100% ✅)

- [x] **C5** — INPUT / LINE INPUT
- [x] **C7** — CLS / LOCATE / VTAB
- [x] **C18** — Fonctions intégrées : SIN, COS, TAN, ATN, ASIN, ACOS, SINQ, COSQ, SINH, COSH, TANH, EXP, LOG, LOG10, SQR, ABS, SGN, INT, FRAC, FIX, ROUND, CEIL, TRUNC, MIN, MAX, EVEN, ODD, PRED, SUCC, FACT, COMBIN, VARIAT, RND, DEG, RAD, CFLOAT, CINT, LEN, ASC, CHR$, VAL, LEFT$, RIGHT$, MID$, UPPER$, LCASE$, LOWER$, TRIM$, STR$, BIN$, HEX$, OCT$, SPACE$, STRING$, TRUE, FALSE, PI
- [x] **C6** — PRINT AT / PRINT USING (codegen prêt)
- [x] **C10** — COLOR / LINE / CIRCLE / BOX / PBOX / PCIRCLE (codegen + runtime prêts, sortie ANSI placeholder)
- [x] **C1** — GOTO/GOSUB avec résolution de labels (fonctionne pour labels non-keywords et keywords)

---

## 🟠 Codegen — Instructions importantes (10/10 ✅)

- [x] **C3** — DIM / ERASE / CLEAR / OPTION BASE
- [x] **C4** — DATA / READ / RESTORE / _DATA
- [x] **C9** — OPEN / CLOSE / OPENW / CLOSEW
- [x] **C12** — SOUND / BEEP / WAVE
- [x] **C13** — PROCEDURE / FUNCTION ✅ complet : parser (FUNCTION, RETURN expr, LOCAL, VAR), codegen (OP_SAVE_LOCAL, OP_BIND_REF, args popping), runtime (call frame save/restore, portée locale récursive)
- [x] **C14** — DEFFN / FN ✅ : parser (`TOK_DEFFN` → `parse_deffn()`, `TOK_FN` → `AST_FN_CALL` dans `parse_primary`), codegen (pop args + body + OP_RET), testé `FN twice(5)=10`, `FN addThree(1,2,3)=6`.
- [x] **C15** — ON ERROR GOSUB / ERROR / ERR / RESUME → codegen + runtime prêts
- [x] **C16** — EVERY / AFTER / ON BREAK → codegen + runtime prêts

---

## 🟡 Codegen — Instructions secondaires (7/7 ✅)

- [x] **C2** — FOR/NEXT : STEP optionnel
- [x] **C8** — INKEY$ / INP? / OUT?
- [x] **C11** — DEFFILL / DEFLINE / DEFTEXT / DEFMOUSE / DEFMARK → no-op codegen
- [x] **C17** — PEEK / POKE / DPEEK / DPOKE / LPEEK / LPOKE → codegen + runtime prêts
- [x] **C19** — SELECT/CASE ✅ : testé, fonctionne. Supporte CASE et DEFAULT.

---

## 🟢 Phases 3-10 (non démarrées)

| Phase | Contenu |
|-------|---------|
| **Phase 3** | Types DEFxxx, tableaux multi-D, MAT |
| **Phase 4** | Fichiers complets (BLOAD, FIELD, FSFIRST, SEEK...) |
| **Phase 5** | Fonctions intégrées restantes (POINT, PTST, EOF, LOF, LOC, MKI$...CVD, TYPE...) |
| **Phase 6** | Graphisme VDI complet (driver SDL2, primitives réelles) |
| **Phase 7** | GEM AES (APPL_INIT, MENU_BAR, FORM_DO, OBJC_*) |
| **Phase 8** | TOS complet (GEMDOS 90 fns, BIOS 20, XBIOS 50) |
| **Phase 9** | Mode interactif amélioré (LOAD, SAVE, MERGE, LIST, RENUM, AUTO) |
| **Phase 10** | Tests de compatibilité, optimisation, documentation |

---



## 🧪 Tests unitaires

| Test | Résultat | Commentaire |
|------|----------|-------------|
| test_os_layer | 102/102 ✅ | Couche OS complète |
| **test_runtime** | **72/72 ✅** | Lifecycle, variables, value stack, bytecode, execution, conditions, arrays, builtins (math + strings), end-to-end |
| test_lexer | 39/39 ✅ | Tokens, nombres, chaînes, commentaires, identifiants, EOL, abréviations, keyword lookup |
| test_parser | 23/23 ✅ | Simple statements, control flow (IF corrigé), procedures, graphics, file I/O |
| test_if.bas | 31/31 ✅ | IF/THEN/ELSE/ENDIF : comparaisons, logique, imbrication, inline |

**Total : 236/236 tests unitaires + 31 tests IF = 100%**

## 📊 Progression

| Bloc | Tâches | Fait | % |
|------|--------|------|---|
| Bugs bloquants | 20 | 20 | 100% |
| Codegen critiques | 6 | 6 | 100% |
| Codegen importants | 10 | 10 | 100% |
| Codegen secondaires | 7 | 7 | 100% |
| Phases 3-10 | ~50 | 0 | 0% |
| **Total** | **~93** | **43** | **46%** |

---

## ✅ Implémenté le 7 juin — PROCEDURE / FUNCTION / LOCAL / VAR

- [x] **Parser** : `TOK_FUNCTION` → `parse_function()`, `TOK_RETURN` accepte `RETURN expr`
- [x] **Parser** : `TOK_LOCAL` → `LOCAL var1, var2, ...`
- [x] **Parser** : `TOK_VAR` → bascule les paramètres suivants en by-ref (stocké dans `param->line`)
- [x] **Codegen** : `OP_SAVE_LOCAL` pour chaque paramètre valeur (save + pop arg)
- [x] **Codegen** : `OP_BIND_REF` pour les paramètres VAR (pop arg, pas de save/restore)
- [x] **Runtime** : `OP_SAVE_LOCAL` sauvegarde dans le call frame, `OP_RET` restaure
- [x] **Runtime** : `OP_BIND_REF` assigne sans sauvegarder, modification persistante après retour
- [x] **Runtime** : Correction `OP_ADD` → `discard(1)` au lieu de `discard(2)` (écrasait les opérandes)

**Tests validés** : `somme`, `mul3`, `facto` récursif, `sumTo` récursif, `fib(7)=13`, appels imbriqués, `addTo(VAR n, delta)`, `swapVals(VAR a, b)` ✅

---

## 🟡 Limites connues

| Limite | Statut | Détail |
|--------|--------|--------|
| `DEFFN` / `FN` | ✅ Implémenté | `DEFFN nom(args) = expr` et `FN nom(args)` fonctionnent. |
| Appel procédure bare `nom args` | 🟡 Non implémenté | Seul `GOSUB nom` et `result = func(args)` fonctionnent. |
| `IF/THEN/ELSE` inline | ✅ Implémenté | `IF cond THEN stmt ELSE stmt` sur une ligne. |
| `SELECT/CASE` avec expressions | ✅ Fonctionnel | Testé avec valeurs numériques. |
| Mots-clés comme noms | 🟡 Restriction | `add`, `double`, `val`, `inc` réservés → `myAdd`... |

## 🐛 Bugs connus restants

- **PRINT#** : la sortie vers un canal fichier n'est pas encore implémentée (seul PRINT écran fonctionne)
- **INPUT#** : lit depuis stdin au lieu du canal fichier spécifié
- **Fichiers binaires** : BLOAD/BSAVE/BGET/BPUT pas encore implémentés
- **Format flottant GFA** : IEEE-754 utilisé au lieu du format GFA 8 octets (mantisse 48 bits, exposant 16 bits)
