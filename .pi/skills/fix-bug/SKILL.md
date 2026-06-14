---
name: fix-bug
description: Process complet de correction de bug. Reproduction → diagnostic → fix → test → régression.
---

# Corriger un bug dans PhoenixBasic

## Étape 1 : Reproduction

1. Créer un fichier `.bas` minimal qui reproduit le bug
2. Lancer : `./build/gfabasic test_bug.bas`
3. Capture la sortie exacte et le comportement attendu
4. Noter toute erreur, crash, ou comportement inattendu

## Étape 2 : Diagnostic

1. Identifier le module responsable selon la pile d'appels ou le symptôme :
   - **Lexer** : mauvais token, erreur de reconnaissance
   - **Parser** : erreur de syntaxe, AST incorrect
   - **Codegen** : bytecode invalide, label non résolu
   - **Runtime** : crash, valeur incorrecte, stack overflow
2. Ajouter des `printf` de debug temporaires si nécessaire
3. Compiler sans `-O2` pour un debugging plus facile : `make clean && make CFLAGS="-ansi -pedantic -Wall -Wextra -g"`

## Étape 3 : Correction

1. Appliquer le fix dans le module identifié
2. S'assurer que le code reste C89 strict (// interdits, déclarations en début de bloc)
3. Vérifier qu'il n'y a pas de warning : `make clean && make runtime`

## Étape 4 : Tests

1. Vérifier que le bug est corrigé : `./build/gfabasic test_bug.bas`
2. Ajouter un test de régression dans `tests/test_*.{c,bas}`
3. Exécuter `make test-all` pour valider les régressions

## Étape 5 : Nettoyage

1. Supprimer les `printf` de debug
2. Nettoyer les fichiers temporaires : `make clean`
3. Compilation finale : `make` (avec `-O2`)
