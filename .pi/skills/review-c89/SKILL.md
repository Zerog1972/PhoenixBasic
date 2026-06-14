---
name: review-c89
description: Vérifier la conformité C89 d'un fichier ou du projet. Détecte les violations C89 : // comments, déclarations mixtes, VLA, inline, for(int i).
---

# Review C89 — PhoenixBasic

## Vérifications automatiques

Compiler avec les flags stricts :

```bash
gcc -ansi -pedantic -Wall -Wextra -O2 -D_DEFAULT_SOURCE -c fichier.c
```

## Liste de vérification manuelle

### ❌ Interdit
- [ ] `//` comments (utiliser `/* */`)
- [ ] Déclarations après instructions dans un bloc
- [ ] `for (int i = 0; ...)` (déclarer `int i;` avant)
- [ ] Variable-length arrays (VLA) : `int arr[n]`
- [ ] `inline` functions
- [ ] `alloca()`
- [ ] `long long` (C99)
- [ ] `//` même dans les exemples ou commentaires

### ✅ Autorisé
- [ ] `/* */` comments
- [ ] `malloc()`, `calloc()`, `realloc()`, `free()`
- [ ] `struct` avec typedef en PascalCase
- [ ] Variables globales `static` dans les .c
- [ ] Macros `#define` avec `##` (token pasting) — extension GNU acceptée

### Vérifications sémantiques
- [ ] Tous les chemins de fonction retournent une valeur
- [ ] Pas de buffer overflow potentiel
- [ ] `char*` vs `const char*` correctement utilisés
- [ ] `printf` format string vs arguments cohérents
- [ ] Fonctions avec paramètres non utilisés marqués `(void)param`

## Compilation de validation

```bash
gcc -ansi -pedantic -Wall -Wextra -O2 -D_DEFAULT_SOURCE \
  -Isrc/utils -Isrc/builtins -Isrc/io -Isrc/events \
  -Isrc/sound -Isrc/tos -Isrc/runtime -Isrc/memory \
  -Isrc/lexer -Isrc/parser -Isrc/codegen \
  -c fichier.c -o /dev/null
```
