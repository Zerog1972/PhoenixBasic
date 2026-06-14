---
name: plan-mode
description: Mode planification pour PhoenixBasic. Analyser, planifier, puis exécuter les changements en phases.
---

# Mode Planification

Quand l'utilisateur demande une modification complexe, suivre ce processus.

## Phase 1 : Analyse

1. Lire les fichiers pertinents (`read`)
2. Comprendre l'architecture impactée (voir `AGENTS.md`)
3. Identifier tous les modules à modifier (lexer, parser, codegen, runtime)
4. Vérifier les dépendances entre les changements

## Phase 2 : Plan

Écrire un plan dans un fichier `.md` :

```markdown
# Plan : [Titre de la feature]

## Modules impactés
- lexer : ajouter mot-clé XYZ
- parser : ajouter règle parse_xyz 
- codegen : émettre OP_XYZ
- runtime : implémenter OP_XYZ

## Ordre
1. lexer (pas de dépendances)
2. parser (dépend du token)
3. codegen (dépend de l'AST)
4. runtime (dépend de l'opcode)
5. tests (dépend de tout)

## Risques
- LL(1) conflict avec EXISTING_TOKEN ?
- Stack balance dans la VM ?
```

## Phase 3 : Exécution

1. Faire approuver le plan par l'utilisateur
2. Exécuter chaque étape dans l'ordre
3. `make` après chaque étape pour vérifier la compilation
4. `make test-all` après la dernière étape

## Phase 4 : Validation

1. Tester manuellement : `./build/gfabasic demo_complete.bas`
2. Vérifier les régressions : `make test-all`
3. Mettre à jour la documentation si nécessaire
