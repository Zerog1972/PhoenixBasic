# Makefile - GFA Basic 3.5 Emulator (C89)
# ========================================
CC       = gcc
CFLAGS   = -ansi -pedantic-errors -Wall -Wextra -O2

INCLUDES = -Isrc/utils -Isrc/builtins -Isrc/io -Isrc/events -Isrc/sound \
           -Isrc/tos -Isrc/runtime -Isrc/memory -Isrc/lexer -Isrc/parser \
           -Isrc/codegen -Isrc/graphics

# Pas de dependance externe : uniquement libc C89
GFX_LIBS  =

BUILDDIR  = build
TESTDIR   = tests

# Tous les .c et leurs objets
SRCS = src/utils/os_layer.c src/builtins/strings.c src/builtins/gfamath.c \
       src/builtins/bit_ops.c \
       src/io/files.c src/events/events.c src/sound/sound.c src/tos/tos.c \
       src/runtime/runtime.c src/memory/memory.c \
       src/lexer/keywords.c src/lexer/lexer.c \
       src/parser/ast.c src/parser/parser.c \
       src/codegen/codegen.c \
       src/graphics/gfx.c

OBJS = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SRCS))

# Application principale
APP = $(BUILDDIR)/gfabasic

.PHONY: all app runtime test-os test-rt test-lexer test-parser test-all clean

all: app
	@echo "=========================================="
	@echo " GFA Basic 3.5 - $(APP)"
	@echo " Usage: $(APP) [fichier.bas]"
	@echo "=========================================="

app: $(OBJS) src/main.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(APP) src/main.c $(OBJS) -lm
	@echo "Application built: $(APP)"

runtime: $(OBJS)
	@echo "All modules compiled ($(words $(OBJS)) objects)."

# Regle generique
$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)/dirs
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILDDIR)/dirs:
	@mkdir -p $(BUILDDIR)/utils $(BUILDDIR)/builtins $(BUILDDIR)/io
	@mkdir -p $(BUILDDIR)/events $(BUILDDIR)/sound $(BUILDDIR)/tos
	@mkdir -p $(BUILDDIR)/runtime $(BUILDDIR)/memory
	@mkdir -p $(BUILDDIR)/lexer $(BUILDDIR)/parser $(BUILDDIR)/codegen
	@mkdir -p $(BUILDDIR)/graphics
	@touch $(BUILDDIR)/dirs

# Tests
test-os: $(OBJS) $(TESTDIR)/test_os.c
	$(CC) $(CFLAGS) -Isrc/utils -o $(BUILDDIR)/test_os $(TESTDIR)/test_os.c $(BUILDDIR)/utils/os_layer.o -lm
	@$(BUILDDIR)/test_os

test-rt: $(OBJS) $(TESTDIR)/test_rt.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $(BUILDDIR)/test_rt $(TESTDIR)/test_rt.c $(OBJS) -lm
	@$(BUILDDIR)/test_rt

test-lexer: $(OBJS) $(TESTDIR)/test_lex.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $(BUILDDIR)/test_lex $(TESTDIR)/test_lex.c $(OBJS) -lm
	@$(BUILDDIR)/test_lex

test-parser: $(OBJS) $(TESTDIR)/test_par.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $(BUILDDIR)/test_par $(TESTDIR)/test_par.c $(OBJS) -lm
	@$(BUILDDIR)/test_par

test-tos-gfx: $(OBJS) $(TESTDIR)/test_gfx.c
	$(CC) $(CFLAGS) -Isrc/tos $(INCLUDES) -o $(BUILDDIR)/test_gfx $(TESTDIR)/test_gfx.c $(OBJS) -lm
	@$(BUILDDIR)/test_gfx

test-all: test-os test-rt test-lexer test-parser test-tos-gfx test-bas
	@echo "=== All tests done ==="

test-bas: $(APP)
	@echo "=== Running BASIC tests ==="
	for f in $(TESTDIR)/test_*.bas; do \
		echo "--- $$(basename $$f) ---"; \
		$(APP) $$f || exit 1; \
	done
	@echo "=== All BASIC tests done ==="

clean:
	rm -rf $(BUILDDIR)
	rm -f __test_*.tmp

distclean: clean
