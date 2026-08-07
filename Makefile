# Makefile - GFA Basic 3.5 Emulator (C89)
# ========================================
# Le code est C89 strict et portable :
#   - hote de developpement : gcc --ansi -pedantic-errors (Windows/Unix)
#   - Atari ST : compilateur Pure C 1.1 (voir cible "atari" et GFABASIC.PRJ)
#
CC       = gcc
CFLAGS   = -ansi -pedantic-errors -Wall -Wextra -O2

INCLUDES = -Isrc/utils -Isrc/builtins -Isrc/io -Isrc/events -Isrc/sound \
           -Isrc/tos -Isrc/runtime -Isrc/memory -Isrc/lexer -Isrc/parser \
           -Isrc/codegen -Isrc/graphics

# Pas de dependance externe : uniquement libc C89
GFX_LIBS  =

BUILDDIR  = build
TESTDIR   = tests

# --- Commandes portables Windows (cmd) / Unix (sh/bash) ---
# Utilise $(SHELL) plutot que $(OS) : sous MSYS2/cygwin, OS=Windows_NT
# mais SHELL=/bin/sh. Il faut donc detecter le shell Unix si present.
ifneq ($(findstring sh,$(SHELL)),)
  MKDIR  = mkdir -p
  TOUCH  = touch
  RMDIR  = rm -rf
  RM     = rm -f
else
  MKDIR  = mkdir
  TOUCH  = type nul >
  RMDIR  = rmdir /S /Q
  RM     = del /Q
endif

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

.PHONY: all app runtime test-os test-rt test-lexer test-parser test-tos-gfx test-bas test-all clean distclean

all: app
	@echo "=========================================="
	@echo " GFA Basic 3.5 - $(APP)"
	@echo " Usage: $(APP) [fichier.bas]"
	@echo "=========================================="

app: $(OBJS) src/main.c
	-@$(MKDIR) $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(APP) src/main.c $(OBJS) -lm
	@echo "Application built: $(APP)"

runtime: $(OBJS)
	@echo "All modules compiled ($(words $(OBJS)) objects)."

# Regle generique
$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)/dirs
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILDDIR)/dirs:
	-@$(MKDIR) $(BUILDDIR)/utils $(BUILDDIR)/builtins $(BUILDDIR)/io
	-@$(MKDIR) $(BUILDDIR)/events $(BUILDDIR)/sound $(BUILDDIR)/tos
	-@$(MKDIR) $(BUILDDIR)/runtime $(BUILDDIR)/memory
	-@$(MKDIR) $(BUILDDIR)/lexer $(BUILDDIR)/parser $(BUILDDIR)/codegen
	-@$(MKDIR) $(BUILDDIR)/graphics
	-@$(TOUCH) $(BUILDDIR)/dirs

# Tests
test-os: $(BUILDDIR)/utils/os_layer.o $(TESTDIR)/test_os.c
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
	$(CC) $(CFLAGS) $(INCLUDES) -o $(BUILDDIR)/test_gfx $(TESTDIR)/test_gfx.c $(OBJS) -lm
	@$(BUILDDIR)/test_gfx

test-all: test-os test-rt test-lexer test-parser test-tos-gfx test-bas
	@echo "=== All tests done ==="

# Boucle de test portable : syntaxe differente entre sh et cmd.exe
ifneq ($(findstring sh,$(SHELL)),)
  TESTBAS_LOOP = for f in $(TESTDIR)/test_*.bas; do echo "--- $$(basename $$f) ---"; $(APP) "$$f"; done
else
  TESTBAS_LOOP = for %f in ($(TESTDIR)/test_*.bas) do @echo --- %~nxf --- & $(APP) "%f"
endif

test-bas: $(APP)
	@echo "=== Running BASIC tests ==="
	@$(TESTBAS_LOOP)
	@echo "=== All BASIC tests done ==="

clean:
	-@$(RMDIR) $(BUILDDIR)
	-@$(RM) __test_*.tmp

distclean: clean