# GFA Basic 3.5 — Émulateur pour Atari ST

Interpréteur compatible **GFA Basic 3.5 pour Atari ST**, écrit en **C ANSI (C89)**.

Analyse, compile et exécute des programmes `.bas` GFA Basic 3.5. Émule les appels TOS (GEMDOS, BIOS, XBIOS, AES, VDI) et fournit un environnement console avec primitives graphiques SDL2.

## Démarrage rapide

```bash
make          # compiler l'émulateur
./build/gfabasic demo_cplt.bas   # exécuter la démo
./build/gfabasic                     # mode interactif (REPL)
```

## Tests

```bash
make test-all   # 369+ tests, 100%
./build/gfabasic tests/test_if.bas   # 31 tests IF/THEN/ELSE
```

## Compilation Atari ST (Windows 11)

Le projet est en **C89 strict** avec une couche d'abstraction OS (`os_layer.c`)
qui cible aussi bien Windows que l'**Atari ST** (via gcc-mintelf).

### Prérequis

- **MSYS2** (installé automatiquement par `build_atari.bat` si absent)
- Toolchain **m68k-atari-mintelf** (gcc 68000 + binutils + mintlib + fdlibm)

### Build .PRG

```bat
build_atari.bat
```

Ce script installe MSYS2 + la toolchain (via `setup_msys2_toolchain.sh`),
puis compile :

```
build\atari\GFABASIC.PRG   (~288 Ko, BSS ~400 Ko → tient en 1 Mo)
```

Résultat : un **exécutable TOS Atari ST M68K** à copier sur disquette/CF
ou à lancer dans Hatari/Steem.

### Tester le .PRG (émulateur Hatari)

```bat
run_atari.bat
```

Ce script crée une **image disquette** `build\atari\GFABASIC.ST`
(via `make_floppy.py`, motif FAT12 720 Ko) puis lance Hatari :
```
hatari.exe --tos tos.img --disk-a build\atari\GFABASIC.ST --auto A:\GFABASIC.PRG --machine st --memsize 4
```
- `--disk-a GFABASIC.ST` : insère l'image disquette dans le lecteur A:
  (méthode universelle — n'exige pas le support GEMDOS HD, absent de certains compilages Hatari)
- `--auto A:\GFABASIC.PRG` : démarre automatiquement le programme depuis la disquette
- `--machine st` : Atari ST 68000
- `--memsize 4` : 4 Mo de RAM
- Mode interactif : tapez `QUIT` pour sortir
- La fenêtre Hatari ferme quand le programme se termine

### Test automatisé non interactif (Hatari)

```bat
run_atari_test.bat
```

Exécute automatiquement `tests\test_atari.bas` dans Hatari et affiche le
résultat (console Atari + erreurs Hatari).

Principe :
1. **Compile** `tools\runner.c` en `RUNNER.PRG` (`make -f Makefile.atari runner`) —
   un mini-launcher TOS qui utilise `Pexec()` pour lancer `GFABASIC.PRG`
   avec `A:\TEST.BAS` en argument (sans lui, les arguments ne peuvent pas être
   passés automatiquement à un PRG autostarté par Hatari).
2. **Construit** une disquette multi-fichiers `GFABASIC.ST` contenant
   GFABASIC.PRG, RUNNER.PRG et TEST.BAS (`make_floppy.py --multi`).
3. **Lance** Hatari avec :
   - `--auto A:\RUNNER.PRG` : démarre le launcher (qui lance GFA Basic)
   - `--conout 2` : redirige la console VT52 vers `simulation_console.txt`
   - `--benchmark` : mode le plus rapide (vitesse CPU maximale) — indispensable,
     car le runtime GFA en C compilé est trop lent en mode cycle-exact du 68000
   - `--run-vbls 4000` : arrêt automatique après 4000 VBL
4. **Affiche** la console Atari et les diagnostics (Bus Errors, panics).

Points d'attention corrigés durant les tests :
- **`argv[argc-1]`** : sous MiNT/Pexec, la cmdline GEMDOS inclut le nom du
  programme en premier token (`argv[1]`), le fichier `.bas` passe donc en
  dernier argument.
- **`RUNNER.PRG` ne doit PAS être strippé** : le strip des PRG contigus
  `elf32-atariprg` cassait le crt0 (Bus Error au démarrage).
- **Framebuffer graphique statique (BSS) 320×200 sous `GFA_TARGET_MINT`** :
  le `malloc(256 Ko)` de la mintlib bouclait avec le gros BSS du programme.
- **`load_file()` en lecture progressive** (sans `fseek`/`ftell`) pour être
  fiable sous GEMDOS.

**Installation manuelle des outils** (le script guide aussi) :

1. **Hatari** (émulateur Windows ≥ 2.6) : https://framagit.org/hatari/hatari/-/releases
   → décompressez `hatari.exe` dans `tools\hatari\`
2. **ROM TOS** : une ROM `tos.img` est fournie avec Hatari
   (ou **EmuTOS** libre : https://emutos.sourceforge.io/)
   → placez `tos.img` dans `tools\hatari\`

### Compilation manuelle (dans MSYS2)

```bash
pacman -S --needed --noconfirm make mingw-w64-x86_64-gcc
bash setup_msys2_toolchain.sh      # une seule fois
make -f Makefile.atari clean
make -f Makefile.atari
```

### Options de compilation croisée

| Option | Rôle |
|--------|------|
| `-m68000` | 68000 nu (ST/STE compatibles) |
| `-mnoshort` | `int` = 32 bits (le code suppose int 32 bits partout) |
| `-DGFA_TARGET_MINT` | Active la branche `osbind.h`/`_DTA`/`_base` de `os_layer.c` |
| `-Os` | Optimise la taille (RAM Atari limitée) |

> **Note** : l'ABI historique TOS (`-mshort`, `int` = 16 bits) casserait le code
> (buffers 4 096 lignes, tailles > 32767). Un audit complet serait nécessaire
> pour cibler l'ABI Pure C 1.1.

## Mode interactif (REPL)

Deux modes : **édition** (prompt `n]`) et **commande** (prompt `>`). Une ligne vide bascule.

| Commande | Description |
|----------|-------------|
| `LIST` | Liste le programme avec indentation automatique |
| `EDIT n` | Édite la ligne n (curseur ← → Home End Bksp Del) |
| `DELETE n` ou `n-m` | Supprime une ligne ou une plage |
| `INSERT n` | Insère une ligne avant la position n |
| `RUN ["fichier"]` | Exécute le programme (ou charge + exécute) |
| `LOAD "fichier"` | Charge un fichier `.bas` |
| `SAVE [A,] "fichier"` | Sauvegarde le programme |
| `NEW` | Efface le programme |
| `CLS` / `QUIT` | Efface l'écran / quitte |

Les mots-clés sont automatiquement mis en majuscules à la saisie.

## Fonctionnalités implémentées

| Catégorie | Détail |
|-----------|--------|
| **Contrôle de flux** | IF/THEN/ELSE/ENDIF, FOR/NEXT/STEP, WHILE/WEND, REPEAT/UNTIL, SELECT/CASE/ENDSELECT, GOTO/GOSUB/RETURN/@, DO/LOOP [WHILE\|UNTIL], EXIT IF, ON x GOTO/GOSUB, STOP, END, QUIT |
| **Procédures/Fonctions** | PROCEDURE, FUNCTION, RETURN expr, ENDFUNC, LOCAL, VAR (by-ref), DEFFN/FN, récursion, appels sans parenthèses (`maProc 3, 7`) |
| **Tableaux 1D** | DIM, OPTION BASE 0/1 (base des indices), ERASE/ARRAYFILL/DIM? non implémentés |
| **Opérateurs** | `+` `-` `*` `/` `^` `=` `<>` `<` `>` `<=` `>=` AND OR XOR NOT EQV IMP MOD DIV, & (concat) |
| **Mathématiques** | 35 fonctions : SIN/COS/TAN/ATN/ASIN/ACOS/SINQ/COSQ/SINH/COSH/TANH, EXP/LOG/LOG10/SQR, ABS/SGN/INT/FRAC/FIX/ROUND/CEIL/TRUNC, MIN/MAX/EVEN/ODD/PRED/SUCC, FACT/COMBIN/VARIAT, RND, DEG/RAD, CFLOAT/CINT |
| **Chaînes** | 28 fonctions : LEN, ASC, CHR$, VAL, VAL?, LEFT$/RIGHT$/MID$, INSTR, RINSTR, UPPER$/LCASE$/LOWER$, TRIM$, STR$, BIN$, HEX$, OCT$, SPACE$, STRING$, MKI$/MKL$/MKS$/MKF$/MKD$, CVI/CVL/CVS/CVF/CVD |
| **Opérateurs bits** | BTST, BSET, BCLR, BCHG, SHL, SHR, ROL, ROR |
| **Print** | PRINT, PRINT #, PRINT AT(x,y), PRINT USING (format #, ##.##, **, $$, +, -, ^^^^, virgules), INPUT, INPUT #, INKEY$, CLS, LOCATE |
| **Fichiers** | OPEN/CLOSE (I/O/R/A/U), OPENW/CLOSEW, PRINT#/INPUT# |
| **Mémoire** | PEEK/POKE/DPEEK/DPOKE/LPEEK/LPOKE (vmem), DATA/READ/RESTORE, MALLOC (partiel), BSAVE (stub) |
| **Graphismes SDL2** | COLOR (palette 16 couleurs), LINE, BOX, PBOX, CIRCLE, PCIRCLE |
| **Son** | BEEP, SOUND ch, freq, dur, vol, env |
| **Événements** | EVERY, AFTER, ON ERROR, ERROR, ERR |
| **Debug** | TRON, TROFF |
| **Édition** | LIST avec indentation, EDIT en place (← → Home End Bksp Del), INSERT, DELETE, LOAD/SAVE |

## Architecture

```
main.c ──┬── lexer/     (490 keywords, tokens, EOL)
         ├── parser/    (LL(1), AST, labels 2-pass)
         ├── codegen/   (AST → bytecode)
         └── runtime/   (VM pile, call stack, builtins)
              ├── memory/    (symboles, tableaux)
              ├── builtins/  (maths 35/35, chaînes 28/28, bits 8/8)
              ├── io/        (fichiers, BLOAD/BSAVE/BGET/BPUT)
              ├── events/    (EVERY, AFTER, ON ERROR)
              ├── sound/     (BEEP, SOUND)
              ├── tos/       (GEMDOS, BIOS, XBIOS, AES)
              └── graphics/  (SDL2, COLOR, LINE, BOX, CIRCLE)
 utils/os_layer   (abstraction fichiers, console, temps)
```

## Plan de développement

Voir [`PLAN.md`](PLAN.md) pour l'inventaire complet des fonctionnalités manquantes
et le plan de travail priorisé.

**Résumé :**

| Priorité | Nb items | Effort | Description |
|----------|----------|--------|-------------|
| **✅ Implémenté** | ~155 | — | Flux, procédures, maths, chaînes, bits, print AT/USING, conversion binaire, fichiers, graphismes, TOS partiel, vmem (PEEK/POKE réels) |
| **✅ Priorité A** | 22/22 | fait (2026-08-15) | Runtime cases : VAL?, INPUT$, PAUSE, MOUSE, TIMER, DATE$/TIME$, ==, RAND(n), PEEK/POKE via vmem… |
| **✅ Correctifs boucles/TOS** | 4 | fait (2026-08-15) | DO/LOOP [WHILE\|UNTIL] (était no-op silencieux), EXIT IF (parser+codegen, toutes boucles), OPTION BASE 0/1 (parse + base des tableaux), GEMDOS()/BIOS()/XBIOS() (opcodes câblés au codegen) |
| **🔜 Priorité B** | 35 | ~5-7 jours | Parser + codegen + runtime : graphismes avancés (PLOT, DRAW, TEXT, POLYLINE, FILL, BITBLT…), SETTIME, QSORT, INSERT/DELETE |
| **⏳ Priorité C** | 10 | ~4-8 semaines | Nouveaux sous-systèmes : GEM AES, VDI, MAT, FIELD, SHEL, CHAIN |

## Limitations

| Limitation | Détail |
|-----------|--------|
| Mots-clés comme noms | Certains mots (`add`, `val`, `double`, `inc`) sont réservés car le lexer n'est pas contextuel |
| Format flottant | IEEE-754 (double précision), pas le format GFA propriétaire 8 octets |
| GEM AES | APPL_INIT/EXIT ok. FORM_ALERT, MENU_BAR, WIND_*, EVNT_* sont des stubs (retournent 0). Voir PLAN.md |
| RESTORE label | Parse OK mais la restauration est globale (le label est ignoré au runtime) |
| `:` séparateur | Non supporté — une ligne = une instruction (conforme au vrai GFA Basic 3.5) |
| Lignes numérotées | Non supportées (`10 PRINT …`). Utiliser des étiquettes nommées (`deb:` / `GOTO deb`) : le lexer ne reconnaît pas un nombre en début de ligne comme numéro de ligne |
| `FOR … DOWNTO` | Non implémentée (erreur de parse) — utiliser une boucle WHILE avec décrément |
| `ERASE` / `CLEAR` / `ON BREAK` | Parsées mais sans effet (no-op) — les valeurs survivent |
| `LINE INPUT`, `HTAB`, `VTAB`, `UCASE$` | Non implémentés (erreur de parse ou runtime error 9) — `LINE INPUT` échoue car `LINE` est lexée comme le mot-clé graphique |
