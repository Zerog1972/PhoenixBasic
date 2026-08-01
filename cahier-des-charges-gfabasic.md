# Cahier des Charges — Implémentation du GFA Basic 3.5 pour Atari ST

> **Langage cible** : C (norme ANSI C89 / ISO C90)
> **Système émulé** : GFA Basic 3.5 pour Atari ST (TOS)
> **Source de référence** : [Gladir.com - Référence GFA Basic](https://www.gladir.com/CODER/GFABASIC/reference.htm)
> **Source complémentaire** : [GFABasic Compendium v3.00](https://gfabasic.net/stg/gfabasic.htm) — manuel officiel GFA Basic 3.x porté en hypertexte par Lonny Pursell (ENCOM)

---

## Table des matières

1. [Présentation générale](#1-présentation-générale)
2. [Architecture du projet](#2-architecture-du-projet)
3. [Analyseur lexical (Lexer)](#3-analyseur-lexical-lexer)
4. [Analyseur syntaxique (Parser)](#4-analyseur-syntaxique-parser)
5. [Représentation intermédiaire et AST](#5-représentation-intermédiaire-et-ast)
6. [Système de types et variables](#6-système-de-types-et-variables)
7. [Moteur d'exécution (Runtime)](#7-moteur-dexécution-runtime)
8. [Jeu d'instructions](#8-jeu-dinstructions)
9. [Environnement d'exécution graphique](#9-environnement-dexécution-graphique)
10. [L'éditeur intégré](#10-léditeur-intégré)
11. [Codes d'erreur](#11-codes-derreur)
12. [Compatibilité et différences](#12-compatibilité-et-différences)
13. [Problèmes connus et pièges](#13-problèmes-connus-et-pièges)
14. [Phases de réalisation](#14-phases-de-réalisation)
15. [Annexes](#15-annexes)

---

## 1. Présentation générale

### 1.1 Objectif

L'objectif est de réaliser un interpréteur compatible avec le **GFA Basic 3.5 pour Atari ST**, écrit intégralement en **C ANSI (C89/ISO C90)**, capable de :

- Analyser et exécuter des programmes sources écrits en GFA Basic 3.5
- Émuler les appels au TOS (GEMDOS, BIOS, XBIOS, AES, VDI)
- Fournir un environnement de fenêtrage GEM compatible
- Gérer les modes graphiques de l'Atari ST (basse, moyenne, haute résolution)
- Interpréter le bytecode ou exécuter ligne par ligne selon le mode choisi

### 1.2 Périmètre fonctionnel

L'implémentation doit couvrir l'intégralité des **~280 commandes, instructions et fonctions** documentées du GFA Basic 3.5 :

| Catégorie | Nombre approx. | Description |
|-----------|----------------|-------------|
| Contrôle de flux | 25 | IF/ENDIF, FOR/NEXT, WHILE/WEND, REPEAT/UNTIL, GOTO, GOSUB/RETURN, SELECT/CASE/ENDSELECT... |
| Types et définitions | 15 | DEFBIT, DEFBYT, DEFWRD, DEFNUM, DEFSTR, DEFDBL, DEFFLT, DEFLIST, DEFMARK, DEFFN... |
| Entrées/Sorties console | 15 | PRINT, INPUT, LINE INPUT, INKEY$, INP?, OUT?, CLS, LOCATE, POS, CRSCOL, CRSLIN, HTAB, TAB... |
| Gestion de fichiers | 25 | OPEN, CLOSE, INPUT#, PRINT#, GET#, PUT#, BLOAD, BSAVE, SGET, SPUT, SEEK, RELSEEK, EOF, LOF, LOC, FIELD, FILES... |
| Manipulation de chaînes | 20 | ASC, CHR$, LEN, MID$, LEFT$, RIGHT$, INSTR, RINSTR, STR$, VAL, STRING$, SPACE$, TRIM$, BIN$, HEX$, OCT$... |
| Mathématiques | 35 | SIN, COS, TAN, ATN, ASIN, ACOS, SINQ, COSQ, EXP, LOG, LOG10, SQR, ABS, SGN, INT, FRAC, FIX, ROUND, RND, RANDOMIZE... |
| Gestion mémoire | 15 | DIM, ERASE, CLEAR, MALLOC, MFREE, BMOVE, PEEK, POKE, LPEEK, LPOKE, DPEEK, DPOKE, SPOKE, SDPOKE, SLPOKE... |
| Graphisme (VDI) | 40 | LINE, CIRCLE, BOX, RBOX, PBOX, PCIRCLE, PELLIPSE, POLYLINE, POLYFILL, POLYMARK, FILL, BOUNDARY, COLOR, DEFFILL, DEFLINE... |
| Gestion des fenêtres GEM | 20 | OPENW, CLOSEW, CLEARW, TITLEW, INFOW, TOPW, GETSIZE, WIND_OPEN, WIND_CLOSE, WIND_FIND, WIND_DELETE... |
| AES (GEM) | 55 | APPL_INIT, APPL_EXIT, APPL_FIND, FORM_ALERT, FORM_DIAL, FORM_DO, OBJC_ADD, OBJC_CHANGE, OBJC_DRAW, OBJC_DELETE... |
| Shell / Processus | 10 | SHEL_READ, SHEL_WRITE, SHEL_GET, SHEL_PUT, SHEL_FIND, SHEL_ENVRN, EXEC, CHAIN, RUN, QUIT... |
| VDI | 8 | CONTRL, INTIN, INTOUT, PTSIN, PTSOUT, GINTIN, GINTOUT, WORK_OUT... |
| GEMDOS / BIOS / XBIOS | 3 | GEMDOS, BIOS, XBIOS |
| Son | 5 | SOUND, BEEP, WAVE... |
| Gestion événements | 12 | EVERY, AFTER, EVNT_MULTI, EVNT_MESAG, EVNT_KEYBD, EVNT_MOUSE, EVNT_BUTTON, EVNT_TIMER... |
| Divers | 20 | TIME, DATE$, TIMER, SETTIME, PAUSE, DELAY, MONITOR, HARDCOPY, SYSTEM... |

### 1.3 Contraintes techniques

- **Langage** : C ANSI (C89 / ISO C90) — pas de fonctionnalités C99 ou ultérieures
- **Portabilité** : Code structuré avec abstraction de la couche graphique/OS
- **Gestion mémoire** : Allocation dynamique manuelle (pas de garbage collector), compatible avec les segments de 64 Ko de l'Atari ST
- **Performance** : L'interpréteur doit être suffisamment performant pour exécuter des programmes de taille moyenne sans ralentissement perceptible
- **Précision numérique** : Support des entiers 8/16/32 bits et flottants simple et double précision

---

## 2. Architecture du projet

### 2.1 Vue d'ensemble

```
┌──────────────────────────────────────────────────────────┐
│                     Interface utilisateur                  │
│       (Console / TUI / Émulation GEM — selon phase)       │
├──────────────────────────────────────────────────────────┤
│                     Frontend (main.c)                     │
│  Chargement fichier .BAS, mode interactif, ligne commande  │
├──────────────────────┬───────────────────────────────────┤
│       Lexer          │         Parser                    │
│  (tokenisation)      │  (analyse grammaticale)           │
│  lexer.c / lexer.h   │  parser.c / parser.h              │
├──────────────────────┴───────────────────────────────────┤
│                        AST / IR                          │
│              ast.c / ast.h  (arbre syntaxique)           │
├──────────────────────────────────────────────────────────┤
│                 Analyse sémantique / Binding              │
│           semantic.c / semantic.h  (résolution noms)     │
├──────────────────────────────────────────────────────────┤
│                     Runtime / Exécuteur                   │
│  runtime.c  (boucle principale, pile, contexte)          │
│  exec_stmt.c   (exécution instructions)                  │
│  exec_expr.c   (évaluation expressions)                  │
│  memory.c      (gestion mémoire, variables, tableaux)    │
├──────────────────────────────────────────────────────────┤
│                   Bibliothèques internes                  │
│  strings.c    │  math.c     │  files.c   │  graphics.c   │
│  events.c     │  sound.c    │  gemdos.c  │  vdi.c        │
│  aes.c        │  bios.c     │  xbios.c   │  window.c     │
├──────────────────────────────────────────────────────────┤
│                 Couche d'abstraction OS                   │
│  os_layer.c / os_layer.h  (fichiers, console, timer...)  │
└──────────────────────────────────────────────────────────┘
```

### 2.2 Composants et modules

| Module | Fichier(s) | Rôle |
|--------|-----------|------|
| **Lexer** | `lexer.c`, `lexer.h`, `token.h` | Analyse lexicale : découpage du source en tokens |
| **Parser** | `parser.c`, `parser.h` | Analyse syntaxique : construction de l'AST |
| **AST** | `ast.c`, `ast.h`, `ast_types.h` | Définition des nœuds de l'arbre syntaxique abstrait |
| **Semantic** | `semantic.c`, `semantic.h` | Vérification des types, résolution des étiquettes/variables |
| **Runtime** | `runtime.c`, `runtime.h`, `runtime_types.h` | Boucle d'exécution, gestion de la pile d'appels |
| **Memory** | `memory.c`, `memory.h` | Gestion des variables, tableaux, chaînes, heap |
| **Strings** | `strings.c`, `strings.h` | Fonctions de manipulation de chaînes |
| **Math** | `gfamath.c`, `gfamath.h` | Fonctions mathématiques intégrées |
| **Files** | `files.c`, `files.h` | Gestion des fichiers (OPEN, CLOSE, I/O...) |
| **Graphics** | `graphics.c`, `graphics.h` | Primitives graphiques VDI |
| **GEM** | `gem_aes.c`, `gem_vdi.c`, `gem_window.c` | Implémentation AES, VDI et fenêtres |
| **BIOS/XBIOS/GEMDOS** | `tos.c`, `tos.h` | Appels système TOS |
| **Events** | `events.c`, `events.h` | Gestion des événements (EVERY, AFTER, ON MENU...) |
| **Sound** | `sound.c`, `sound.h` | Gestion du son (SOUND, BEEP) |
| **OS Layer** | `os_layer.c`, `os_layer.h` | Abstraction système (fichiers, console, temps) |
| **Utils** | `utils.c`, `utils.h` | Utilitaires généraux |

---

## 3. Analyseur lexical (Lexer)

### 3.1 Tokens

Le lexer doit reconnaître les catégories de tokens suivantes :

```c
typedef enum {
    /* Mots-clés (tous en majuscules, insensibles à la casse) */
    TOK_PRINT, TOK_INPUT, TOK_LINE_INPUT, TOK_IF, TOK_THEN,
    TOK_ELSE, TOK_ENDIF, TOK_FOR, TOK_TO, TOK_STEP, TOK_NEXT,
    TOK_WHILE, TOK_WEND, TOK_REPEAT, TOK_UNTIL, TOK_DO,
    TOK_LOOP, TOK_EXIT_IF, TOK_GOTO, TOK_GOSUB, TOK_RETURN,
    TOK_ON, TOK_SELECT, TOK_CASE, TOK_DEFAULT, TOK_ENDSELECT,
    TOK_DIM, TOK_LET, TOK_DATA, TOK_READ, TOK_RESTORE,
    TOK_PROCEDURE, TOK_DEFFN, TOK_FN, TOK_END,
    TOK_OPEN, TOK_CLOSE, TOK_OPENW, TOK_CLOSEW, TOK_CLEARW,
    TOK_DEFBIT, TOK_DEFBYT, TOK_DEFWRD, TOK_DEFNUM, TOK_DEFSTR,
    TOK_DEFDBL, TOK_DEFFLT, TOK_DEFLIST, TOK_DEFMARK, TOK_DEFLINE,
    TOK_DEFTEXT, TOK_DEFMOUSE,
    TOK_COLOR, TOK_LINE, TOK_CIRCLE, TOK_BOX, TOK_RBOX,
    TOK_PBOX, TOK_PCIRCLE, TOK_PELLIPSE, TOK_CLS,
    /* ... tous les autres mots-clés (~280) ... */

    /* Littéraux */
    TOK_INTEGER,       /* entier décimal, hexa (&H), binaire (&X), octal (&O) */
    TOK_FLOAT,         /* nombre à virgule flottante */
    TOK_STRING,        /* chaîne entre guillemets doubles */
    TOK_IDENTIFIER,    /* nom de variable / étiquette */

    /* Opérateurs */
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_CARET,  /* + - * / ^ */
    TOK_EQ, TOK_LT, TOK_GT, TOK_LE, TOK_GE, TOK_NE,      /* = < > <= >= <> */
    TOK_LPAREN, TOK_RPAREN, TOK_COMMA, TOK_SEMICOLON,
    TOK_APOSTROPHE,    /* ' (séparateur colonnes PRINT) */
    TOK_COLON,         /* : (séparateur d'instructions) */
    TOK_HASH,          /* # (canal fichier) */
    TOK_AMPERSAND,     /* & (concaténation binaire) */
    TOK_AND, TOK_OR, TOK_XOR, TOK_NOT, TOK_EQV, TOK_IMP,
    TOK_MOD, TOK_DIV,

    /* Spéciaux */
    TOK_EOL,           /* fin de ligne */
    TOK_EOF,           /* fin de fichier */
} TokenType;
```

### 3.2 Caractéristiques du lexer

- **Insensible à la casse** : `Print`, `PRINT`, `print` sont équivalents
- **Nombres** :
  - Décimaux : `123`, `3.14`, `1.5E-2`
  - Hexadécimaux : `&HFF`, `&hA0`
  - Binaires : `&X1010`, `&x1100`
  - Octaux : `&O77`, `&o377`
- **Chaînes** : Délimitées par `"`, échappement par `""` (doublement du guillemet)
- **Commentaires** : `REM` en début d'instruction ou `'` (apostrophe) sur une ligne
- **Séparateur d'instructions** : `:` (deux-points) permet plusieurs instructions sur une ligne
- **Continuation de ligne** : Pas supportée nativement en GFA Basic 3.5 standard
- **Étiquettes** : Noms seuls en début de ligne suivis de `:` (ex: `MaBoucle:`)

### 3.3 Noms de variables

- Commencent par une lettre, peuvent contenir lettres, chiffres, underscore `_` et point `.`
- Longueur max : 255 caractères (limitée par la longueur de ligne max de 254 caractères)
- Les noms de fonctions suivent les mêmes règles que les noms de variables
- Les noms de procédures et d'étiquettes ont des règles moins restrictives (voir GOTO)
- Un suffixe de type (postfix) détermine le type de la variable :
  - `$` : chaîne de caractères (ex: `Nom$`) — longueur max 32767
  - `%` : entier long signé 32 bits (ex: `Compteur%`) — plage -2147483648 à 2147483647
  - `&` : entier word signé 16 bits (ex: `Petit&`) — plage -32768 à 32767
  - `!` : booléen (ex: `Flag!`) — valeurs FALSE (0) ou TRUE (-1)
  - `|` : octet non signé (ex: `Byte|`) — plage 0 à 255
  - `#` : flottant double précision (ex: `Precis#`) — 8 octets
  - *Sans suffixe* : flottant double précision (type par défaut)
- La notation `{adresse}` (ex: `BYTE{addr}`) n'est pas un suffixe de type mais un accès mémoire par adresse absolue (voir section 8.10)

---

## 4. Analyseur syntaxique (Parser)

### 4.1 Grammaire (sous-ensemble représentatif)

```
program       := line*
line          := [line_number] [statement (':' statement)*] [comment] EOL
line_number   := INTEGER
statement     := let_stmt | print_stmt | input_stmt | if_stmt
              |  for_stmt | while_stmt | repeat_stmt | select_stmt
              |  gosub_stmt | goto_stmt | return_stmt
              |  dim_stmt | data_stmt | read_stmt | restore_stmt
              |  procedure_def | deffn_stmt | call_stmt
              |  open_stmt | close_stmt | graphics_stmt
              |  « ... »

/* Expressions */
expression    := and_expr (OR and_expr)*
and_expr      := not_expr (AND not_expr)*
not_expr      := [NOT] compare_expr
compare_expr  := add_expr (('=' | '<' | '>' | '<=' | '>=' | '<>') add_expr)?
add_expr      := mul_expr (('+' | '-') mul_expr)*
mul_expr      := power_expr (('*' | '/' | MOD | DIV) power_expr)*
power_expr    := unary_expr ('^' unary_expr)*
unary_expr    := ['-' | '+'] primary_expr
primary_expr  := number | string | identifier | function_call
              |  '(' expression ')'

/* Assignation */
let_stmt      := 'LET'? identifier '=' expression
              |  identifier '=' expression

/* Contrôle de flux */
if_stmt       := 'IF' expression 'THEN' [statement]
              |  'IF' expression EOL statements... ['ELSE' EOL statements...] 'ENDIF'

for_stmt      := 'FOR' identifier '=' expression 'TO' expression ['STEP' expression] EOL
                 statements... 'NEXT' [identifier]

while_stmt    := 'WHILE' expression EOL statements... 'WEND'

repeat_stmt   := 'REPEAT' EOL statements... 'UNTIL' expression

select_stmt   := 'SELECT' expression EOL
                 ('CASE' expression EOL statements...)*
                 ['DEFAULT' EOL statements...]
                 'ENDSELECT'

/* Procédures / fonctions */
procedure_def := 'PROCEDURE' identifier ['(' [parameter (',' parameter)*] ')'] EOL
                 statements...
                 'RETURN'

deffn_stmt    := 'DEFFN' identifier ['(' [parameter (',' parameter)*] ')'] '=' expression
```

### 4.2 Gestion des numéros de ligne

Le parser doit supporter le mode avec numéros de ligne (compatibilité Atari ST) :

- Les numéros de ligne sont optionnels
- Si présents, ils doivent être en ordre croissant
- Le parser doit les ignorer pour la construction de l'AST mais les conserver pour l'édition
- Les références `GOTO` et `GOSUB` doivent pouvoir cibler des numéros de ligne ET des étiquettes nommées

### 4.3 Stratégie de parsing

**Parsing récursif descendant (LL(1))** avec backtracking minimal. Le GFA Basic a une grammaire relativement simple et déterministe grâce à ses mots-clés distincts en début d'instruction.

---

## 5. Représentation intermédiaire et AST

### 5.1 Types de nœuds AST

```c
/* Nœuds d'instructions */
typedef enum {
    AST_PROGRAM,          /* programme : liste d'instructions */
    AST_LINE_NUMBER,      /* numéro de ligne (optionnel) */
    AST_ASSIGN,           /* assignation (LET) */
    AST_PRINT,            /* PRINT */
    AST_INPUT,            /* INPUT */
    AST_IF,               /* IF/ELSE/ENDIF */
    AST_FOR,              /* FOR ... NEXT */
    AST_WHILE,            /* WHILE ... WEND */
    AST_REPEAT,           /* REPEAT ... UNTIL */
    AST_SELECT,           /* SELECT CASE */
    AST_GOTO,             /* GOTO */
    AST_GOSUB,            /* GOSUB */
    AST_RETURN,           /* RETURN */
    AST_ON_GOTO_GOSUB,    /* ON x GOTO/GOSUB */
    AST_DIM,              /* DIM */
    AST_DATA,             /* DATA */
    AST_READ,             /* READ */
    AST_RESTORE,          /* RESTORE */
    AST_PROCEDURE,        /* PROCEDURE ... RETURN */
    AST_CALL,             /* Appel procédure GOSUB ou CALL */
    AST_DEFFN,            /* DEFFN */
    AST_OPEN, AST_CLOSE,  /* Fichiers */
    AST_GRAPHICS,         /* Instructions graphiques (LINE, CIRCLE...) */
    AST_WINDOW,           /* OPENW, CLOSEW... */
    AST_MEMORY,           /* PEEK/POKE/BMOVE... */
    AST_SOUND,            /* SOUND */
    AST_EVENT,            /* EVERY, AFTER, ON BREAK... */
    /* ... */
} ASTNodeType;

/* Nœuds d'expression */
typedef enum {
    EXPR_NUMBER,          /* Entier ou flottant */
    EXPR_STRING,          /* Chaîne littérale */
    EXPR_VARIABLE,        /* Référence à une variable */
    EXPR_ARRAY_ACCESS,    /* Accès tableau : arr(i, j) */
    EXPR_BINARY,          /* Opération binaire */
    EXPR_UNARY,           /* Opération unaire (-, NOT) */
    EXPR_FUNCTION_CALL,   /* Appel de fonction */
    EXPR_FN_CALL,         /* Appel FN (DEFFN) */
} ExprNodeType;
```

### 5.2 Structure des nœuds

```c
typedef struct ASTExprNode {
    ExprNodeType type;
    union {
        struct { long integer; } number;
        struct { char *value; } string;
        struct { char *name; int index; } variable;
        struct { TokenType op; struct ASTExprNode *left, *right; } binary;
        struct { TokenType op; struct ASTExprNode *operand; } unary;
        struct { char *name; struct ASTExprNode **args; int argc; } func_call;
        struct { struct ASTExprNode *array; struct ASTExprNode **indices; int ndim; } array_access;
    } data;
} ASTExpr;

typedef struct ASTStmtNode {
    ASTNodeType type;
    int line_number;  /* optionnel, -1 si absent */
    union {
        /* Contenu spécifique au type */
        struct { char *label; } label;       /* étiquette */
        struct { ASTExpr *target; ASTExpr *value; } assign;
        struct { ASTExpr **items; int count; int channel; } print;
        struct { ASTStmtNode *cond; ASTStmtNode *then_branch; ASTStmtNode *else_branch; } if_stmt;
        struct { char *var; ASTExpr *start, *end, *step; ASTStmtNode *body; } for_loop;
        struct { ASTExpr *cond; ASTStmtNode *body; } while_loop;
        /* ... */
    } data;
    struct ASTStmtNode *next;  /* instruction suivante */
} ASTStmt;
```

---

## 6. Système de types et variables

### 6.1 Types de données supportés

**Convention officielle GFA Basic 3.x (Atari ST) :**

| Type GFA Basic | Postfix | Taille mémoire | Plage / Description |
|---------------|---------|---------------|---------------------|
| Booléen (Boolean) | `!` | 1 octet (1 bit en tableau) | 0 (FALSE) ou -1 (TRUE). Toute valeur non nulle est convertie en -1. |
| Octet (Byte) | `\|` | 1 octet | 0 à 255. Plage non signée. |
| Mot (Word) | `&` | 2 octets | -32768 à 32767. Entier signé 16 bits. |
| Entier long (Long) | `%` | 4 octets | -2147483648 à 2147483647. Entier signé 32 bits. |
| Flottant (Float) | `#` | 8 octets | 2.225073858507E-308 à 3.595386269725E+1000. ~14 chiffres de précision. |
| *Défaut (sans postfix)* | — | 8 octets | Flottant double précision (identique à `#`). |
| Chaîne (String) | `$` | 0 à 32767 caractères | Caractères ASCII 0-255. Stockée via descripteur 6 octets. |
| Tableau | — | variable | Tout type ci-dessus, 1 à 7 dimensions. |

> **Note importante** : La notation `{adresse}` (ex : `BYTE{addr}`, `CARD{addr}`, `LONG{addr}`, `SINGLE{addr}`) est un **accès mémoire par adresse absolue** et non un suffixe de type. Voir section 8.10.

### 6.1.1 Format interne du flottant GFA Basic v3

Le format flottant GFA Basic v3 n'est pas IEEE-754 standard. Il utilise 8 octets :

```
Octet:  0       1       2       3       4       5        6       7
Bit :   63                                             16 15             0
        IMMM MMMM MMMM MMMM MMMM MMMM MMMM MMMM MMMM MMMM  SEEE EEEE EEEE EEEE
        <--------------- Mantisse 48 bits -------------->  <-- Exposant 16 bits -->
        ^  (normalisée)                                    ^ Bits de signe
        |                                                  | (S=1 → exposant négatif)
        +-- MSB = bit entier (toujours 1 sauf zéro)
```

- Mantisse : 48 bits, normalisée
- Exposant : 16 bits, biais = 1023
- Si S=1, l'exposant est négatif : exp = -exp
- Zéro : tous les octets à 0

### 6.2 Descripteur de tableau

Un tableau en GFA Basic est géré via un **descripteur de 6 octets** :
- **4 premiers octets** : adresse du début du tableau en mémoire
- **2 derniers octets** : nombre de dimensions du tableau

Le tableau lui-même commence par une série d'entiers 32 bits (4 octets chacun) donnant le nombre d'éléments dans chaque dimension, en commençant par la **dernière** dimension. Pour un tableau `DIM a%(2,3)` avec `OPTION BASE 0` :

```
*a%() ───► [Descripteur 6 octets]
           │ Adresse tableau │ Nb dims │
           └─────────────────┴─────────┘
                     │
                     ▼
           [4 octets: nb éléments dim 2] = 4  (0,1,2,3)
           [4 octets: nb éléments dim 1] = 3  (0,1,2)
           [a%(0,0)] [a%(1,0)] [a%(2,0)] [a%(0,1)] ...
```

Fonctions d'accès :
- `V:var` / `VARPTR(var)` : adresse d'une variable
- `*arr()` / `ARRPTR(arr())` : adresse du descripteur de tableau
- `DPEEK(*a%()+4)` : retourne le nombre de dimensions

### 6.3 Limites des tableaux

- Tableaux à 1 dimension : limités uniquement par la mémoire disponible
- Tableaux multi-dimensionnels :
  - Maximum **7 dimensions**
  - Dimensions après la 1ère doivent être < 65535
  - Produit des éléments doit être < 65535 (sauf pour les tableaux à 1 dimension)
  - Exemple valide : `DIM a%(100,10,10)` → 100×10×10 = 10000 ≤ 65535 ✓
- `DIM?()` retourne le nombre total d'éléments, dépendant de `OPTION BASE`

### 6.4 Directives DEFxxx

Les directives `DEFBYT`, `DEFWRD`, `DEFNUM`, `DEFSTR`, `DEFDBL`, `DEFFLT`, `DEFBIT` définissent le type par défaut des variables dont le nom commence par une plage de lettres donnée :

```
DEFBYT b        ' b, b0, Banane, ... sont des octets
DEFSTR s-t      ' s, t, SaChaine$, ... sont des chaînes
```

`DEFLIST` est ignoré par le compilateur ; il contrôle la capitalisation dans l'éditeur.

### 6.5 Variables réservées

GFA Basic définit plusieurs variables réservées accessibles globalement :

| Variable | Type | Description |
|----------|------|-------------|
| `FALSE` | Booléen | Constante valant 0 |
| `TRUE` | Booléen | Constante valant -1 (**attention** : pas 1 comme dans d'autres langages) |
| `PI` | Flottant | Constante π = 3.14159265359 |
| `DATE$` | Chaîne | Date système au format `JJ.MM.AAAA` (ou `MM/JJ/AAAA` en mode US) |
| `TIME$` | Chaîne | Heure système au format `HH:MM:SS` (rafraîchie toutes les 2 secondes) |
| `TIMER` | Entier long | Temps écoulé depuis le boot en 1/200ème de seconde (équivalent `LPEEK(&H4BA)`) |
| `_C` | Entier | Nombre de registres de couleur disponibles (= `WORK_OUT(13)`) |
| `_X` | Entier | Largeur de la fenêtre courante en pixels |
| `_Y` | Entier | Hauteur de la fenêtre courante en pixels |

> **Note sur `_X` et `_Y`** : Ces variables sont initialisées via les variables Line-A puis modifiées par les commandes de fenêtrage. En basse résolution ST : `_X=320`, `_Y=200` (alors que `WORK_OUT(0)=319`, `WORK_OUT(1)=199`).

Instructions associées :
- `SETTIME [heure$],date$` : règle l'heure et la date système
- `DATE$=date$` : règle uniquement la date
- `TIME$=heure$` : règle uniquement l'heure
- Ces instructions appellent en interne `Tsetdate()`, `Tsettime()` (GEMDOS)

### 6.6 Variable spéciale `$` et instruction `VOID`

**`$` (Variable spéciale)**  
L'instruction `$` est utilisée pour passer des options au compilateur. Elle remplace la commande `OPTION` de la version 2.

**`VOID` / `~` (Appel sans valeur de retour)**  
Permet d'appeler une fonction en ignorant sa valeur de retour.

```
VOID INP(2)     ' Attend une touche, ignore le code
~INP(2)          ' Forme alternative plus rapide (calcule un entier)
~LEN(INPUT$(28,#1))  ' Astuce pour utiliser VOID avec des fonctions chaînes
```

La forme `~` est plus rapide que `VOID` car elle travaille en arithmétique entière. `VOID` ne peut pas être utilisé directement avec des fonctions chaînes.

### 6.7 Représentation mémoire des variables

```c
typedef enum {
    VAR_BIT,
    VAR_BYTE,
    VAR_CARD,     /* entier 16 bits */
    VAR_LONG,     /* entier 32 bits */
    VAR_FLOAT,    /* flottant simple précision (32 bits) */
    VAR_DOUBLE,   /* flottant double précision (64 bits) */
    VAR_STRING,   /* chaîne */
    VAR_ARRAY,    /* tableau */
} VarType;

typedef struct {
    VarType type;
    char *name;
    union {
        unsigned char  bit_val;       /* bit unique */
        unsigned char  byte_val;
        unsigned short card_val;
        long           long_val;
        float          float_val;
        double         double_val;
        struct {
            char   *data;
            size_t  length;           /* longueur actuelle */
            size_t  capacity;         /* capacité allouée */
        } str;
        struct {
            VarType  elem_type;
            int      num_dims;
            int     *dim_sizes;       /* tailles de chaque dimension */
            char    *data;            /* données brutes */
            size_t   total_size;      /* taille totale en octets */
        } array;
    } value;
} Variable;
```

### 6.8 Pile d'appels et contexte d'exécution

```c
typedef struct {
    ASTStmt *return_addr;      /* adresse de retour (instruction suivant le GOSUB) */
    Variable **local_vars;     /* variables locales (PROCEDURE) */
    int local_count;
} CallFrame;

typedef struct {
    CallFrame *call_stack;
    int stack_depth;
    int stack_capacity;

    /* Table des variables globales */
    Variable **globals;
    int global_count;
    int global_capacity;

    /* Données pour DATA/READ */
    ASTStmt *data_ptr;         /* pointeur dans la liste DATA */
    ASTStmt *data_base;        /* base pour RESTORE */

    /* Fichiers ouverts */
    FILE *file_handles[100];   /* 0-99 canaux */

    /* État GEM */
    int gem_app_id;            /* identifiant application AES */
    int windows[16];           /* fenêtres ouvertes (max 16 sur ST) */

    /* Curseur/position */
    int cursor_x, cursor_y;
} RuntimeContext;
```

---

## 7. Moteur d'exécution (Runtime)

### 7.1 Modes d'exécution

L'implémentation peut supporter deux modes :

1. **Exécution directe (mode interpréteur ligne)** : Chaque ligne est analysée et exécutée immédiatement. Mode compatible avec le comportement historique de GFA Basic en console.

2. **Exécution par bytecode (mode recommandé)** :
   - Le parser produit un AST
   - Une phase de compilation transforme l'AST en bytecode
   - La machine virtuelle exécute le bytecode
   - Meilleure performance, meilleure isolation des erreurs

### 7.2 Boucle d'exécution (mode bytecode)

```c
int runtime_execute(RuntimeContext *ctx, Bytecode *code) {
    size_t ip = 0; /* pointeur d'instruction */
    while (ip < code->length) {
        OpCode op = code->instructions[ip];
        switch (op) {
            case OP_PUSH_CONST:
                /* empile une constante */
                break;
            case OP_LOAD_VAR:
                /* charge une variable sur la pile */
                break;
            case OP_STORE_VAR:
                /* stocke dans une variable */
                break;
            case OP_ADD:
                /* addition */
                break;
            case OP_JMP:
                /* saut inconditionnel */
                break;
            case OP_JMP_IF_FALSE:
                /* branchement conditionnel */
                break;
            case OP_CALL:
                /* appel procédure/fonction */
                break;
            case OP_RET:
                /* retour */
                break;
            /* ... instructions spécifiques GFA ... */
            case OP_LINE:
                /* LINE x1,y1,x2,y2 */
                break;
            case OP_PRINT:
                /* PRINT */
                break;
            case OP_INPUT:
                /* INPUT */
                break;
            default:
                runtime_error("Opcode inconnu");
                return 0;
        }
        ip++;
    }
    return 1;
}
```

### 7.3 Gestion des erreurs

- Les erreurs de syntaxe doivent être rapportées avec le numéro de ligne et le contexte
- Les erreurs d'exécution doivent déclencher le gestionnaire `ON ERROR GOSUB` si défini
- Codes d'erreur compatibles TOS :
  - `ERR` : numéro d'erreur
  - `ERR$` : message d'erreur
  - `ERROR n` : déclenche l'erreur n

### 7.4 Événements

Le runtime doit supporter les événements asynchrones du GFA Basic :

```
ON BREAK GOSUB etiquette     ' interruption (Ctrl+C)
ON ERROR GOSUB etiquette     ' erreur d'exécution
ON MENU GOSUB etiquette      ' sélection de menu
EVERY t GOSUB etiquette      ' appel périodique
AFTER t GOSUB etiquette      ' appel différé unique
```

Implémentation via un mécanisme de vérification entre chaque instruction (ou dans la boucle d'événements AES).

---

## 8. Jeu d'instructions

Cette section détaille l'ensemble des instructions à implémenter, groupées par catégorie fonctionnelle.

### 8.1 Instructions de base

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `LET` | `[LET] var = expr` | Assignation (LET optionnel) |
| `PRINT` | `PRINT [#n,] [expr][;][,]...` | Affichage écran/fichier |
| `PRINT AT` | `PRINT AT(x,y); expr` | Affichage positionné |
| `PRINT USING` | `PRINT [#n,] USING format$; expr...` | Affichage formaté |
| `INPUT` | `INPUT [#n,] ["prompt";] var...` | Saisie clavier/fichier |
| `LINE INPUT` | `LINE INPUT [#n,] ["prompt";] var$` | Saisie ligne complète |
| `INKEY$` | `INK$ = INKEY$` | Lecture caractère clavier sans attente |
| `INP?` | `INP?(n)` | Lecture d'un port I/O |
| `OUT?` | `OUT? n, val` | Écriture sur un port I/O |
| `CLS` | `CLS` | Effacement écran |
| `LOCATE` | `LOCATE x, y` | Positionnement curseur |
| `POS` | `POS(n)` | Position horizontale curseur |
| `CRSCOL` | `CRSCOL` | Colonne courante du curseur |
| `CRSLIN` | `CRSLIN` | Ligne courante du curseur |
| `HTAB` | `HTAB tab_pos` | Tabulation horizontale |
| `VTAB` | `VTAB ligne` | Positionnement curseur à la ligne spécifiée (comptée à partir de 1) |

### 8.2 Contrôle de flux

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `IF...ENDIF` | `IF expr THEN ... [ELSE ...] ENDIF` | Conditionnelle |
| `FOR...NEXT` | `FOR v = deb TO fin [STEP p] ... NEXT [v]` | Boucle compteur |
| `FOR...DOWNTO` | `FOR v = deb DOWNTO fin ... NEXT [v]` | Boucle compteur décroissante (équivaut à STEP -1, mais STEP n'est pas autorisé avec DOWNTO) |
| `WHILE...WEND` | `WHILE expr ... WEND` | Boucle tant que |
| `REPEAT...UNTIL` | `REPEAT ... UNTIL expr` | Boucle jusqu'à |
| `LOOP WHILE` | `DO ... LOOP WHILE expr` | Boucle avec test final |
| `LOOP UNTIL` | `DO ... LOOP UNTIL expr` | Boucle avec test final |
| `EXIT IF` | `EXIT IF expr` | Sortie conditionnelle de boucle |
| `GOTO` | `GOTO etiquette` | Saut inconditionnel |
| `GOSUB` | `GOSUB etiquette` | Appel sous-programme |
| `RETURN` | `RETURN` | Retour de sous-programme |
| `ON ... GOSUB` | `ON n GOSUB e1, e2, ...` | Saut indexé |
| `ON ... GOTO` | `ON n GOTO e1, e2, ...` | Saut indexé |
| `SELECT...ENDSELECT` | `SELECT expr ... CASE v: ... [DEFAULT] ... ENDSELECT` | Sélection multiple |
| `STOP` | `STOP` | Arrêt du programme |
| `END` | `END` | Fin du programme |
| `QUIT` | `QUIT [n]` | Quitte le programme avec code retour n |
| `CONT` | `CONT` | Continue après STOP |
| `PAUSE` | `PAUSE [n]` | Pause (centièmes de secondes) |
| `DELAY` | `DELAY n` | Délai (millisecondes) |

### 8.3 Définition de types

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `DEFBIT` | `DEFBIT plage` | Définit type booléen par défaut |
| `DEFBYT` | `DEFBYT plage` | Définit type octet par défaut |
| `DEFWRD` | `DEFWRD plage` | Définit type entier 16 bits par défaut |
| `DEFNUM` / `DEFFLT` | `DEFNUM plage` | Définit type flottant par défaut |
| `DEFDBL` | `DEFDBL plage` | Définit type double précision par défaut |
| `DEFSTR` | `DEFSTR plage` | Définit type chaîne par défaut |
| `DEFFN` | `DEFFN f(x) = expr` | Définit une fonction une ligne |
| `FN` | `FN f(args)` | Appelle une fonction DEFFN |
| `DEFLIST` | — | Liste de valeurs |
| `DEFMARK` | `DEFMARK c, h, a` | Définit un marqueur graphique |

### 8.4 Procédures et sous-programmes

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `PROCEDURE` | `PROCEDURE nom[(args)] ... RETURN` | Définition de procédure |
| `FUNCTION` | `FUNCTION nom[(args)] ... RETURN expr ... ENDFUNC` | Définition de fonction multi-lignes |
| `ENDFUNC` | `ENDFUNC` | Termine une définition FUNCTION |
| `VAR` | `PROCEDURE nom(x, VAR y)` | Déclare un paramètre passé par référence (modifiable par la procédure) |
| `GOSUB` | `GOSUB nom` | Appel sous-programme |
| `@` | `@proc` | Synonyme de GOSUB (appel de procédure) |
| `RETURN` | `RETURN` | Retour |
| `LOCAL` | `LOCAL var1, var2, ...` | Variables locales |

### 8.5 Tableaux et données

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `DIM` | `DIM arr(dim1[, dim2, ...])` | Dimensionnement de tableau |
| `DIM?` | `DIM?(arr)` | Vérifie si un tableau est dimensionné |
| `ERASE` | `ERASE arr` | Supprime un tableau |
| `OPTION BASE` | `OPTION BASE n` | Base des indices (0 ou 1 par défaut) |
| `ARRAYFILL` | `ARRAYFILL arr, value` | Remplit un tableau avec une valeur |
| `ARRPTR` | `ARRPTR(arr)` | Adresse du descripteur de tableau |
| `DATA` | `DATA val1, val2, ...` | Définit des données |
| `READ` | `READ var1, var2, ...` | Lit les données DATA |
| `RESTORE` | `RESTORE [etiquette]` | Réinitialise le pointeur DATA |
| `_DATA` | `_DATA` / `_DATA=` | Pointeur DATA courant. `_DATA=0` si le prochain READ est hors données. |
| `CLEAR` | `CLEAR` | Efface toutes les variables |
| `CLR` | `CLR var1, var2, ...` | Efface les variables listées |

### 8.6 Opérations matricielles (MAT)

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `MAT READ` | `MAT READ a` | Lecture matrice depuis DATA |
| `MAT INPUT` | `MAT INPUT a` | Saisie matrice |
| `MAT PRINT` | `MAT PRINT a` | Affichage matrice |
| `MAT SET` | `MAT a = expr` | Affectation scalaire à tous les éléments |
| `MAT CLR` | `MAT CLR a` | Mise à zéro de la matrice |
| `MAT ONE` | `MAT ONE a` | Matrice identité |
| `MAT CPY` | `MAT a = b` | Copie de matrice |
| `MAT XCPY` | `MAT a = b * c` | Copie avec transposition |
| `MAT ADD` | `MAT a = b + c` | Addition matricielle |
| `MAT SUB` | `MAT a = b - c` | Soustraction matricielle |
| `MAT MUL` | `MAT a = b * c` | Multiplication matricielle |
| `MAT TRANS` | `MAT a = TRN(b)` | Transposition |
| `MAT INV` | `MAT a = INV(b)` | Inversion matricielle |
| `MAT DET` | `MAT DET(a)` | Déterminant |
| `MAT QDET` | `MAT QDET(a)` | Déterminant rapide |
| `MAT RANG` | `MAT RANG(a)` | Rang d'une matrice |
| `MAT NORM` | `MAT NORM(a)` | Norme d'une matrice |
| `MAT BASE` | `MAT BASE = n` | Base des indices pour les matrices |

### 8.7 Gestion des fichiers

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `OPEN` | `OPEN mode$, #n, "fichier"[, len]` | Ouvre un fichier |
| `CLOSE` | `CLOSE [#n]` | Ferme un fichier |
| `INPUT #` | `INPUT #n, var...` | Lecture fichier |
| `LINE INPUT #` | `LINE INPUT #n, var$` | Lecture ligne fichier |
| `PRINT #` | `PRINT #n, expr...` | Écriture fichier |
| `WRITE #` | `WRITE #n, expr...` | Écriture fichier avec délimiteurs |
| `GET #` | `GET #n[, pos]` | Lecture binaire positionnée |
| `PUT #` | `PUT #n[, pos]` | Écriture binaire positionnée |
| `BLOAD` | `BLOAD "fichier"[, addr]` | Charge un fichier binaire |
| `BSAVE` | `BSAVE "fichier", debut, fin` | Sauve un fichier binaire |
| `SGET` | `SGET #n` | Lecture depuis un périphérique |
| `SPUT` | `SPUT #n` | Écriture vers un périphérique |
| `BGET` | `BGET #n` | Lecture bloc binaire périphérique |
| `BPUT` | `BPUT #n` | Écriture bloc binaire périphérique |
| `FIELD` | `FIELD #n, len AS var$...` | Définit des champs dans un buffer |
| `LSET` | `LSET var$ = expr` | Affecte un champ (aligné gauche) |
| `RSET` | `RSET var$ = expr` | Affecte un champ (aligné droite) |
| `EOF` | `EOF(#n)` | Test de fin de fichier |
| `LOF` | `LOF(#n)` | Taille du fichier |
| `LOC` | `LOC(#n)` | Position courante dans le fichier |
| `SEEK` | `SEEK #n, pos` | Positionnement absolu |
| `RELSEEK` | `RELSEEK #n, offset` | Positionnement relatif |
| `EXIST` | `EXIST("fichier")` | Test d'existence d'un fichier |
| `KILL` | `KILL "fichier"` | Supprime un fichier |
| `NAME` | `NAME "anc" AS "nouv"` | Renomme un fichier |
| `MKDIR` | `MKDIR "repertoire"` | Crée un répertoire |
| `RMDIR` | `RMDIR "repertoire"` | Supprime un répertoire |
| `CHDIR` | `CHDIR "repertoire"` | Change de répertoire |
| `CHDRIVE` | `CHDRIVE n` | Change d'unité de disque |
| `DIR$` | `DIR$("masque")` | Liste un répertoire |
| `FILES` | `FILES ["masque"]` | Affiche la liste des fichiers |
| `DIR ... TO` | `DIR masque$ TO var$()` | Liste dans un tableau |
| `FSFIRST` | `FSFIRST("masque", attr)` | Premier fichier correspondant |
| `FSNEXT` | `FSNEXT` | Fichier suivant |
| `FGETDTA` | `FGETDTA` | Obtient le DTA |
| `FSETDTA` | `FSETDTA addr` | Fixe le DTA |

### 8.8 Fonctions mathématiques

| Fonction | Description |
|----------|-------------|
| `ABS(x)` | Valeur absolue |
| `SGN(x)` | Signe (-1, 0, 1) |
| `INT(x)` | Partie entière (troncature vers -∞) |
| `FRAC(x)` | Partie fractionnaire |
| `FIX(x)` | Partie entière (troncature vers 0) |
| `ROUND(x)` | Arrondi à l'entier le plus proche |
| `CEIL(x)` | Arrondi à l'entier supérieur |
| `SQR(x)` | Racine carrée |
| `EXP(x)` | Exponentielle (e^x) |
| `LOG(x)` | Logarithme népérien |
| `LOG10(x)` | Logarithme décimal |
| `SIN(x)` | Sinus (radians) |
| `COS(x)` | Cosinus (radians) |
| `TAN(x)` | Tangente (radians) |
| `ATN(x)` | Arc tangente (radians) |
| `ASIN(x)` | Arc sinus (radians) |
| `ACOS(x)` | Arc cosinus (radians) |
| `SINQ(x)` | Sinus (degrés) |
| `COSQ(x)` | Cosinus (degrés) |
| `COSH(x)` | Cosinus hyperbolique |
| `SINH(x)` | Sinus hyperbolique |
| `TANH(x)` | Tangente hyperbolique |
| `DEG(x)` | Conversion radians → degrés |
| `RAD(x)` | Conversion degrés → radians |
| `PI` | Constante π |
| `RND(x)` | Nombre aléatoire |
| `RANDOM` | Nombre aléatoire (compatible ST) |
| `RANDOMIZE [n]` | Initialise le générateur aléatoire |
| `MAX(a,b)` | Maximum |
| `MIN(a,b)` | Minimum |
| `EVEN(x)` | Teste si pair |
| `ODD(x)` | Teste si impair |
| `PRED(x)` | Prédécesseur (x - 1) |
| `SUCC(x)` | Successeur (x + 1) |
| `COMBIN(n,k)` | Combinaisons C(n,k) |
| `FACT(n)` | Factorielle |
| `CFLOAT(x)` | Conversion entier → flottant |
| `CINT(x)` | Conversion flottant → entier |

### 8.9 Fonctions de chaînes

| Fonction | Description |
|----------|-------------|
| `ASC(s$)` | Code ASCII du 1er caractère |
| `CHR$(n)` | Caractère à partir du code ASCII |
| `LEN(s$)` | Longueur de la chaîne |
| `MID$(s$, p[, n])` | Sous-chaîne (lecture/écriture) |
| `LEFT$(s$, n)` | n premiers caractères |
| `RIGHT$(s$, n)` | n derniers caractères |
| `INSTR([p,] s$, rch$)` | Recherche d'une sous-chaîne |
| `RINSTR([p,] s$, rch$)` | Recherche depuis la fin |
| `STR$(x)` | Conversion nombre → chaîne |
| `VAL(s$)` | Conversion chaîne → nombre |
| `TRIM$(s$)` | Supprime les espaces |
| `STRING$(n, s$)` | Répète une chaîne n fois |
| `SPACE$(n)` | n espaces |
| `BIN$(x)` | Conversion → binaire |
| `HEX$(x)` | Conversion → hexadécimal |
| `OCT$(x)` | Conversion → octal |
| `MKD$(x#)` | Conversion double → chaîne 8 octets |
| `MKF$(x!)` | Conversion float → chaîne 6 octets |
| `MKI$(x%)` | Conversion entier → chaîne 2 octets |
| `MKL$(x&)` | Conversion long → chaîne 4 octets |
| `MKS$(x!)` | Conversion float → chaîne 4 octets |
| `CVD(s$)` | Conversion chaîne 8 octets → double |
| `CVF(s$)` | Conversion chaîne 6 octets → float |
| `CVI(s$)` | Conversion chaîne 2 octets → entier |
| `CVL(s$)` | Conversion chaîne 4 octets → long |
| `CVS(s$)` | Conversion chaîne 4 octets → float |
| `LCASE$(s$)` | Conversion en minuscules |
| `UCASE$(s$)` | Conversion en majuscules |
| `INSERT(a$, b$)` | Insertion d'une chaîne |

### 8.10 Mémoire et pointeurs

| Instruction/Fonction | Syntaxe | Description |
|---------------------|---------|-------------|
| `PEEK` | `PEEK(addr)` | Lecture octet mémoire |
| `POKE` | `POKE addr, val` | Écriture octet mémoire |
| `DPEEK` | `DPEEK(addr)` | Lecture mot 16 bits |
| `DPOKE` | `DPOKE addr, val` | Écriture mot 16 bits |
| `LPEEK` | `LPEEK(addr)` | Lecture 32 bits |
| `LPOKE` | `LPOKE addr, val` | Écriture 32 bits |
| `SPOKE` | `SPOKE addr, val` | Écriture 16 bits (avec test) |
| `SDPOKE` | `SDPOKE addr, val` | Écriture 16 bits (avec test) |
| `SLPOKE` | `SLPOKE addr, val` | Écriture 32 bits (avec test) |
| `BYTE{addr}` | `BYTE{addr}` | Lecture octet via adresse (notation alternative) |
| `CARD{addr}` | `CARD{addr}` | Lecture mot 16 bits |
| `SINGLE{addr}` | `SINGLE{addr}` | Lecture flottant simple |
| `WORD(expr)` | `WORD(expr)` | Extraction mot de poids faible |
| `BYTE(expr)` | `BYTE(expr)` | Extraction octet de poids faible |
| `CARD(expr)` | `CARD(expr)` | Extraction 16 bits de poids faible |
| `BMOVE` | `BMOVE src, dst, len` | Copie bloc mémoire |
| `MALLOC` | `MALLOC(n)` | Allocation mémoire dynamique |
| `MFREE` | `MFREE addr` | Libération mémoire dynamique |
| `FRE(x)` | `FRE(n)` | Mémoire libre restante |
| `HIMEM` | `HIMEM` | Adresse de fin de la mémoire utilisable |
| `HIDEM` | `HIDEM n` | Réserve de la mémoire |
| `RESERVE` | `RESERVE n` | Réserve mémoire pour le système |
| `BASEPAGE` | `BASEPAGE` | Adresse de la page de base |
| `ABSOLUTE` | `ABSOLUTE var, addr` | Variable à une adresse absolue |
| `V:var` | `V:var` | Variable d'éditeur |

### 8.11 Manipulation de bits

| Fonction | Description |
|----------|-------------|
| `BTST(bit, expr)` | Teste la valeur d'un bit (0 = bit 0) |
| `BSET(bit, expr)` | Met un bit à 1 |
| `BCLR(bit, expr)` | Met un bit à 0 |
| `BCHG(bit, expr)` | Inverse un bit |
| `SHL(expr, n)` | Décalage arithmétique gauche |
| `SHR(expr, n)` | Décalage arithmétique droite |
| `ROL(expr, n)` | Rotation gauche |
| `ROR(expr, n)` | Rotation droite |

### 8.12 Opérateurs logiques bit à bit

| Opérateur | Description |
|-----------|-------------|
| `AND` | ET bit à bit |
| `OR` | OU bit à bit |
| `XOR` | OU exclusif bit à bit |
| `NOT` | Complément binaire |
| `EQV` | Équivalence binaire (XNOR) |
| `IMP` | Implication binaire |
| `MOD` | Modulo |
| `DIV` | Division entière |

### 8.13 GEM AES (Application Environment Services)

Ces fonctions sont le cœur de l'interface utilisateur graphique de l'Atari ST.

| Fonction | Syntaxe | Description |
|----------|---------|-------------|
| `APPL_INIT` | `APPL_INIT` | Initialise l'application AES |
| `APPL_EXIT` | `APPL_EXIT` | Termine l'application |
| `APPL_FIND` | `APPL_FIND(nom$)` | Cherche l'ID d'une application |
| `APPL_READ` | `APPL_READ(app_id, len, buf)` | Lit depuis un tampon d'événements |
| `APPL_WRITE` | `APPL_WRITE(app_id, len, buf)` | Écrit dans un tampon d'événements |
| `APPL_TPLAY` | `APPL_TPLAY(n, ...)` | Rejoue des entrées enregistrées |
| `APPL_TRECORD` | `APPL_TRECORD(n, ...)` | Enregistre les actions utilisateur |
| `MENU_BAR` | `MENU_BAR(arbre, show)` | Affiche/masque la barre de menus |
| `MENU_ICHECK` | `MENU_ICHECK(arbre, item, flag)` | Coche/décoche un item de menu |
| `MENU_IENABLE` | `MENU_IENABLE(arbre, item, flag)` | Active/désactive un item |
| `MENU_REGISTER` | `MENU_REGISTER(app_id, txt$)` | Enregistre l'application au menu bureau |
| `MENU_TEXT` | `MENU_TEXT(arbre, item, txt$)` | Change le texte d'un item |
| `MENU_TNORMAL` | `MENU_TNORMAL(arbre, item, n)` | Normalise un item de menu |
| `MENU` | `MENU([n])` | Affiche un menu (instruction simplifiée) |
| `MENU KILL` | `MENU KILL` | Supprime le menu |
| `MENU OFF` | `MENU OFF` | Désactive le menu |
| `FORM_ALERT` | `FORM_ALERT(def, txt$)` | Affiche une boîte d'alerte |
| `FORM_BUTTON` | `FORM_BUTTON(arbre, obj, sel, flags)` | Gère les boutons d'un formulaire |
| `FORM_CENTER` | `FORM_CENTER(arbre, x, y, w, h)` | Centre un arbre d'objets |
| `FORM_DIAL` | `FORM_DIAL(flag, x, y, w, h, xb, yb, wb, hb)` | Gère les boîtes de dialogue |
| `FORM_DO` | `FORM_DO(arbre, start_obj)` | Lance un formulaire |
| `FORM_ERROR` | `FORM_ERROR(n)` | Affiche un message d'erreur AES |
| `FORM_KEYBD` | `FORM_KEYBD(arbre, obj, nxt_obj, key)` | Gère les événements clavier formulaire |
| `FORM INPUT` | `FORM INPUT ... TO var$` | Saisie dans un champ texte |
| `FORM INPUT AS` | `FORM INPUT AS ... TO var$` | Saisie formatée |
| `OBJC_ADD` | `OBJC_ADD(pere, fils)` | Ajoute un objet à l'arbre |
| `OBJC_CHANGE` | `OBJC_CHANGE(arbre, obj, res, x, y, w, h, state, flags)` | Modifie un objet |
| `OBJC_DELETE` | `OBJC_DELETE(arbre, obj)` | Supprime un objet |
| `OBJC_DRAW` | `OBJC_DRAW(arbre, start, depth, x, y, w, h)` | Dessine un arbre d'objets |
| `OBJC_EDIT` | `OBJC_EDIT(arbre, obj, key, idx)` | Édition d'un objet |
| `OBJC_FIND` | `OBJC_FIND(arbre, start, depth, x, y)` | Cherche un objet aux coordonnées |
| `OBJC_OFFSET` | `OBJC_OFFSET(arbre, obj)` | Décale les coordonnées d'un objet |
| `OBJC_ORDER` | `OBJC_ORDER(arbre, obj, new_pos)` | Change l'ordre d'un objet |
| `OB_X` | `OB_X(arbre, obj)` | Position X d'un objet |
| `OB_Y` | `OB_Y(arbre, obj)` | Position Y d'un objet |
| `OB_W` | `OB_W(arbre, obj)` | Largeur d'un objet |
| `OB_H` | `OB_H(arbre, obj)` | Hauteur d'un objet |
| `OB_HEAD` | `OB_HEAD(arbre, obj)` | Fils aîné |
| `OB_TAIL` | `OB_TAIL(arbre, obj)` | Dernier fils |
| `OB_NEXT` | `OB_NEXT(arbre, obj)` | Frère suivant |
| `OB_TYPE` | `OB_TYPE(arbre, obj)` | Type d'objet |
| `OB_FLAGS` | `OB_FLAGS(arbre, obj)` | Flags d'objet |
| `OB_STATE` | `OB_STATE(arbre, obj)` | État d'objet |
| `OB_SPEC` | `OB_SPEC(arbre, obj)` | Spécification d'objet |
| `OB_ADR` | `OB_ADR(arbre, obj)` | Adresse de l'objet |
| `RSRC_LOAD` | `RSRC_LOAD("fichier.rsc")` | Charge un fichier ressource |
| `RSRC_FREE` | `RSRC_FREE` | Libère les ressources |
| `RSRC_GADDR` | `RSRC_GADDR(type, idx, addr)` | Obtient l'adresse d'une ressource |
| `RSRC_SADDR` | `RSRC_SADDR(type, idx, addr)` | Fixe l'adresse d'une ressource |
| `RSRC_OBFIX` | `RSRC_OBFIX(arbre, obj)` | Fixe les coordonnées d'un objet |
| `GRAF_DRAGBOX` | `GRAF_DRAGBOX(w, h, sx, sy, xc, yc, wc, hc, endx, endy)` | Rectangle de glissement |
| `RCALL` | `RCALL(n, table&())` | Appel AES générique |
| `RC_COPY` | `RC_COPY(x1,y1,x2,y2, x3,y3,x4,y4)` | Copie de rectangle |
| `RC_INTERSECT` | `RC_INTERSECT(x1,y1,x2,y2, x3,y3,x4,y4)` | Intersection de rectangles |
| `ALERT` | `ALERT n, txt$, def` | Boîte d'alerte simplifiée |
| `FILESELECT` | `FILESELECT chemin$, masque$, nom$` | Sélecteur de fichiers |
| `FSEL_INPUT` | `FSEL_INPUT(chemin$, nom$)` | Dialogue d'ouverture de fichier |

### 8.14 Gestion des fenêtres GEM

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `OPENW` | `OPENW n[, x, y, w, h]` | Ouvre une fenêtre |
| `CLOSEW` | `CLOSEW n` | Ferme une fenêtre |
| `CLEARW` | `CLEARW n` | Efface une fenêtre |
| `TITLEW` | `TITLEW n, "titre"` | Change le titre d'une fenêtre |
| `INFOW` | `INFOW n, x, y, w, h` | Redimensionne la zone de travail |
| `TOPW` | `TOPW n` | Amène une fenêtre au premier plan |
| `GETSIZE` | `GETSIZE n, x, y, w, h` | Lit les dimensions d'une fenêtre |
| `WIND_OPEN` | `WIND_OPEN(arbre, kind, x, y, w, h)` | Ouvre une fenêtre via AES |
| `WIND_CLOSE` | `WIND_CLOSE(handle)` | Ferme une fenêtre |
| `WIND_DELETE` | `WIND_DELETE(handle)` | Supprime une fenêtre |
| `WIND_FIND` | `WIND_FIND(x, y)` | Cherche une fenêtre à la position |
| `MWOUT` | `MWOUT n, x, y, w, h` | Sortie vers une zone de fenêtre |
| `SETDRAW` | `SETDRAW flag` | Définit le mode d'affichage |
| `SHOWM` | `SHOWM` | Affiche la souris |
| `HIDEM` | `HIDEM` | Cache la souris |
| `MOUSE` | `MOUSE x, y` | Positionne la souris |
| `MOUSEX` | `MOUSEX` | Position X de la souris |
| `MOUSEY` | `MOUSEY` | Position Y de la souris |
| `MOUSEK` | `MOUSEK` | État des boutons de la souris |
| `SETMOUSE` | `SETMOUSE forme$` | Change la forme du curseur |

### 8.15 Graphisme (VDI)

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `COLOR` | `COLOR fg[, bg]` | Couleur du stylo et du fond |
| `DEFFILL` | `DEFFILL c[, p, m]` | Définit le motif/couleur de remplissage |
| `DEFLINE` | `DEFLINE s[, p, m]` | Définit le style de ligne |
| `DEFTEXT` | `DEFTEXT h, orient, style` | Définit l'apparence du texte |
| `DEFMARK` | `DEFMARK c, type, h` | Définit un marqueur |
| `DEFMOUSE` | `DEFMOUSE forme$` | Définit la forme du curseur |
| `CLS` | `CLS` | Efface l'écran |
| `CLIP` | `CLIP x1, y1, w, h` | Zone de clipping |
| `ACLIP` | `ACLIP x1, y1, x2, y2` | Clipping LINE-A avancé |
| `LINE` | `LINE x1, y1, x2, y2` | Ligne |
| `ALINE` | `ALINE x1, y1, x2, y2` | Ligne avec motif |
| `HLINE` | `HLINE y, x1, x2` | Ligne horizontale |
| `BOX` | `BOX x1, y1, x2, y2` | Rectangle |
| `RBOX` | `RBOX x1, y1, x2, y2` | Rectangle arrondi |
| `PBOX` | `PBOX x1, y1, x2, y2` | Rectangle plein |
| `PRBOX` | `PRBOX x1, y1, x2, y2` | Rectangle plein |
| `CIRCLE` | `CIRCLE x, y, r[, a1, a2]` | Cercle / arc |
| `PCIRCLE` | `PCIRCLE x, y, r[, a1, a2]` | Cercle plein / secteur |
| `PELLIPSE` | `PELLIPSE x, y, rx, ry[, a1, a2]` | Ellipse pleine |
| `POLYLINE` | `POLYLINE n, xyrr&()` | Ligne polygonale |
| `POLYFILL` | `POLYFILL n, xyrr&()` | Polygone plein |
| `POLYMARK` | `POLYMARK n, xyrr&()` | Marqueurs multiples |
| `APOLY` | `APOLY n, pts&()` | Polygone à motif |
| `FILL` | `FILL x, y[, couleur_limite]` | Remplissage par diffusion (flood fill) |
| `BOUNDARY` | `BOUNDARY flag` | Définit l'encadrement du remplissage |
| `PLOT` | `PLOT x, y` | Dessine un point |
| `POINT` | `POINT(x, y)` | Lit la couleur d'un pixel |
| `PTST` | `PTST(x, y)` | Teste un point |
| `CURVE` | `CURVE n, pts&()` | Courbe de Bézier |
| `ATEXT` | `ATEXT x, y, "texte"` | Texte graphique positionné |
| `ACHAR` | `ACHAR x, y, code%` | Affiche un caractère graphique |
| `TEXT` | `TEXT x, y, "texte"[, ...]` | Texte graphique |
| `BITBLT` | `BITBLT ...` | Transfert de bloc binaire (blitter) |
| `GET` | `GET x1, y1, x2, y2, var$` | Capture une zone graphique |
| `PUT` | `PUT x, y, var$[, mode]` | Restitue une zone graphique |
| `SETCOLOR` | `SETCOLOR n, val` | Définit un registre de couleur |
| `MODE` | `MODE mode` | Change le mode d'écriture (VDI) |
| `HARDCOPY` | `HARDCOPY` | Impression d'écran |
| `CONTRL` | `CONTRL` | Adresse du tableau de contrôle VDI |
| `INTIN` | `INTIN` | Adresse du tableau d'entrée entier VDI |
| `INTOUT` | `INTOUT` | Adresse du tableau de sortie entier VDI |
| `PTSIN` | `PTSIN` | Adresse du tableau d'entrée points VDI |
| `PTSOUT` | `PTSOUT` | Adresse du tableau de sortie points VDI |
| `GINTIN` | `GINTIN` | Adresse du tableau INTIN global |
| `GINTOUT` | `GINTOUT` | Adresse du tableau INTOUT global |
| `WORK_OUT` | `WORK_OUT` | Données de la station de travail |
| `ADDRIN` | `ADDRIN` | Adresse du tableau d'entrée AES |
| `ADDROUT` | `ADDROUT` | Adresse du tableau de sortie AES |

### 8.16 Shell et processus

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `SHEL_READ` | `SHEL_READ(cmd$, tail$)` | Lit la ligne de commande |
| `SHEL_WRITE` | `SHEL_WRITE(...)` | Écrit des données pour le shell |
| `SHEL_GET` | `SHEL_GET(buf$, len)` | Lit un message du shell |
| `SHEL_PUT` | `SHEL_PUT(buf$, len)` | Envoie un message au shell |
| `SHEL_FIND` | `SHEL_FIND(buf$)` | Cherche un shell/AES |
| `SHEL_ENVRN` | `SHEL_ENVRN(env$, val$)` | Lit une variable d'environnement |
| `EXEC` | `EXEC n, ...` | Lance un programme externe |
| `CHAIN` | `CHAIN "fichier.bas"` | Chaîne un autre programme BASIC |
| `RUN` | `RUN ["fichier.bas"]` | Exécute un programme |
| `QUIT` | `QUIT` | Quitte le programme |
| `SYSTEM` | `SYSTEM [n]` | Retour au système |

### 8.17 TOS (GEMDOS, BIOS, XBIOS)

| Fonction | Syntaxe | Description |
|----------|---------|-------------|
| `GEMDOS` | `GEMDOS(fn[, arg1, arg2])` | Appel GEMDOS (environ 90 fonctions) |
| `BIOS` | `BIOS(fn[, arg1, arg2])` | Appel BIOS (environ 20 fonctions) |
| `XBIOS` | `XBIOS(fn[, arg1, arg2])` | Appel XBIOS (environ 50 fonctions) |
| `GEMSYS` | `GEMSYS(fn, ...)` | Appel système GEM générique |
| `VDISYS` | `VDISYS[opcode [,c_int,c_pts[,subopc]]]` | Appel VDI générique avec blocs CONTRL/INTIN/PTSIN |

**Fonctions XBIOS principales (table détaillée) :**

| N° | Fonction | Description |
|----|----------|-------------|
| 0 | `XBIOS(0, mode, p%, v%)` | Initialise la souris (non compatible GEM) |
| 2 | `XBIOS(2)` | Adresse de la mémoire écran physique |
| 3 | `XBIOS(3)` | Adresse de la mémoire écran logique |
| 4 | `XBIOS(4)` | Résolution écran : 0=320×200, 1=640×200, 2=640×400 |
| 5 | `XBIOS(5, L:log%, L:phys%, res%)` | Change la résolution écran (non GEM) |
| 6 | `XBIOS(6, L:adr%)` | Réinitialise tous les registres de couleur |
| 7 | `XBIOS(7, reg%, coul%)` | Lit/écrit un registre de couleur |
| 8 | `XBIOS(8, L:buf%, L:f%, drv%, sec%, trk%, side%, n%)` | Lit des secteurs disque |
| 9 | `XBIOS(9, L:buf%, L:f%, drv%, sec%, trk%, side%, n%)` | Écrit des secteurs disque |
| 10 | `XBIOS(10, L:buf%, L:f%, drv%, sec%, trk%, side%, i%, L:magic%, val%)` | Formate une piste |
| 12 | `XBIOS(12, n%, L:adr%)` | Émet des octets via MIDI |
| 14 | `XBIOS(14, dev%)` | Adresse de la table I/O série (0=RS232, 1=IKBD, 2=MIDI) |
| 15 | `XBIOS(15, baud%, hshake%, ucr%, rsr%, tsr%, scr%)` | Configure l'interface série |
| 16 | `XBIOS(16, L:keys%, L:shift%, L:caplock%)` | Change les tables de traduction clavier |
| 17 | `XBIOS(17)` | Nombre aléatoire 24 bits (0 à 16777215) |
| 18 | `XBIOS(18, L:buf%, L:ser%, type%, exec%)` | Crée un secteur de boot |
| 19 | `XBIOS(19, L:buf%, L:f%, drv%, sec%, trk%, side%, n%)` | Vérifie des secteurs |
| 20 | `XBIOS(20)` | Hardcopy — imprime l'écran |
| 21 | `XBIOS(21, cmd%, rate%)` | Configure le curseur (0=hide, 1=show, 2=blink, 4=set rate) |
| 22 | `XBIOS(22, L:time%)` | Règle date et heure (bits packés) |
| 23 | `XBIOS(23)` | Lit date et heure (bits packés) |
| 24 | `XBIOS(24)` | Réinstalle la table clavier originale |
| 25 | `XBIOS(25, n%, L:adr%)` | Envoie des octets au processeur clavier (IKBD) |
| 26 | `XBIOS(26, int%)` | Désactive une interruption MFP |
| 27 | `XBIOS(27, int%)` | Active une interruption MFP |
| 28 | `XBIOS(28, val%, reg%)` | Lit/écrit un registre du chip sonore YM-2149 |
| 29 | `XBIOS(29, bits%)` | Met à 0 des bits du port A du chip sonore |
| 30 | `XBIOS(30, bits%)` | Met à 1 des bits du port A du chip sonore |
| 31 | `XBIOS(31, t%, ctrl%, data%, L:adr%)` | Configure les timers MFP |
| 32 | `XBIOS(32, L:adr%)` | Démarre une séquence sonore en interruption |
| 33 | `XBIOS(33, config%)` | Lit/écrit les paramètres imprimante |
| 34 | `XBIOS(34)` | Adresse de la table des vecteurs clavier/MIDI |
| 35 | `XBIOS(35, delay%, rate%)` | Règle la répétition clavier |
| 36 | `XBIOS(36, L:adr%)` | Adresse du bloc de paramètres hardcopy |
| 37 | `XBIOS(37)` | Attend la prochaine interruption VBL |
| 38 | `XBIOS(38, L:adr%)` | Appelle une routine assembleur en mode superviseur |
| 39 | `XBIOS(39)` | Désactive l'AES s'il n'est pas en ROM |
| 64 | `XBIOS(64, test%)` | Contrôle/interroge le blitter (test%=-1 → statut, bit 1=blitter présent) |

### 8.18 Son

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `SOUND` | `SOUND canal, freq, duree, vol, enveloppe` | Son sur le YM-2149 |
| `BEEP` | `BEEP` | Signal sonore système |
| `WAVE` | `WAVE canal, enveloppe, forme, periode, duree, freq` | Son avec enveloppe complexe |

### 8.19 Gestion du temps et des événements

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `TIME` | `TIME` | Temps écoulé en secondes depuis boot |
| `TIMER` | `TIMER` | Minuteur en 200 Hz |
| `DATE$` | `DATE$` | Date système |
| `SETTIME` | `SETTIME mot$` | Fixe l'heure et la date |
| `DELAY` | `DELAY n` | Délai en millisecondes |
| `PAUSE` | `PAUSE [n]` | Pause (centièmes de secondes) |
| `EVERY` | `EVERY t GOSUB etiq` | Appel périodique |
| `EVERY OFF` | `EVERY OFF` | Désactive les appels périodiques |
| `AFTER` | `AFTER t GOSUB etiq` | Appel différé |
| `AFTER OFF` | `AFTER OFF` | Annule les appels différés |
| `ON BREAK` | `ON BREAK GOSUB etiq` | Gestion Ctrl+C |
| `ON BREAK CONT` | `ON BREAK CONT` | Continue après Ctrl+C |
| `ON ERROR` | `ON ERROR GOSUB etiq` | Gestionnaire d'erreurs |
| `ERR` | `ERR` | Dernier code d'erreur |
| `ERR$` | `ERR$` | Message d'erreur |
| `ERROR` | `ERROR n` | Déclenche une erreur |
| `FATAL` | `FATAL` | Erreur fatale |
| `EVNT_MULTI` | `EVNT_MULTI(...)` | Événement multiple AES |
| `EVNT_MESAG` | `EVNT_MESAG(buf$)` | Message AES |
| `EVNT_KEYBD` | `EVNT_KEYBD` | Événement clavier AES |
| `EVNT_MOUSE` | `EVNT_MOUSE(flags, x, y, w, h, mx, my, btn, kstate, kret)` | Événement souris clavier AES |
| `EVNT_BUTTON` | `EVNT_BUTTON(clicks, mask, state, mx, my, btn, kstate)` | Événement boutons AES |
| `EVNT_TIMER` | `EVNT_TIMER(time)` | Minuteur AES |
| `EVNT_DCLICK` | `EVNT_DCLICK(rate, set)` | Double-clic AES |
| `ON MENU` | `ON MENU` | Active la gestion des menus |
| `ON MENU BUTTON` | `ON MENU BUTTON n` | Associe un numéro de menu |
| `ON MENU GOSUB` | `ON MENU GOSUB n` | Routine pour élément de menu |
| `ON MENU IBOX GOSUB` | `ON MENU IBOX GOSUB` | Routine pour boîte d'information |
| `ON MENU KEY GOSUB` | `ON MENU KEY GOSUB n` | Routine pour raccourci clavier |
| `ON MENU MESSAGE GOSUB` | `ON MENU MESSAGE GOSUB` | Routine pour message AES |
| `ON MENU OBOX GOSUB` | `ON MENU OBOX GOSUB` | Routine pour boîte de sortie |

### 8.20 Autres instructions

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `MONITOR` | `MONITOR` | Appelle le moniteur machine |
| `DEBUG` | `DEBUG` | Active le débogueur |
| `KEYDEF` | `KEYDEF n, chaine$` | Définit une touche de fonction |
| `KEYGET` | `KEYGET` | Lit une touche de fonction |
| `KEYLOOK` | `KEYLOOK` | Teste si une touche de fonction est enfoncée |
| `KEYPAD` | `KEYPAD` | Teste le pavé numérique |
| `KEYPRESS` | `KEYPRESS` | Simule une pression de touche |
| `KEYTEST` | `KEYTEST` | Teste une touche |
| `JOYSTICK` / `STICK` | `STICK(n)` | État du joystick |
| `STRIG` | `STRIG(n)` | Bouton de joystick |
| `PADX` | `PADX` | Position X de la tablette graphique |
| `PADY` | `PADY` | Position Y de la tablette graphique |
| `PADT` | `PADT` | Contact de la tablette graphique |
| `LPENX` | `LPENX` | Position X du crayon optique |
| `LPENY` | `LPENY` | Position Y du crayon optique |
| `TOUCH` | `TOUCH(n)` | Contact crayon optique |
| `SPRITE` | `SPRITE n, x, y, forme$` | Affiche un sprite |
| `STORE` | `STORE "fichier"` | Sauvegarde du programme en mémoire |
| `RECALL` | `RECALL "fichier"` | Rappel du programme sauvegardé |
| `PSAVE` | `PSAVE "fichier"` | Sauvegarde de l'image écran |
| `TRACE$` | `TRACE$` | Trace d'exécution |
| `TRON` | `TRON` | Active la trace |
| `TROFF` | `TROFF` | Désactive la trace |
| `QSORT` | `QSORT base&(), el&(), max` | Tri rapide d'un tableau |
| `SSORT` | `SSORT base&(), el&(), max` | Tri shell d'un tableau |
| `MSHRINK` | `MSHRINK(debut, fin)` | Réduit la mémoire occupée |

### 8.21 Instructions éditeur (mode interactif / IDE)

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `LIST` | `LIST [l1[-l2]]` | Liste le programme |
| `LLIST` | `LLIST [l1[-l2]]` | Imprime le programme |
| `NEW` | `NEW` | Efface le programme en mémoire |
| `LOAD` | `LOAD "fichier.bas"` | Charge un programme |
| `SAVE` | `SAVE "fichier.bas"` | Sauvegarde un programme |
| `RUN` | `RUN ["fichier.bas"]` | Exécute un programme en mémoire |
| `CHAIN` | `CHAIN "fichier.bas"` | Chaîne un autre programme |
| `MERGE` | `MERGE "fichier.bas"` | Fusionne deux programmes |
| `RENUM` | `RENUM [debut, pas]` | Renumérote les lignes |
| `DELETE` | `DELETE l1[-l2]` | Supprime des lignes |
| `AUTO` | `AUTO [debut, pas]` | Numérotation automatique |

### 8.22 Fonctions de test et conversion numériques

| Fonction | Description |
|----------|-------------|
| `EVEN(x)` | Vrai si pair |
| `ODD(x)` | Vrai si impair |
| `PRED(x)` | Prédécesseur |
| `SUCC(x)` | Successeur |
| `TRUNC(x)` | Partie entière (troncature vers 0) |
| `CFLOAT(x)` | Conversion en flottant |
| `CINT(x)` | Conversion en entier |
| `CVL(s$)` | Chaîne → entier long |
| `CVI(s$)` | Chaîne → entier |
| `CVS(s$)` | Chaîne → flottant simple |
| `CVF(s$)` | Chaîne → flottant |
| `CVD(s$)` | Chaîne → double |
| `TYPE(ptr)` | Retourne le type de la variable à l'adresse `ptr` : 0=var, 1=var$, 2=var%, 3=var!, 4=var(), 5=var$(), 6=var%(), 7=var!(). -1 si erreur |
| `TT?` | Retourne -1 si processeur 68020/030, 0 sinon |
| `TRUE` | Constante -1 (vrai) |
| `FALSE` | Constante 0 (faux) |

### 8.23 Opérateurs additionnels

| Opérateur | Syntaxe | Description |
|-----------|---------|-------------|
| `@` | `@procedure` | Synonyme de `GOSUB` (appel de procédure) |
| `==` | `a == b` | Comparaison approximative : seuls 28 bits de mantisse sont comparés (~8,5 chiffres) |
| `W:` | `W:expr` | Passe une expression numérique comme mot 16 bits aux routines OS/C |
| `L:` | `L:expr` | Passe une expression numérique comme mot long 32 bits aux routines OS/C |
| `{}` | `BYTE{addr}`, `CARD{addr}`, `LONG{addr}`, `FLOAT{addr}`, `SINGLE{addr}`, `DOUBLE{addr}`, `CHAR{addr}`, `INT{addr}` | Accès mémoire typé par adresse absolue |

### 8.24 Graphisme turtle (DRAW)

GFA Basic intègre un mode graphique « turtle » inspiré de LOGO :

```
DRAW "PU FD 40 PD FD 40"
```

| Commande | Description |
|----------|-------------|
| `FD n` | Forward — avance de n pixels |
| `BK n` | Backward — recule de n pixels |
| `SX x` / `SY y` | Scale — échelle du mouvement |
| `LT a` | Left Turn — tourne à gauche de a degrés |
| `RT a` | Right Turn — tourne à droite de a degrés |
| `TT a` | Turn To — oriente le stylo à l'angle absolu a |
| `MA x,y` | Move Absolute — déplace le stylo aux coordonnées absolues |
| `DA x,y` | Draw Absolute — trace jusqu'aux coordonnées absolues |
| `MR x,y` | Move Relative — déplacement relatif |
| `DR x,y` | Draw Relative — tracé relatif |
| `PU` | Pen Up — lève le stylo |
| `PD` | Pen Down — abaisse le stylo |
| `CO c` | Colour — change la couleur |

Fonctions d'interrogation : `DRAW(0)` = position X, `DRAW(1)` = position Y, `DRAW(2)` = angle en degrés.

`SETDRAW x,y,w` est un raccourci pour `DRAW "MA",x,y,"TT",w`.

### 8.25 Commandes STE spécifiques

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `DMACONTROL` | `DMACONTROL ctrl` | Contrôle le son DMA STE : 0=stop, 1=jouer une fois, 2=jouer en boucle |
| `DMASOUND` | `DMASOUND debut, fin, freq[, ctrl]` | Sortie son DMA échantillonné STE. freq : 0=6.25 kHz, 1=12.5 kHz, 2=25 kHz, 3=50 kHz |

### 8.26 Commandes de manipulation de tableaux

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `DELETE` | `DELETE x(i)` | Supprime le i-ème élément du tableau x. Les éléments suivants sont décalés vers le bas. |
| `INSERT` | `INSERT x(i), val` | Insère `val` à la position i du tableau x. Les éléments suivants sont décalés vers le haut. |
| `SWAP` | `SWAP a, b` | Échange les valeurs de deux variables |

### 8.27 Fonctions chaînes additionnelles

| Fonction | Description |
|----------|-------------|
| `UPPER$(s$)` | Convertit tous les caractères minuscules en majuscules |
| `LCASE$(s$)` / `LOWER$(s$)` | Convertit tous les caractères majuscules en minuscules |
| `VAL?(s$)` | Retourne le nombre de caractères en début de chaîne pouvant être convertis en nombre |
| `VARIAT(n,k)` | Calcul du nombre d'arrangements : n!/(n-k)! |

### 8.28 Instructions PROCEDURE et FUNCTION

| Instruction | Syntaxe | Description |
|------------|---------|-------------|
| `FUNCTION` | `FUNCTION nom[(args)] ... RETURN expr ... ENDFUNC` | Définit une fonction multi-lignes retournant une valeur |
| `VAR` | `PROCEDURE nom(x, VAR y)` | Déclare un paramètre passé par référence (modifiable) |
| `ENDFUNC` | `ENDFUNC` | Termine une définition de fonction |
| `@` | `@nom` | Appel de procédure (synonyme de GOSUB) |

### 8.29 Commandes GDOS et VDI étendues

Ces fonctions nécessitent GDOS chargé et un fichier ASSIGN.SYS valide :

| Fonction | Description |
|----------|-------------|
| `V_OPNWK(id)` | Ouvre une station de travail physique et retourne son handle |
| `V_CLSWK()` | Ferme la station de travail courante |
| `V_OPNVWK(id, ...)` | Ouvre une station de travail virtuelle |
| `V_CLSVWK(id)` | Ferme une station de travail virtuelle |
| `V_CLRWK()` | Efface le buffer de sortie (écran ou imprimante) |
| `V_UPDWK()` | Envoie les instructions graphiques bufferisées au périphérique |
| `V~H` | Handle VDI interne de GFA-Basic (lecture/écriture). `V~H=-1` réinitialise. |
| `VSETCOLOR` | `VSETCOLOR reg, r, g, b` — corrige le mapping des registres de couleur (SETCOLOR a un bug dans TOS). Voir table de correspondance ci-dessous. |
| `VQT_EXTENT(text$)` | Retourne le rectangle englobant d'un texte dans PTSOUT(7) |
| `VQT_NAME(i, nom$)` | Retourne le handle et le nom de la police chargée n°i |
| `VST_LOAD_FONTS(x)` | Charge les polices additionnelles spécifiées dans ASSIGN.SYS |
| `VST_UNLOAD_FONTS(x)` | Décharge les polices |
| `VSYNC` | Attend la prochaine impulsion de synchronisation verticale (anti-flicker) |

**Table de correspondance VSETCOLOR :**

Basse résolution : `SETCOLOR 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15` → `VSETCOLOR 0 2 3 6 4 7 5 8 9 10 11 14 12 15 13 1`

### 8.30 Bibliothèque Window étendue

| Fonction | Description |
|----------|-------------|
| `WIND_CREATE(attr, x, y, w, h)` | Alloue une nouvelle fenêtre et retourne son handle |
| `WIND_CALC(type, attr, ix, iy, iw, ih, ox, oy, ow, oh)` | Calcule la taille totale ou la zone de travail d'une fenêtre (type : 0=taille totale, 1=zone travail) |
| `WIND_GET(handle, code, w1, w2, w3, w4)` | Fournit des informations sur une fenêtre. Codes : 4=zone travail, 5=taille totale, 6=taille précédente, 7=taille max, 8=slider horizontal, 9=slider vertical, 10=fenêtre active, 11/12=rectangles, 15/16=taille sliders |
| `WIND_SET(handle, code, w1, w2, w3, w4)` | Modifie une fenêtre. Codes : 1=composants, 2=titre, 3=ligne info, 5=taille, 10=fenêtre active, 14=arbre menu bureau |
| `WIND_UPDATE(flag)` | Coordonne les redraws d'écran (menus déroulants). flag : 0=redraw fini, 1=redraw commence, 2=perte souris, 3=prise souris |
| `W_HAND(n)` | Retourne le handle GEM de la fenêtre n° n |
| `W_INDEX(handle)` | Retourne le numéro de fenêtre GFA correspondant au handle GEM |
| `WINDTAB` / `WINDTAB(i,j)` | Adresse de la table des paramètres de fenêtres (voir structure détaillée ci-dessous) |

### 8.31 Structure WINDTAB

La table WINDTAB contient les paramètres de toutes les fenêtres. Chaque entrée fait 12 octets (6 mots de 16 bits) :

```
Offset  Fenêtre n
0       Handle GEM
2       Attributs (bits 0-11 : titre, close box, full box, move box, ligne info, 
                  sizing box, flèches haut/bas, slider vertical, flèches 
                  gauche/droite, slider horizontal)
4       Coordonnée X
6       Coordonnée Y
8       Largeur
10      Hauteur
```

Après les 4 entrées de fenêtres, on trouve :
- Offset 48 : -1 (marqueur de fin)
- Offset 52-58 : Coordonnées et taille du bureau
- Offset 60-63 : Coordonnées de la jointure des 4 fenêtres
- Offset 64-65 : Origine pour les instructions graphiques (CLIP OFFSET)

---

## 9. Environnement d'exécution graphique

### 9.1 Émulation de l'Atari ST

L'implémentation en C devra abstraire la couche d'affichage pour permettre plusieurs backends :

#### Backend SDL2 (recommandé)

```c
/* Dimensions de l'écran Atari ST */
#define ST_SCREEN_WIDTH   640
#define ST_SCREEN_HEIGHT  400
#define ST_SCREEN_WIDTH_LO  320
#define ST_SCREEN_HEIGHT_LO 200

/* Modes graphiques */
#define ST_MODE_LOW    0  /* 320x200, 16 couleurs */
#define ST_MODE_MEDIUM 1  /* 640x200, 4 couleurs */
#define ST_MODE_HIGH   2  /* 640x400, monochrome */

/* Palette ST (16 couleurs) */
static const unsigned long st_palette[16] = {
    0x000000, /* 0:  Noir */
    0x000080, /* 1:  Bleu foncé */
    0x008000, /* 2:  Vert foncé */
    0x008080, /* 3:  Cyan foncé */
    0x800000, /* 4:  Rouge foncé */
    0x800080, /* 5:  Magenta foncé */
    0x808000, /* 6:  Marron */
    0xC0C0C0, /* 7:  Gris clair */
    0x808080, /* 8:  Gris foncé */
    0x0000FF, /* 9:  Bleu clair */
    0x00FF00, /* 10: Vert clair */
    0x00FFFF, /* 11: Cyan clair */
    0xFF0000, /* 12: Rouge clair */
    0xFF00FF, /* 13: Magenta clair */
    0xFFFF00, /* 14: Jaune */
    0xFFFFFF, /* 15: Blanc */
};
```

#### Couche d'abstraction

```c
/* Interface générique d'affichage */
typedef struct {
    int  (*init)(int mode);
    void (*shutdown)(void);
    void (*clear)(int color);
    void (*set_pixel)(int x, int y, int color);
    int  (*get_pixel)(int x, int y);
    void (*draw_line)(int x1, int y1, int x2, int y2);
    void (*draw_text)(int x, int y, const char *text);
    void (*fill_rect)(int x, int y, int w, int h, int color);
    void (*update)(void);
    void (*set_palette)(int index, unsigned long rgb);
    int  (*get_event)(void);
} DisplayDriver;
```

### 9.2 Émulation VDI

Le VDI (Virtual Device Interface) de l'Atari ST doit être émulé en mappant ses fonctions sur le backend graphique.

```c
/* Fonctions VDI essentielles */
int vdi_v_opnwk(int *work_in, int *handle, int *work_out);
int vdi_v_clswk(int handle);
int vdi_v_clrwk(int handle);
int vdi_v_pline(int handle, int count, int *pxy);
int vdi_v_bar(int handle, int *pxy);
int vdi_v_circle(int handle, int x, int y, int radius);
int vdi_v_gtext(int handle, int x, int y, const char *text);
int vdi_vsf_color(int handle, int color);
int vdi_vsl_color(int handle, int color);
int vdi_vst_color(int handle, int color);
/* ... (environ 80 fonctions VDI) */
```

### 9.3 Émulation GEMDOS

Les fonctions GEMDOS contrôlent les fichiers, les processus et la gestion mémoire TOS :

```c
/* Identifiants des fonctions GEMDOS */
#define GEMDOS_PTERM     0x00   /* Terminer le processus */
#define GEMDOS_CCONIN    0x01   /* Entrée console */
#define GEMDOS_CCONOUT   0x02   /* Sortie console */
#define GEMDOS_FOPEN     0x3D   /* Ouvrir un fichier */
#define GEMDOS_FCLOSE    0x3E   /* Fermer un fichier */
#define GEMDOS_FREAD     0x3F   /* Lire un fichier */
#define GEMDOS_FWRITE    0x40   /* Écrire un fichier */
#define GEMDOS_FSEEK     0x42   /* Chercher dans un fichier */
#define GEMDOS_FGETDTA   0x2F   /* Lire DTA */
#define GEMDOS_FSETDTA   0x1A   /* Fixer DTA */
/* ... (environ 90 fonctions) */

long gemdos_call(int fn, long arg1, long arg2);
```

### 9.4 Émulation XBIOS

Le XBIOS gère des fonctions bas niveau propres à l'Atari ST (son, changement de résolution, lignes d'interruption, etc.) :

```c
/* Fonctions XBIOS essentielles */
#define XBIOS_GETRES    4    /* Obtenir la résolution écran */
#define XBIOS_SETSCREEN 5    /* Changer la résolution */
#define XBIOS_DOSOUND   32   /* Émettre un son */
#define XBIOS_GIACCESS  28   /* Accès aux paramètres graphiques */
/* ... (environ 50 fonctions) */

long xbios_call(int fn, long arg);
```

### 9.5 Émulation BIOS

```c
/* Fonctions BIOS */
#define BIOS_BCONIN    2      /* Entrée console */
#define BIOS_BCONOUT   3      /* Sortie console */
#define BIOS_BCOSTAT   1      /* Statut entrée console */
#define BIOS_RWABS     4      /* Lecture/écriture absolue disque */
#define BIOS_SETEXC    5      /* Fixer vecteur d'exception */
/* ... (environ 20 fonctions) */

long bios_call(int fn, long arg1, long arg2);
```

---

## 10. L'éditeur intégré

### 10.1 Comportement de l'éditeur GFA Basic 3.0

L'éditeur GFA Basic 3.0 n'est pas un éditeur de texte standard mais un environnement spécialisé :

- **Vérification syntaxique à la volée** : À chaque fois que le curseur quitte une ligne, une vérification syntaxique est effectuée. Si la ligne est incorrecte, le message `Syntax error` apparaît et le curseur ne peut pas quitter la ligne (sauf en transformant la ligne en commentaire avec `'`).
- **Indentation automatique** : Les instructions à l'intérieur de boucles et conditions sont automatiquement indentées.
- **Expansion d'abréviations** : `p` est automatiquement expansé en `PRINT`, `?` en `PRINT`, etc.
- **Formatage automatique** : Les espaces redondants sont supprimés (`2 + 2` devient `2+2`). La capitalisation est ajustée selon le réglage `DEFLIST`.
- **Repliement de code (Folding)** : Les procédures et fonctions peuvent être « pliées » (raccourcies à leur seule déclaration) via la touche `Help`. Un `>` en début de ligne indique le pliage.

### 10.2 Limites de l'éditeur

| Caractéristique | Limite |
|----------------|--------|
| Longueur max d'une ligne | 254 caractères (pas 255 comme documenté) |
| Nombre max de lignes | Recommandé < 65535 (bugs au-delà) |
| Lignes supportées par Ctrl+G | 0 à 999999 en entrée, mais échoue si > 65535 |
| Recherche (F6) | Incorrecte sur les lignes > 65535 |
| Ctrl+Z (fin de listing) | Fonctionne mais numéro de ligne incorrect si > 65535 |

### 10.3 Raccourcis clavier principaux

| Touche | Action |
|--------|--------|
| Flèches | Déplacement curseur |
| Insert | Insère une ligne blanche |
| Clr/Home | Début de l'écran |
| Ctrl+Clr/Home | Début du listing |
| Undo | Annule les modifications de la ligne courante (non confirmée) |
| Help | Plie/déplie une procédure ou fonction |
| Shift+F10 | Exécute le programme (RUN) |

### 10.4 Mode interactif (commandes éditeur)

En mode direct (hors programme), les commandes suivantes sont disponibles :

| Commande | Syntaxe | Description |
|----------|---------|-------------|
| `LIST` | `LIST [l1[-l2]]` | Liste le programme |
| `LLIST` | `LLIST [l1[-l2]]` | Imprime le listing |
| `NEW` | `NEW` | Efface le programme en mémoire |
| `LOAD` | `LOAD "fichier.bas"` | Charge un programme |
| `SAVE` | `SAVE "fichier.bas"` | Sauvegarde un programme (format tokenisé) |
| `SAVE,A` | `SAVE,A "fichier.asc"` | Sauvegarde en ASCII |
| `RUN` | `RUN ["fichier"]` | Exécute le programme |
| `MERGE` | `MERGE "fichier"` | Fusionne un programme ASCII |
| `DELETE` | `DELETE l1[-l2]` | Supprime des lignes |
| `RENUM` | `RENUM [début, pas]` | Renumérote les lignes |
| `AUTO` | `AUTO [début, pas]` | Numérotation automatique |
| `CHAIN` | `CHAIN "fichier.bas"` | Chaîne un autre programme |
| `TRON` | `TRON` | Active la trace d'exécution |
| `TROFF` | `TROFF` | Désactive la trace |
| `CONT` | `CONT` | Continue après un STOP |

### 10.5 Fonctions de debugging

| Instruction | Description |
|------------|-------------|
| `TRON` | Active la trace : chaque ligne exécutée est affichée |
| `TROFF` | Désactive la trace |
| `TRACE$` | Variable contenant la dernière ligne tracée |
| `STOP` | Arrête l'exécution (reprise possible avec `CONT`) |
| `MONITOR` | Appelle le moniteur machine intégré |

---

## 11. Codes d'erreur

### 11.1 Codes d'erreur de l'éditeur

Messages affichés lors de la vérification syntaxique à la volée :

```
missing Select          missing Wend            missing Until
missing Loop            missing Next            missing While
missing Repeat          missing Do              missing For
missing Endif           missing If              Exit without loop
missing Return          Procedure in loop       Procedure redefined
missing Endfunc         Function in loop        Function redefined
missing Procedure       Label redefined         Local without Procedure/Function
Local in loop           Goto into/outof For-Next, Procedure or Function
Resume in For-Next      Resume without Procedure
missing Function        Syntax Error            Line too long
```

### 11.2 Codes d'erreur de l'interpréteur

```
  0  Division by zero
  1  Overflow
  2  Not Integer -2147483648 .. 2147483647
  3  Not Byte 0 .. 255
  4  Not Word -32768 .. 32767
  5  Square root only for positive numbers
  6  Logarithm only for numbers greater than zero
  7  (réservé)
  8  Out of memory
  9  Function or command not yet implemented
 10  String too long max. 32767 characters
 11  Not GFA-BASIC 3.5 program
 12  Program too long memory full NEW
 13  Not GFA-BASIC program file too short NEW
 14  Array dimensioned twice
 15  Array not dimensioned
 16  Array index too large
 17  Dim index too large
 18  Wrong number of indices
 19  Procedure not found
 20  Label not found
 21  On Open only "I"nput "O"utput "R"andom "A"ppend "U"pdate allowed
 22  File already open
 23  File # wrong
 24  File not open
 25  Input wrong not numeric
 26  End of file reached
 27  Too many points for Polyline/Polyfill/Polymark max. 128
 28  Array must have one dimension
 29  Number of points too large for array
 30  Merge - Not an ASCII file
 31  Merge - Line too long aborted
 32  ==> Syntax error program aborted
 33  Undefined label
 34  Out of data
 35  Data not numeric
 36  (retiré, v1/v2 uniquement)
 37  Disk full
 38  Command not allowed in direct mode
 39  Program error Gosub not possible
 40  Clear not allowed in For-Next loops or Procedures
 41  Cont not possible
 42  Parameter missing
 43  Expression too complex
 44  Undefined function
 45  Too many parameters
 46  Parameter wrong must be a number
 47  Parameter wrong must be a string
 48  Open "R" Record length wrong
 49  Too many "R"-files (max 31)
 50  Not an "R"-File
 51  (retiré)
 52  Fields larger than record length
 53  (retiré)
 54  GET/PUT Field string length changed
 55  GET/PUT Record number wrong
 56  (retiré)  57-59 (réservé)
 60  Sprite String length wrong
 61  Error while RESERVE
 62  MENU error
 63  RESERVE error (jamais utilisé)
 64  Pointer (*x) error
 65  Array too small (<256)
 66  No VAR-Array
 67  ASIN/ACOS error
 68  VAR-Type mismatch
 69  ENDFUNC without RETURN
 70  (réservé)
 71  Index too large
 72  Invalid DATA pointer (non documenté)
 73-79  (réservé)
 80  Matrix operations for one and two dimensional arrays only
 81  Matrices are of different order
 82  Vector product not defined
 83  Matrix product not defined
 84  Scalar product not defined
 85  Transposition for two dimensional arrays only
 86  Non square matrix
 87  Transposition not defined
 88  FACT/COMBIN/VARIAT not defined
 89  (réservé)
 90  LOCAL error
 91  FOR error
 92  Resume (next) not possible Fatal, For or Local
 93  Stack error
 94-97  (réservé)
 98  Command only available on STE
 99  (réservé dans le code original ; RUN!Lib l'utilise)
100  GFA BASIC Version 3.6 TT E (bannière de version)
```

Codes non utilisés dans le code original : #9, #25, #40, #44, #63, #91  
Doublon : #20 ≡ #33  
Retirés de v1/v2 : #36, #51, #53, #55

### 11.3 Codes d'erreur BIOS

```
 -1  General error           -10  Write fault
 -2  Drive not ready         -11  Read fault
 -3  Unknown command         -12  General error 12
 -4  CRC error               -13  Write protected
 -5  Bad request             -14  Media change detected
 -6  Seek error              -15  Unknown device
 -7  Unknown media           -16  Bad sector (verify)
 -8  Sector not found        -17  Insert other disk
 -9  Out of paper
```

### 11.4 Codes d'erreur GEMDOS

```
-32  Invalid function number     -49  No more files
-33  File not found              -64  GEMDOS range error
-34  Path not found              -65  GEMDOS internal error
-35  Too many open files         -66  Invalid executable file format
-36  Access denied               -67  Memory block growth failure
-37  Invalid handle              -128 Program stopped by break key
-39  Out of memory                  (compilé uniquement)
-40  Invalid memory block address
-46  Invalid drive specification
```

### 11.5 Codes d'erreur Bomb (68000)

```
101  (réservé)
102  2 bombs - bus error       (PEEK/POKE invalide possible)
103  3 bombs - address error   (adresse impaire : DPOKE/DPEEK/LPOKE/LPEEK)
104  4 bombs - illegal instruction
105  5 bombs - divide by zero  (code machine 68000)
106  6 bombs - CHK exception
107  7 bombs - TRAPV exception
108  8 bombs - privilege violation
109  9 bombs - trace exception
```

### 11.6 Gestion des erreurs dans le runtime

- `ERR` : retourne le dernier code d'erreur
- `ERR$` : retourne le message d'erreur correspondant
- `ERROR n` : déclenche l'erreur n
- `ON ERROR GOSUB etiquette` : définit le gestionnaire d'erreurs
- `RESUME [NEXT]` : reprend l'exécution après une erreur
- `FATAL` : erreur fatale, termine le programme

---

## 12. Compatibilité et différences

### 12.1 Compatibilité GFA Basic v2 → v3

Les programmes GFA Basic v2 doivent être sauvegardés en ASCII (`SAVE,A`) puis chargés dans v3 avec `MERGE`.

**Différences notables :**

| Élément | v2 | v3 |
|---------|----|----|
| `MUL`/`DIV` | Acceptent les flottants | Entiers uniquement (plus rapide) |
| `PRINT USING` | Formats erronés si nombre trop long | Affiche ce qui tient dans le format |
| `CLS` | — | Émet `ESC-E-CR` pour compatibilité `TAB()` |
| `KEYPAD` | — | Nécessite `KEYPAD 0` pour désactiver la capture Alt/Control |
| `MOUSEX`/`MOUSEY` | `CARD()` retournait les négatifs | Fenêtrées : coordonnées négatives possibles hors fenêtre |
| `OPTION` | Contrôlait le compilateur | Remplacé par `$` en v3 |

### 12.2 Compatibilité GFA Basic 3.5 ↔ 3.6

Fonctions ajoutées en v3.6 (non disponibles en v3.5) :
- `_C`, `_X`, `_Y`
- `GETSIZE()`

La v3.5 peut charger des fichiers tokenisés v3.6, mais les fonctions ci-dessus afficheront du charabia et risquent de crasher à l'exécution. Pour migrer de v3.6 vers v3.5, sauvegarder en ASCII puis faire `MERGE`.

### 12.3 Différences Interpréteur / Compilateur

**Commandes non acceptées par le compilateur :**
- `TRON`, `TROFF`, `TRACE$` (debugging)
- `DEFLIST` (ignoré)
- `LOAD`, `SAVE`, `PSAVE`, `LIST`, `LLIST` (pas de sens en compilé)
- `DUMP` (les variables ne sont pas accessibles par nom)
- `==>` (marqueur de ligne erronée)

**Différences de comportement :**
- `CHAIN` : En compilé, appelle `SHEL_WRITE()` puis `QUIT`; le programme chaîné est lancé par le shell
- `STOP` retourne le code d'erreur -128 en compilé
- L'éditeur initialise toujours l'AES et le VDI ; le compilateur ne le fait que si des appels AES/VDI sont présents
- `~INP(2)` peut échouer sous MiNT si l'AES n'est pas initialisé

---

## 13. Problèmes connus et pièges

### 13.1 Fonctionnalités non implémentées

GFA Basic 3.x ne supporte pas (en raison de son âge) :
- FPU 68881/68882 (partiellement)
- CPU 68010/020/030/040/060 (partiellement)

### 13.2 Commandes et fonctions problématiques (liste non exhaustive)

De nombreuses commandes ont des comportements incorrects ou des bugs documentés :

| Commande | Problème connu |
|----------|---------------|
| `ACOS()`, `ASIN()` | Limitations numériques |
| `AFTER GOSUB`, `EVERY GOSUB` | Comportement instable |
| `APPL_TPLAY()` | Implémentation incomplète |
| `BITBLT` | Divers bugs |
| `DEFFN` | Problèmes avec les variables locales (voir « The local=FN bug ») |
| `DIM`, `ERASE` | Messages d'erreur incorrects (#14, #17) |
| `DIM` dans une `FUNCTION` | Écrase les variables `LOCAL` |
| `EXIT IF FALSE` | Bug dans le compilateur |
| `FILESELECT` | Nécessite ≥ 32500 octets libres en compilé |
| `GET #`, `PUT #` | Changement de longueur des champs |
| `GETSIZE()` | Problèmes de compatibilité |
| `IF-THEN` | Bug de compilation sur une ligne |
| `INLINE` | Divers problèmes |
| `INSTR()`, `RINSTR()` | Problèmes avec certains cas limites |
| `MAT DET`, `MAT QDET`, `MAT RANG` | Ne réagissent pas à Break |
| `MAT INPUT` | Crasha le compilateur |
| `MAT PRINT` | Émet des virgules au lieu de retours à la ligne en compilé |
| `MAT MUL` | Ne réagit pas à Break |
| `OBJC_CHANGE()` | Comportement incorrect |
| `OPTION BASE 1` | Bug sérieux en compilé — utiliser `OPTION BASE 0` |
| `ON BREAK GOSUB`, `ON ERROR GOSUB` | Comportement parfois imprévisible |
| `ON MENU BUTTON` | Problèmes de gestion |
| `ON MENU KEY` | Problèmes de gestion |
| `ON GOSUB` | Bug selon le contexte |
| `QSORT`, `SSORT` | Ne trient que les nombres positifs |
| `RESUME label` | Fonctionne uniquement dans les PROCEDURE sans `LOCAL` |
| `SOUND`, `WAVE` | Instables |
| `SPRITE` | Longueur de chaîne incorrecte |
| `STR$()` | Affichage incorrect pour exposant 1000 (`2.5E+:00`) |
| `VSETCOLOR` | Non fonctionnel dans certaines configurations |
| `V_OPNWK()`, `V_OPNVWK()` | Problèmes sous certaines résolutions |

### 13.3 Pièges de portage

- **`TRUE` = -1** en GFA Basic, pas 1 comme dans d'autres langages
- **`OPTION BASE 1`** est fondamentalement buggé en compilé. Toujours utiliser `OPTION BASE 0`.
- Les **booléens en tableau** sont stockés en ordre inversé (bits de poids faible en premier)
- **`DIM` avec 8 dimensions** crashe le compilateur
- La **limite de 65535 éléments** pour les tableaux multi-dimensionnels n'est pas correctement vérifiée
- `~INP(2)` sous MiNT échoue si l'AES n'est pas initialisé
- Les tableaux booléens `!()` ne sont pas stockés dans l'ordre logique

### 13.4 Conventions de l'éditeur

- `DEFLIST 0` (défaut) : mots-clés en majuscules, variables en minuscules
- L'apostrophe `'` en début de ligne = commentaire (contourne la vérification syntaxique)
- Le point d'exclamation `!` en fin de ligne = début de commentaire
- Une seule instruction par ligne (hors `:`)

---

## 14. Phases de réalisation

### Phase 1 — Fondations (Lexer, Parser, Runtime de base)

1. **Infrastructure du projet**
   - Mise en place du Makefile et de la structure de répertoires
   - Implémentation de l'abstraction OS (`os_layer.c`)
   - Gestion mémoire de base (`memory.c`)

2. **Lexer**
   - Tokenisation complète de tous les mots-clés (~280 tokens)
   - Gestion des nombres (décimal, hexa, binaire, octal)
   - Gestion des chaînes et commentaires
   - Tests unitaires

3. **Parser**
   - Grammaire récursive descendante
   - Construction de l'AST
   - Gestion des numéros de ligne et étiquettes
   - Tests unitaires

4. **Runtime de base**
   - Boucle d'exécution ligne par ligne
   - Variables scalaires (entiers, flottants, chaînes)
   - Opérations arithmétiques et logiques

### Phase 2 — Contrôle de flux et procédures

1. **Instructions de contrôle**
   - `IF/ELSE/ENDIF`
   - `FOR/NEXT`
   - `WHILE/WEND`
   - `REPEAT/UNTIL`
   - `SELECT/CASE/ENDSELECT`
   - `GOTO`, `GOSUB/RETURN`

2. **Procédures et fonctions**
   - `PROCEDURE ... RETURN`
   - Paramètres et variables locales
   - `DEFFN` / `FN`

3. **Gestion des erreurs**
   - `ON ERROR GOSUB`
   - Codes d'erreur (`ERR`, `ERR$`)
   - `ERROR`, `FATAL`

### Phase 3 — Types avancés et tableaux

1. **Système de types complet**
   - Directives `DEFxxx`
   - Suffixes de type (`$`, `%`, `&`, `!`, `#`, `|`, `{}`)

2. **Tableaux**
   - `DIM`, `ERASE`, `OPTION BASE`
   - Tableaux multidimensionnels
   - `ARRAYFILL`, `ARRPTR`

3. **Opérations matricielles**
   - Ensemble des instructions `MAT`

### Phase 4 — Entrées/sorties et fichiers

1. **I/O console**
   - `PRINT`, `PRINT AT`, `PRINT USING`
   - `INPUT`, `LINE INPUT`
   - `INKEY$`, `INP?`, `OUT?`
   - `CLS`, `LOCATE`

2. **Gestion des fichiers**
   - `OPEN`, `CLOSE`
   - `INPUT #`, `PRINT #`, `GET #`, `PUT #`
   - `BLOAD`, `BSAVE`
   - `FIELD`, `LSET`, `RSET`
   - Opérations sur fichiers (`KILL`, `NAME`, `MKDIR`, ...)
   - `FSFIRST`, `FSNEXT`

### Phase 5 — Fonctions intégrées

1. **Mathématiques**
   - Fonctions trigonométriques et hyperboliques
   - Logarithmes, exponentielles
   - Nombres aléatoires

2. **Chaînes**
   - Ensemble des fonctions de chaînes

3. **Mémoire et bits**
   - `PEEK`/`POKE` et variantes
   - `BMOVE`, `MALLOC`, `MFREE`
   - Opérations sur les bits (`BTST`, `BSET`, ...)

### Phase 6 — Graphisme (VDI)

1. **Backend graphique**
   - Driver SDL2
   - Émulation de l'écran Atari ST (320x200, 640x200, 640x400)
   - Palette de couleurs

2. **Primitives graphiques**
   - `LINE`, `CIRCLE`, `BOX`, `RBOX`, `PBOX`, `PCIRCLE`, `PELLIPSE`
   - `POLYLINE`, `POLYFILL`, `POLYMARK`
   - `FILL`, `BOUNDARY`
   - `GET`, `PUT`

3. **Attributs graphiques**
   - `COLOR`, `DEFFILL`, `DEFLINE`, `DEFTEXT`, `DEFMARK`
   - `SETCOLOR`

### Phase 7 — GEM AES et fenêtrage

1. **Initialisation AES**
   - `APPL_INIT`, `APPL_EXIT`

2. **Gestion des fenêtres**
   - `OPENW`, `CLOSEW`, `CLEARW`, `TITLEW`, `TOPW`
   - `WIND_OPEN`, `WIND_CLOSE`, `WIND_DELETE`, `WIND_FIND`

3. **Formulaires**
   - `FORM_ALERT`, `FORM_BUTTON`, `FORM_DIAL`, `FORM_DO`
   - `FORM_CENTER`, `FORM_KEYBD`, `FORM_ERROR`
   - `FORM INPUT`, `FORM INPUT AS`

4. **Menus**
   - `MENU_BAR`, `MENU_ICHECK`, `MENU_IENABLE`, `MENU_TEXT`
   - `MENU`, `MENU KILL`, `MENU OFF`

5. **Objets GEM**
   - `OBJC_ADD`, `OBJC_DELETE`, `OBJC_DRAW`, `OBJC_CHANGE`
   - Fonctions `OB_xxx` (accès aux propriétés)

6. **Ressources**
   - `RSRC_LOAD`, `RSRC_FREE`, `RSRC_GADDR`

7. **Sélecteurs**
   - `FILESELECT`, `FSEL_INPUT`
   - `ALERT`

### Phase 8 — TOS, Shell, Événements, Son

1. **TOS (GEMDOS, BIOS, XBIOS)**
   - Implémentation des fonctions essentielles
   - `GEMDOS(0x...)`, `BIOS(0x...)`, `XBIOS(0x...)`

2. **Gestion des événements**
   - `EVERY`, `AFTER`
   - `ON BREAK`, `ON MENU`
   - `EVNT_*` (AES)

3. **Son**
   - `SOUND`, `BEEP`
   - `WAVE`

4. **Shell et processus**
   - `SHEL_READ`, `SHEL_WRITE`, `SHEL_ENVRN`
   - `EXEC`

### Phase 9 — Mode interactif / Éditeur

1. **Mode interactif (REPL)**
   - Saisie de lignes numérotées
   - Commandes d'édition : `LIST`, `LLIST`, `NEW`, `LOAD`, `SAVE`
   - `RUN`, `DELETE`, `RENUM`, `MERGE`

2. **Débogueur**
   - `TRON`, `TROFF`, `TRACE$`
   - `MONITOR`
   - Évaluation de variables à l'arrêt

### Phase 10 — Optimisation, Tests et Compatibilité

1. **Tests de régression**
   - Suite de tests couvrant chaque instruction
   - Tests de compatibilité avec des programmes GFA Basic 3.5 existants

2. **Optimisation**
   - Mise en cache des variables
   - Mode bytecode (si non implémenté avant)

3. **Documentation**
   - Manuel utilisateur
   - Documentation développeur

---

## 15. Annexes

### 15.1 Structure des répertoires proposée

```
gfa-basic/
├── Makefile
├── README.md
├── doc/
│   └── reference.md
├── src/
│   ├── main.c
│   ├── lexer/
│   │   ├── lexer.c
│   │   ├── lexer.h
│   │   ├── token.h
│   │   └── keywords.h
│   ├── parser/
│   │   ├── parser.c
│   │   ├── parser.h
│   │   ├── ast.c
│   │   ├── ast.h
│   │   └── ast_types.h
│   ├── semantic/
│   │   ├── semantic.c
│   │   └── semantic.h
│   ├── runtime/
│   │   ├── runtime.c
│   │   ├── runtime.h
│   │   ├── exec_stmt.c
│   │   ├── exec_expr.c
│   │   └── callstack.c
│   ├── memory/
│   │   ├── memory.c
│   │   ├── memory.h
│   │   └── variables.c
│   ├── builtins/
│   │   ├── strings.c
│   │   ├── math.c
│   │   ├── bitops.c
│   │   └── conversion.c
│   ├── io/
│   │   ├── files.c
│   │   ├── console.c
│   │   └── printer.c
│   ├── graphics/
│   │   ├── vdi.c
│   │   ├── vdi.h
│   │   ├── primitives.c
│   │   └── display.c
│   ├── gem/
│   │   ├── aes.c
│   │   ├── aes_funcs.c
│   │   ├── window.c
│   │   ├── menu.c
│   │   ├── form.c
│   │   ├── object.c
│   │   └── resource.c
│   ├── tos/
│   │   ├── gemdos.c
│   │   ├── bios.c
│   │   ├── xbios.c
│   │   └── tos_emul.h
│   ├── events/
│   │   ├── events.c
│   │   └── timer.c
│   ├── sound/
│   │   └── sound.c
│   ├── shell/
│   │   ├── shell.c
│   │   └── exec.c
│   ├── editor/
│   │   ├── editor.c
│   │   └── interactive.c
│   └── utils/
│       ├── error.c
│       ├── os_layer.c
│       └── os_layer.h
├── tests/
│   ├── lexer/
│   │   └── test_lexer.c
│   ├── parser/
│   │   └── test_parser.c
│   ├── runtime/
│   │   └── test_exec.c
│   └── samples/
│       ├── hello.bas
│       ├── graphics.bas
│       ├── windows.bas
│       └── ...
└── thirdparty/
    └── SDL2/ (ou autre backend)
```

### 15.2 Conventions de codage C89

- Pas de commentaires `//` (utiliser `/* ... */`)
- Pas de déclarations de variables dans les boucles `for`
- Pas de types `bool` (utiliser `int`)
- Pas de `inline`
- Variables déclarées en début de bloc
- Utilisation de `#define` pour les constantes
- Fonctions déclarées avec prototypes complets
- `stdlib.h`, `stdio.h`, `string.h`, `math.h` comme bibliothèques standard

### 15.3 Références

- [GFA Basic 3.5 Reference — Gladir.com](https://www.gladir.com/CODER/GFABASIC/reference.htm)
- [GFABasic Compendium v3.00 — Lonny Pursell / ENCOM](https://gfabasic.net/stg/gfabasic.htm) — Manuel officiel GFA Basic 3.x complet
- [Atari ST TOS Documentation (Gladir)](https://www.gladir.com/CODER/GEMDOS/reference.htm)
- [GFABasic.net](http://gfabasic.net/) — Site de Lonny Pursell, référence de la communauté GFA Basic
- *GFA-Basic Programmer's Reference Guide Volume I* (livre de référence)
- *Atari ST Internals* — Abacus Software
- The Atari Compendium (TOS.HYP) — Documentation TOS, VDI, AES, BIOS, XBIOS, GEMDOS
- NVDI 5 documentation (VDI étendu)
- *68030 Assembly Language Reference* — Steve Williams (Addison-Wesley) — pour le format flottant
- GFA_PTCH.TXT — Patches interpréteur/compilateur (Christoph Conrad, Gregor Duchalski)

### 15.5 Options du compilateur ($)

L'instruction `$` remplace `OPTION` de la v2 pour passer des directives au compilateur :

| Option | Description |
|--------|-------------|
| `$M` | Réserve de l'espace mémoire supplémentaire |
| `$P` | Contrôle l'optimisation du compilateur |
| `$F%` | Sélection de bibliothèque (non documenté, problèmes connus) |
| `$X` | Contrôle des warnings (non documenté, problèmes connus) |

### 15.6 Tableau des résolutions Atari ST

| Mode | Résolution | Couleurs | Constante |
|------|-----------|----------|-----------|
| Basse (ST Low) | 320×200 | 16 (0-15) | `ST_MODE_LOW` |
| Moyenne (ST Medium) | 640×200 | 4 (0-3) | `ST_MODE_MEDIUM` |
| Haute (ST High) | 640×400 | Monochrome | `ST_MODE_HIGH` |

### 15.7 Fichiers de programme GFA Basic

| Extension | Format | Description |
|-----------|--------|-------------|
| `.BAS` / `.GFA` | Tokenisé | Programme sauvegardé en format binaire (SAVE) |
| `.ASC` | ASCII | Programme en texte (SAVE,A ; compatible MERGE) |
| `.PRG` | Exécutable | Programme compilé exécutable |
| `.RSC` | Ressource | Fichier ressource GEM (menus, boîtes de dialogue) |

### 15.4 Glossaire

| Terme | Définition |
|-------|-----------|
| **AES** | Application Environment Services — couche graphique haut niveau du GEM |
| **AST** | Abstract Syntax Tree — arbre syntaxique abstrait |
| **BIOS** | Basic Input/Output System — couche bas niveau du TOS |
| **DTA** | Disk Transfer Area — zone de transfert disque |
| **GEM** | Graphics Environment Manager — interface graphique de l'Atari ST |
| **GEMDOS** | GEM Disk Operating System — gestion fichiers/processus |
| **TOS** | The Operating System — système d'exploitation de l'Atari ST |
| **VDI** | Virtual Device Interface — couche graphique bas niveau du GEM |
| **XBIOS** | Extended BIOS — fonctions bas niveau spécifiques à l'Atari ST |
| **GFA** | Gesellschaft für Automatische Datenverarbeitung — créateur du BASIC |

---

> **Document version** : 3.0
> **Date** : 7 juin 2026
> **Auteur** : Généré par analyse croisée de :
> - [Gladir.com - Référence GFA Basic](https://www.gladir.com/CODER/GFABASIC/reference.htm)
> - [GFABasic Compendium v3.00](https://gfabasic.net/stg/gfabasic.htm) (Lonny Pursell / ENCOM)
> - [GFA-BASIC 3.5 Interpreter Reference Manual](https://info-coach.fr/atari/documents/general/gfabasic.pdf) (GFA Data Media UK, ST Format, 1991)
