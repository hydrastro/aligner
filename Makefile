# aligner — a C source formatter with strong opinions about column 80.

CC      ?= cc
AR      ?= ar
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -O2 -g
LDLIBS  ?= -lpthread

DSDIR    := ds
SRCDIR   := src
TESTDIR  := tests
BUILDDIR := build

# ds is the data-structures library this project depends on. We compile the
# subset of files we actually use; the unicode_tables generator is skipped
# (it needs network); we use the pre-shipped unicode_tables.{c,h}.
DS_LIB_SRCS := \
  $(DSDIR)/lib/allocators.c \
  $(DSDIR)/lib/common.c \
  $(DSDIR)/lib/context.c \
  $(DSDIR)/lib/diagnostic.c \
  $(DSDIR)/lib/dlist.c \
  $(DSDIR)/lib/error.c \
  $(DSDIR)/lib/hash_table.c \
  $(DSDIR)/lib/iter.c \
  $(DSDIR)/lib/list.c \
  $(DSDIR)/lib/queue.c \
  $(DSDIR)/lib/stack.c \
  $(DSDIR)/lib/status.c \
  $(DSDIR)/lib/str.c \
  $(DSDIR)/lib/str_algo.c \
  $(DSDIR)/lib/str_io.c \
  $(DSDIR)/lib/str_io_posix.c \
  $(DSDIR)/lib/string_map.c

DS_OBJS := $(patsubst $(DSDIR)/%.c,$(BUILDDIR)/ds/%.o,$(DS_LIB_SRCS))
DS_LIB  := $(BUILDDIR)/libds.a
DS_CFLAGS := -std=c89 -O2 -fPIC -I$(DSDIR) -I$(DSDIR)/lib -I$(DSDIR)/build

ALN_SRCS := $(wildcard $(SRCDIR)/*.c)
ALN_OBJS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(ALN_SRCS))
ALN_CFLAGS := $(CFLAGS) -I$(DSDIR) -I$(SRCDIR)
ALN_BIN  := $(BUILDDIR)/aligner

.PHONY: all clean distclean test test-lexer aligner-c repo-cleanup self-host from-aligner-c

all: $(ALN_BIN)

$(ALN_BIN): $(ALN_OBJS) $(DS_LIB)
	$(CC) -o $@ $(ALN_OBJS) $(DS_LIB) $(LDLIBS)

# Single-file amalgam: build/aligner.c is a self-contained .c that compiles
# with libc only. Useful for dropping into another project, code review, or
# squinting at the whole thing in one buffer.
SINGLE_SRC := $(BUILDDIR)/aligner.c
SINGLE_BIN := $(BUILDDIR)/aligner-amalgam

aligner-c: $(SINGLE_BIN)

$(SINGLE_SRC): tools/build_single.sh tools/shim.c $(ALN_SRCS) $(wildcard $(SRCDIR)/*.h)
	@bash tools/build_single.sh $@

$(SINGLE_BIN): $(SINGLE_SRC)
	$(CC) -std=c99 -O2 -Wall -Wextra -Wpedantic -o $@ $<
	@echo "single-file binary: $@ ($$(stat -c%s $@ 2>/dev/null || stat -f%z $@) bytes)"

# Self-host: regenerate the canonical aligned aligner.c at the repo root by
# running the built aligner over its own amalgam. After this:
#   - aligner.c at repo root IS the aligner, aligned, libc-only-buildable
#   - cc -std=c99 -O2 -o aligner aligner.c   ← that's the entire build
# This target is idempotent: running it again produces the byte-identical
# file (because the aligner is idempotent on its own output).
ALIGNED_SRC := aligner.c
ALIGNED_BIN := $(BUILDDIR)/aligner-self

self-host: $(ALIGNED_SRC)

$(ALIGNED_SRC): $(ALN_BIN) $(SINGLE_SRC)
	./$(ALN_BIN) $(SINGLE_SRC) > $@
	@echo "regenerated $@ ($$(wc -l < $@) lines, $$(wc -c < $@) bytes)"

# Build the binary FROM the canonical aligned source. No Makefile machinery
# strictly required; this target is a convenience that does the obvious
# `cc -std=c99 -O2 -o aligner aligner.c` and stashes the result in build/.
from-aligner-c: $(ALIGNED_BIN)

$(ALIGNED_BIN): $(ALIGNED_SRC)
	$(CC) -std=c99 -O2 -Wall -Wextra -Wpedantic -o $@ $<
	@echo "self-hosted binary: $@ (built straight from $(ALIGNED_SRC))"

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(ALN_CFLAGS) -c $< -o $@

$(DS_LIB): $(DS_OBJS)
	$(AR) rcs $@ $^

$(BUILDDIR)/ds/%.o: $(DSDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(DS_CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)

# distclean removes everything `clean` does, plus other transient debris that
# tends to collect in a working tree: editor backups, swap files, core dumps,
# test helper binaries, nix result symlinks, formatter run leftovers.
# After distclean the tree should look exactly like a fresh git checkout.
distclean: clean
	rm -rf result result-*                                  # nix build output
	rm -f $(TESTDIR)/runner.log                             # test logs
	rm -rf $(TESTDIR)/tmp                                   # test scratch
	find . -name '*.o'        -not -path './$(DSDIR)/*' -delete
	find . -name '*.a'        -delete
	find . -name '*.so'       -delete
	find . -name '*.so.*'     -delete
	find . -name 'a.out'      -delete
	find . -name '*.swp'      -delete
	find . -name '*.swo'      -delete
	find . -name '*~'         -delete
	find . -name '.DS_Store'  -delete
	find . -name 'core'       -delete
	find . -name 'core.[0-9]*' -delete

test: all
	@bash $(TESTDIR)/runner.sh

# Build and run just the lexer-stage standalone test.
test-lexer: all
	@bash $(TESTDIR)/runner.sh lexer

# Heavy cleanup: distclean PLUS removes old experiments left lying around
# from earlier sessions (the cfmt-project/ subtree, stray .c at root, old
# binaries). Idempotent and safe to run repeatedly.
repo-cleanup:
	@bash tools/repo-cleanup.sh
