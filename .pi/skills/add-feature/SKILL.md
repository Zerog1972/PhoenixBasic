---
name: add-feature
description: Ajouter un nouveau mot-clé ou une fonctionnalité au langage GFA Basic. Étapes systématiques : keyword → token → parse → codegen → runtime → test.
---

# Ajouter une fonctionnalité à PhoenixBasic

Suivre ces étapes **dans l'ordre**. Chaque étape dépend de la précédente.

## Étape 1 : Keyword (lexer)

1. Ajouter le nouveau mot-clé dans `src/lexer/keywords.c` 
   - Respecter l'ordre alphabétique dans la table des keywords
2. Ajouter le token correspondant dans `src/lexer/token.h` (enum, TOK_NOM)
3. Si nécessaire, ajouter la gestion dans `src/lexer/lexer.c`

## Étape 2 : Parse

1. Ajouter le(s) nœud(s) AST dans `src/parser/ast.h` (struct + type enum)
2. Ajouter la construction dans `src/parser/ast.c`
3. Ajouter la règle de parsing dans `src/parser/parser.c`
   - Fonction `parse_*` qui consomme les tokens
   - Vérifier que la grammaire reste LL(1)

## Étape 3 : Codegen

1. Ajouter la génération de bytecode dans `src/codegen/codegen.c`
2. Fonction `gen_*` qui traverse le nœud AST et émet des opcodes
3. Ajouter de nouveaux opcodes dans `src/runtime/runtime.h` si nécessaire

## Étape 4 : Runtime

1. Implémenter le nouvel opcode dans `src/runtime/runtime.c`
2. Ajouter `case OP_NOM:` dans la boucle principale `runtime_execute()`
3. S'assurer que la pile est correctement gérée (push/pop équilibrés)

## Étape 5 : Tests

1. Ajouter des tests C dans `tests/test_*.c` pour la nouvelle fonctionnalité
2. Ajouter des tests BASIC dans `tests/test_*.bas`
3. Exécuter `make test-all` et vérifier que tout passe
4. Vérifier les régressions avec les tests existants

## Règles importantes

- Chaque étape doit compiler sans erreur (`make runtime` après chaque étape)
- C89 strict : pas de `//`, déclarations en début de bloc, pas de VLA
- Toute allocation mémoire doit vérifier le retour NULL
- Les nouveaux tokens doivent être documentés dans `README.md`
