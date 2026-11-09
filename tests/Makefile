# tests/Makefile - builds the core, the unit tests and the Linux host,
# then runs every test.
#
#   make -C tests          normal build + sanitizer build, all tests
#   make -C tests normal   normal build only
#   make -C tests san      -fsanitize=address,undefined build only
#
# Build output goes to build/ at the repository root.

ROOT   := ..
WARN   := -std=c99 -Wall -Wextra -Wpedantic -Werror
INC    := -I$(ROOT)/include -I$(ROOT)/src

ifeq ($(SAN),1)
BUILD  := $(ROOT)/build/san
OPT    := -O1 -g -fno-omit-frame-pointer \
          -fsanitize=address,undefined -fno-sanitize-recover=all
else
BUILD  := $(ROOT)/build/normal
OPT    := -O2 -g
endif

CFLAGS := $(WARN) $(OPT) $(INC)

CORE_SRC := $(wildcard $(ROOT)/src/*.c)
CORE_HDR := $(wildcard $(ROOT)/src/*.h) $(wildcard $(ROOT)/include/*.h)
UNIT     := test_token test_compile test_vm
UNIT_BIN := $(addprefix $(BUILD)/,$(UNIT))
TL       := $(BUILD)/tl

.PHONY: all normal san test
all: normal san

normal:
	@$(MAKE) --no-print-directory test SAN=0

san:
	@$(MAKE) --no-print-directory test SAN=1

test: $(UNIT_BIN) $(TL)
	@echo "== unit tests ($(notdir $(BUILD)))"
	@set -e; for t in $(UNIT_BIN); do $$t; done
	@echo "== golden tests ($(notdir $(BUILD)))"
	@sh ./run_golden.sh $(TL) programs
	@echo "== REPL tests ($(notdir $(BUILD)))"
	@sh ./run_repl.sh $(TL) repl

$(BUILD):
	@mkdir -p $@

$(BUILD)/test_%: unit/test_%.c unit/port_capture.c unit/port_capture.h \
                 unit/check.h $(CORE_SRC) $(CORE_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -Iunit -o $@ $< unit/port_capture.c $(CORE_SRC)

$(TL): $(ROOT)/ports/linux/main.c $(CORE_SRC) $(CORE_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(ROOT)/ports/linux/main.c $(CORE_SRC)
