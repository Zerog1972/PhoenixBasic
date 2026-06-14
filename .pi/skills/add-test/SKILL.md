---
name: add-test
description: Ajouter des tests unitaires C ou des tests BASIC à PhoenixBasic.
---

# Ajouter des tests

## Tests C (dans `tests/test_*.c`)

### Format (assert-based)

```c
static void test_mon_feature(void) {
    /* Setup */
    int result = ma_fonction(param);
    
    /* Verification */
    assert(result == valeur_attendue);
    
    printf("  test_mon_feature: OK\n");
}
```

### Ajouter au runner dans `main()`

```c
int main(void) {
    /* ... */
    test_mon_feature();
    /* ... */
    printf("=== All tests done (%d passed, %d failed) ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
```

### Compiler et exécuter

```bash
make test-rt    # pour runtime
make test-lexer # pour lexer
make test-parser # pour parser
```

## Tests BASIC (dans `tests/test_*.bas`)

### Format

```basic
' Description du test
' ======================

passed = 0
failed = 0

PROCEDURE check
  IF check_cond
    passed = passed + 1
    PRINT "Test X: OK"
  ELSE
    failed = failed + 1
    PRINT "Test X: FAIL"
  ENDIF
RETURN

' --- Test 1: description ---
check_cond = (expression)
check_num = 1
check

PRINT passed; " passed, "; failed; " failed"
END
```

### Exécuter

```bash
./build/gfabasic tests/test_mon_test.bas
```

## Bonnes pratiques

- Toujours tester le cas nominal ET les cas limites
- Tester les erreurs attendues (division par zéro, overflow, etc.)
- Un test = une assertion
- Utiliser des noms de test descriptifs
- Ajouter le test AVANT de corriger le bug (TDD)
