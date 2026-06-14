---
name: run-tests
description: Exécuter et analyser les résultats des tests PhoenixBasic. Supporte test-all, test-lexer, test-parser, test-rt, test-os.
---

# Exécuter les tests PhoenixBasic

## Commandes disponibles

```bash
make test-all        # Suite complète (267+ tests)
make test-lexer      # Tests lexer (39 tests)
make test-parser     # Tests parser (23 tests)
make test-rt         # Tests runtime (72 tests)
make test-os         # Tests OS layer (102 tests)
```

## Analyser les résultats

Chaque `make test-*` affiche le nombre de tests réussis/échoués.
Le total des tests est la somme des 4 catégories. Objectif : 100%.

## Tests BASIC

Les tests `.bas` ne font pas partie de `make test-all`. Les exécuter séparément :

```bash
./build/gfabasic tests/test_if.bas     # 31 tests IF/THEN/ELSE
./build/gfabasic tests/test_func.bas   # Tests FUNCTION/PROCEDURE
```

## En cas d'échec

1. Identifier le test qui échoue (le nom est affiché)
2. Lire le fichier source du test dans `tests/` 
3. Appliquer `/skill:fix-bug` pour corriger
