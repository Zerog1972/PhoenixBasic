# Plan : Correction de la précédence AND/OR/NOT dans les expressions

## Contexte
Les opérateurs logiques AND/OR/XOR/EQV/IMP ont actuellement la même
précédence que `+` et `-`, ce qui les place **au-dessus** des comparaisons.
Il faut qu'ils soient **en dessous**.

## Modules impactés
- **parser.c** : restructuration de la hiérarchie d'expressions
- **tests/test_if.bas** : ajout de tests AND/OR/NOT sans parenthèses

## Ordre d'exécution
1. **parser.c** : seul module à modifier (codegen/runtime déjà OK)
2. **make** : vérifier la compilation
3. **make test-all** : vérifier les régressions
4. **tests/test_if.bas** : ajouter des tests BASIC

## Changements dans parser.c

### 1. Fonction `parse_expression` (existante → renommée)
- Renommer `parse_expression` → `parse_comparison` (fonction interne static)
- Aucun changement de logique : toujours `simple_expr (comp simple_expr)*`

### 2. Nouvelle fonction `parse_expression` (point d'entrée)
- Appelle `parse_comparison` pour obtenir l'opérande gauche
- Boucle : tant que le token est AND/OR/XOR/EQV/IMP, crée un noeud binaire
- Devient le nouveau point d'entrée pour toutes les expressions

### 3. Fonction `parse_simple_expr`
- Supprimer TOK_AND_OP, TOK_OR_OP, TOK_XOR_OP, TOK_EQV_OP, TOK_IMP_OP
- Ne garder que TOK_PLUS, TOK_MINUS, TOK_AMPERSAND

### 4. Forward declarations
- Ajouter `parse_comparison` dans les forward declarations statiques

## Risques
- **Appels récursifs** : `parse_expression` est appelée depuis parse_primary
  pour les parenthèses → OK, la nouvelle `parse_expression` appelle
  `parse_comparison`, et le cycle est correct.
- **Assignation `=` vs comparaison `=`** : Le `=` dans les assignations
  est géré au niveau de `parse_statement`, pas dans `parse_expression`.
  Aucun impact.
- **Régression** : Les tests existants utilisent des parenthèses pour
  AND/OR/NOT → toujours OK. Les tests de comparaisons simples inchangés.
