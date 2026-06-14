# PhoenixBasic — GFA Basic 3.5 Emulator (C89)

> Interpréteur GFA Basic 3.5 pour Atari ST, écrit en C ANSI (C89).

## Architecture

```
main.c ──┬── lexer/    (token.h, keywords.c/h, lexer.c/h)
         ├── parser/   (ast.c/h, parser.c/h — LL(1) récursif)
         ├── codegen/  (codegen.c/h — AST → bytecode)
         └── runtime/  (runtime.c/h — VM à pile, call stack, builtins)
              ├── memory/   (memory.c — symboles, tableaux, DATA)
              ├── builtins/ (math_builtin.c/h, strings.c/h)
              ├── io/       (files.c/h)
              ├── events/   (events.c/h — EVERY, AFTER, ON ERROR)
              ├── sound/    (sound.c/h — BEEP, SOUND)
              └── tos/      (tos.c/h — GEMDOS, BIOS, XBIOS)
 utils/os_layer (os_layer.c/h — abstraction fichiers, console, temps)
 build/          (objets .o)
 tests/          (test_*.c, test_*.bas)
```

## Règles C89 Strictes

- **Commentaires** : `/* */` uniquement, pas de `//`
- **Déclarations** : toutes les variables en début de bloc `{`
- **Pas de VLA** (variable-length arrays)
- **Pas de `alloca`** — `malloc`/`free` uniquement
- **Fonctions** : prototype obligatoire avant usage
- **Types mixtes** : pas de déclarations après des instructions
- **Inline** : pas de `inline` (C89 ne le supporte pas)
- **For** : pas de `for (int i = 0; ...)` — déclarer `i` avant
- **// comments** interdits même dans les .h

## Conventions de nommage

| Élément | Règle | Exemple |
|---------|-------|---------|
| Types (typedef struct) | PascalCase | `ParserState`, `GfaVarType` |
| Fonctions publiques | snake_case | `parse_expression()`, `lex_next_token()` |
| Fonctions statiques | snake_case | `match_keyword()`, `emit_bytecode()` |
| Macros | UPPER_CASE | `MAX_TOKEN_LEN`, `OP_ADD` |
| Constantes enum | TOK_UPPER_CASE | `TOK_IF`, `OP_ADD` |
| Variables | snake_case | `current_token`, `program_counter` |

## Compilation

```bash
make          # Compile tout
make app      # Compile l'émulateur
make test-all # Lance tous les tests (267+, 100%)
make test-os  # Test os_layer uniquement
make test-rt  # Test runtime uniquement
make test-lexer  # Test lexer uniquement
make test-parser # Test parser uniquement
make clean    # Nettoie build/
```

## Exécution

```bash
./build/gfabasic                    # Mode interactif (REPL)
./build/gfabasic demo_complete.bas  # Exécute un fichier .bas
./build/gfabasic tests/test_if.bas  # Exécute les tests
```

## Tests

- **Tests C** dans `tests/test_*.c` — basés sur des `assert()`, sans framework
- **Tests BASIC** dans `tests/test_*.bas` — programmes .bas avec PRINT
- Coverage : `make test-all` = 267+ tests, visé 100%
- Tests BASIC à exécuter avec `./build/gfabasic tests/test_*.bas`

## Bytecode (VM à pile)

Les opcodes sont définis dans `src/runtime/runtime.h` et forment un bytecode
postfixé (notation polonaise inversée). La VM évalue tout sur une pile.

Exemples : `OP_ADD` dépile deux valeurs et empile le résultat.
`OP_PRINT` dépile une valeur et l'affiche. `OP_JMP` saute à une adresse.

## Variables GFA

| Suffixe | Type | Enum |
|---------|------|------|
| (aucun) | Float (double) | `GFA_VAR_FLOAT` |
| $ | String | `GFA_VAR_STRING` |
| % | Long (int32) | `GFA_VAR_LONG` |
| & | Word (int16) | `GFA_VAR_WORD` |
| \| | Byte (uint8) | `GFA_VAR_BYTE` |
| ! | Bool | `GFA_VAR_BOOL` |
