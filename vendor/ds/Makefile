CC ?= cc
CXX ?= c++
AR ?= ar
VALGRIND ?= valgrind

CFLAGS = -std=c89 -Wall -Wextra -Werror -pedantic -pedantic-errors \
         -Waggregate-return -Wbad-function-cast -Wcast-align -Wcast-qual \
         -Wdeclaration-after-statement -Wfloat-equal -Wlogical-op \
         -Wmissing-declarations -Wmissing-include-dirs -Wmissing-prototypes \
         -Wnested-externs -Wpointer-arith -Wredundant-decls -Wsequence-point \
         -Wstrict-prototypes -Wswitch -Wundef -Wunreachable-code \
         -Wwrite-strings -Wconversion \
         -O2 -fPIC
TEST_CFLAGS = -std=c99 -Wall -Wextra -Werror -pedantic -O2
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -O2
LDFLAGS = -shared
LDLIBS = -pthread
ARFLAGS = rcs
VALGRIND_FLAGS ?= --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=definite,possible --error-exitcode=99 --track-origins=yes

PREFIX ?= /usr/local
INCLUDEDIR ?= $(PREFIX)/include
LIBDIR ?= $(PREFIX)/lib
INSTALL ?= install

LIB_DIR   := lib
GEN_DIR   := gen
TEST_DIR  := test
BUILD_DIR := build
DATA_DIR  := data
TEST_BUILD_DIR := $(BUILD_DIR)/test
INCLUDES  := -I. -I$(LIB_DIR) -I$(BUILD_DIR)

LIB_SRC := \
  $(LIB_DIR)/allocators.c \
  $(LIB_DIR)/avl.c \
  $(LIB_DIR)/bst.c \
  $(LIB_DIR)/btree.c \
  $(LIB_DIR)/common.c \
  $(LIB_DIR)/deque.c \
  $(LIB_DIR)/dlist.c \
  $(LIB_DIR)/hash_table.c \
  $(LIB_DIR)/heap.c \
  $(LIB_DIR)/history.c \
  $(LIB_DIR)/list.c \
  $(LIB_DIR)/context.c \
  $(LIB_DIR)/diagnostic.c \
  $(LIB_DIR)/error.c \
  $(LIB_DIR)/graph.c \
  $(LIB_DIR)/iter.c \
  $(LIB_DIR)/persistent_rbt.c \
  $(LIB_DIR)/persistent_trie.c \
  $(LIB_DIR)/queue.c \
  $(LIB_DIR)/rbt.c \
  $(LIB_DIR)/stack.c \
  $(LIB_DIR)/status.c \
  $(LIB_DIR)/string_map.c \
  $(LIB_DIR)/str.c \
  $(LIB_DIR)/str_algo.c \
  $(LIB_DIR)/str_io.c \
  $(LIB_DIR)/str_io_posix.c \
  $(LIB_DIR)/str_unicode.c \
  $(LIB_DIR)/trie.c \
  $(LIB_DIR)/unicode_runtime.c

GEN_SRC_C := $(BUILD_DIR)/unicode_tables.c
GEN_SRC_H := $(BUILD_DIR)/unicode_tables.h

TEST_UCD_DIR := $(TEST_DIR)/ucd
TEST_UCD_FILES := \
  $(TEST_UCD_DIR)/UnicodeData.txt \
  $(TEST_UCD_DIR)/CaseFolding.txt \
  $(TEST_UCD_DIR)/CompositionExclusions.txt \
  $(TEST_UCD_DIR)/GraphemeBreakProperty.txt \
  $(TEST_UCD_DIR)/emoji-data.txt
TEST_GEN_SRC_C := $(TEST_BUILD_DIR)/unicode_tables.c
TEST_GEN_SRC_H := $(TEST_BUILD_DIR)/unicode_tables.h

LIB_OBJ      := $(LIB_SRC:.c=.o)
LIB_OBJ_SAFE := $(patsubst %.c,%_safe.o,$(LIB_SRC))
GEN_OBJ      := $(GEN_SRC_C:.c=.o)
TEST_GEN_OBJ := $(TEST_GEN_SRC_C:.c=.o)

LIB_STATIC      := libds.a
LIB_SHARED      := libds.so
LIB_STATIC_SAFE := libds_safe.a
LIB_SHARED_SAFE := libds_safe.so
TEST_LIB_STATIC := $(TEST_BUILD_DIR)/libds_test.a

TEST_SRC := $(wildcard $(TEST_DIR)/*_test.c)
TEST_BIN := $(patsubst $(TEST_DIR)/%.c,$(TEST_BUILD_DIR)/%,$(TEST_SRC))
EXAMPLE_DIR := examples
EXAMPLE_BUILD_DIR := $(BUILD_DIR)/examples
EXAMPLE_SRC := $(wildcard $(EXAMPLE_DIR)/*.c)
EXAMPLE_BIN := $(patsubst $(EXAMPLE_DIR)/%.c,$(EXAMPLE_BUILD_DIR)/%,$(EXAMPLE_SRC))

GEN_BIN := $(BUILD_DIR)/gen_ucd
GEN_SRC := $(GEN_DIR)/gen_ucd.c

UCD_FILES := \
  $(DATA_DIR)/UnicodeData.txt \
  $(DATA_DIR)/CaseFolding.txt \
  $(DATA_DIR)/CompositionExclusions.txt \
  $(DATA_DIR)/GraphemeBreakProperty.txt \
  $(DATA_DIR)/emoji-data.txt

UCD_BASE := https://www.unicode.org/Public/UCD/latest/ucd
UCD_AUX  := $(UCD_BASE)/auxiliary
EMOJI_URL := https://www.unicode.org/Public/UCD/latest/ucd/emoji

.PHONY: all clean distclean unicode-data install uninstall test test-build check check-cpp sanitize test-safe examples valgrind valgrind-test valgrind-safe valgrind-examples
all: $(LIB_STATIC) $(LIB_SHARED) $(LIB_STATIC_SAFE) $(LIB_SHARED_SAFE)

install: all
	$(INSTALL) -d $(DESTDIR)$(INCLUDEDIR)/lib $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0644 ds.h ds_safe.h $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL) -m 0644 $(LIB_DIR)/*.h $(DESTDIR)$(INCLUDEDIR)/lib
	$(INSTALL) -m 0644 $(LIB_STATIC) $(LIB_STATIC_SAFE) $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0755 $(LIB_SHARED) $(LIB_SHARED_SAFE) $(DESTDIR)$(LIBDIR)

uninstall:
	rm -f $(DESTDIR)$(INCLUDEDIR)/ds.h $(DESTDIR)$(INCLUDEDIR)/ds_safe.h
	rm -rf $(DESTDIR)$(INCLUDEDIR)/lib
	rm -f $(DESTDIR)$(LIBDIR)/$(LIB_STATIC) $(DESTDIR)$(LIBDIR)/$(LIB_STATIC_SAFE)
	rm -f $(DESTDIR)$(LIBDIR)/$(LIB_SHARED) $(DESTDIR)$(LIBDIR)/$(LIB_SHARED_SAFE)


$(BUILD_DIR) $(DATA_DIR) $(TEST_BUILD_DIR) $(EXAMPLE_BUILD_DIR):
	@mkdir -p $@

$(DATA_DIR)/UnicodeData.txt: | $(DATA_DIR)
	@[ -f $@ ] || curl -fsSL -o $@ $(UCD_BASE)/UnicodeData.txt
$(DATA_DIR)/CaseFolding.txt: | $(DATA_DIR)
	@[ -f $@ ] || curl -fsSL -o $@ $(UCD_BASE)/CaseFolding.txt
$(DATA_DIR)/CompositionExclusions.txt: | $(DATA_DIR)
	@[ -f $@ ] || curl -fsSL -o $@ $(UCD_BASE)/CompositionExclusions.txt
$(DATA_DIR)/GraphemeBreakProperty.txt: | $(DATA_DIR)
	@[ -f $@ ] || curl -fsSL -o $@ $(UCD_AUX)/GraphemeBreakProperty.txt
$(DATA_DIR)/emoji-data.txt: | $(DATA_DIR)
	@[ -f $@ ] || curl -fsSL -o $@ $(EMOJI_URL)/emoji-data.txt

unicode-data: $(UCD_FILES)

$(GEN_BIN): $(GEN_SRC) | $(BUILD_DIR)
	$(CC) -std=c89 -Wall -Wextra -O2 $(INCLUDES) -o $@ $<

$(GEN_SRC_C) $(GEN_SRC_H): $(GEN_BIN) $(UCD_FILES) | $(BUILD_DIR)
	$(GEN_BIN) \
	  --unicode-data $(DATA_DIR)/UnicodeData.txt \
	  --case-folding $(DATA_DIR)/CaseFolding.txt \
	  --comp-excl    $(DATA_DIR)/CompositionExclusions.txt \
	  --gb-prop      $(DATA_DIR)/GraphemeBreakProperty.txt \
	  --emoji-data   $(DATA_DIR)/emoji-data.txt \
	  --out-c        $(GEN_SRC_C) \
	  --out-h        $(GEN_SRC_H)

$(TEST_GEN_SRC_C) $(TEST_GEN_SRC_H): $(GEN_BIN) $(TEST_UCD_FILES) | $(TEST_BUILD_DIR)
	$(GEN_BIN) \
	  --unicode-data $(TEST_UCD_DIR)/UnicodeData.txt \
	  --case-folding $(TEST_UCD_DIR)/CaseFolding.txt \
	  --comp-excl    $(TEST_UCD_DIR)/CompositionExclusions.txt \
	  --gb-prop      $(TEST_UCD_DIR)/GraphemeBreakProperty.txt \
	  --emoji-data   $(TEST_UCD_DIR)/emoji-data.txt \
	  --out-c        $(TEST_GEN_SRC_C) \
	  --out-h        $(TEST_GEN_SRC_H)

%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

%_safe.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -D DS_THREAD_SAFE $(INCLUDES) -c -o $@ $<

$(GEN_OBJ): $(GEN_SRC_C) $(GEN_SRC_H)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $(GEN_SRC_C)

$(TEST_GEN_OBJ): $(TEST_GEN_SRC_C) $(TEST_GEN_SRC_H)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $(TEST_GEN_SRC_C)

$(LIB_STATIC): $(LIB_OBJ) $(GEN_OBJ)
	$(AR) $(ARFLAGS) $@ $^

$(LIB_SHARED): $(LIB_OBJ) $(GEN_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(LIB_STATIC_SAFE): $(LIB_OBJ_SAFE) $(GEN_OBJ)
	$(AR) $(ARFLAGS) $@ $^

$(LIB_SHARED_SAFE): $(LIB_OBJ_SAFE) $(GEN_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_LIB_STATIC): $(LIB_OBJ) $(TEST_GEN_OBJ) | $(TEST_BUILD_DIR)
	$(AR) $(ARFLAGS) $@ $^

$(TEST_BUILD_DIR)/%: $(TEST_DIR)/%.c $(TEST_LIB_STATIC) | $(TEST_BUILD_DIR)
	$(CC) $(TEST_CFLAGS) $(INCLUDES) -o $@ $< $(TEST_LIB_STATIC) $(LDLIBS)

test-build: $(TEST_BIN)

test: $(TEST_BIN)
	@set -e; \
	for test_bin in $(TEST_BIN); do \
	  printf '\n== %s ==\n' "$$test_bin"; \
	  "$$test_bin"; \
	done

check: test

$(TEST_BUILD_DIR)/cpp_smoke: $(TEST_DIR)/cpp_smoke.cpp $(LIB_STATIC) | $(TEST_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $< $(LIB_STATIC) $(LDLIBS)

check-cpp: $(TEST_BUILD_DIR)/cpp_smoke
	$(TEST_BUILD_DIR)/cpp_smoke

$(EXAMPLE_BUILD_DIR)/%: $(EXAMPLE_DIR)/%.c $(TEST_LIB_STATIC) | $(EXAMPLE_BUILD_DIR)
	$(CC) $(TEST_CFLAGS) $(INCLUDES) -o $@ $< $(TEST_LIB_STATIC) $(LDLIBS)

examples: $(EXAMPLE_BIN)

clean:
	rm -f $(LIB_DIR)/*.o $(LIB_DIR)/*_safe.o $(BUILD_DIR)/*.o \
	      $(LIB_STATIC) $(LIB_SHARED) $(LIB_STATIC_SAFE) $(LIB_SHARED_SAFE) \
	      $(GEN_BIN)
	rm -f $(GEN_SRC_C) $(GEN_SRC_H)
	rm -rf $(TEST_BUILD_DIR)

distclean: clean
	rm -rf $(DATA_DIR)


SAFE_TEST_BUILD_DIR := $(TEST_BUILD_DIR)/safe
TEST_LIB_STATIC_SAFE_TEST := $(SAFE_TEST_BUILD_DIR)/libds_test_safe.a
SAFE_TEST_SRC := \
  $(TEST_DIR)/thread_safe_smoke_test.c \
  $(TEST_DIR)/thread_safe_all_structures_test.c
TEST_BIN_SAFE := $(patsubst $(TEST_DIR)/%.c,$(SAFE_TEST_BUILD_DIR)/%,$(SAFE_TEST_SRC))

$(SAFE_TEST_BUILD_DIR):
	@mkdir -p $@

$(TEST_LIB_STATIC_SAFE_TEST): $(LIB_OBJ_SAFE) $(TEST_GEN_OBJ) | $(SAFE_TEST_BUILD_DIR)
	$(AR) $(ARFLAGS) $@ $^

$(SAFE_TEST_BUILD_DIR)/%: $(TEST_DIR)/%.c $(TEST_LIB_STATIC_SAFE_TEST) | $(SAFE_TEST_BUILD_DIR)
	$(CC) $(TEST_CFLAGS) -D DS_THREAD_SAFE $(INCLUDES) -o $@ $< $(TEST_LIB_STATIC_SAFE_TEST) $(LDLIBS)

test-safe: $(TEST_BIN_SAFE)
	@set -e; \
	for test_bin in $(TEST_BIN_SAFE); do \
	  printf '\n== %s ==\n' "$$test_bin"; \
	  "$$test_bin"; \
	done

sanitize:
	$(MAKE) clean
	@status=0; \
	$(MAKE) CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -pedantic-errors -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined' TEST_CFLAGS='-std=c99 -Wall -Wextra -Werror -pedantic -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined' LDLIBS='-pthread -fsanitize=address,undefined' test || status=$$?; \
	$(MAKE) clean; \
	exit $$status


valgrind-test: test-build
	@command -v $(VALGRIND) >/dev/null 2>&1 || { \
	  echo "valgrind not found; install Valgrind or set VALGRIND=/path/to/valgrind"; \
	  exit 127; \
	}
	@set -e; \
	for test_bin in $(TEST_BIN); do \
	  printf '\n== valgrind %s ==\n' "$$test_bin"; \
	  $(VALGRIND) $(VALGRIND_FLAGS) "$$test_bin"; \
	done

valgrind-safe: test-safe
	@command -v $(VALGRIND) >/dev/null 2>&1 || { \
	  echo "valgrind not found; install Valgrind or set VALGRIND=/path/to/valgrind"; \
	  exit 127; \
	}
	@set -e; \
	for test_bin in $(TEST_BIN_SAFE); do \
	  printf '\n== valgrind %s ==\n' "$$test_bin"; \
	  $(VALGRIND) $(VALGRIND_FLAGS) "$$test_bin"; \
	done

valgrind-examples: examples
	@command -v $(VALGRIND) >/dev/null 2>&1 || { \
	  echo "valgrind not found; install Valgrind or set VALGRIND=/path/to/valgrind"; \
	  exit 127; \
	}
	@set -e; \
	for example_bin in $(EXAMPLE_BIN); do \
	  printf '\n== valgrind %s ==\n' "$$example_bin"; \
	  $(VALGRIND) $(VALGRIND_FLAGS) "$$example_bin"; \
	done

valgrind: valgrind-test valgrind-safe valgrind-examples
