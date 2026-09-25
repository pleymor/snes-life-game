PVSNESLIB_HOME ?= $(HOME)/pvsneslib

# The PVSnesLib toolchain (snes_rules) has side effects at parse time (it
# shells out to generate hdr.asm) and requires PVSNESLIB_HOME to point at a
# real install. Task 1-7 targets (test/clean/all, appended below this file)
# are pure host-side C and must keep working even when PVSnesLib is not
# installed, so the include only happens when `rom` is the goal. snes_rules'
# own recipes re-invoke `make` as a child process (e.g. `make buildActual`),
# so BUILD_ROM is exported to the environment to keep the guard true there too.
ifneq (,$(filter rom,$(MAKECMDGOALS))$(BUILD_ROM))

export PVSNESLIB_HOME
export BUILD_ROM := 1
export ROMNAME := life
export ROMTITLE := SNES LIFE GAME

include $(PVSNESLIB_HOME)/devkitsnes/snes_rules

.PHONY: rom
rom: buildWithSummary
	mkdir -p build
	mv $(ROMNAME).sfc build/
	mv $(ROMNAME).sym build/
	mv $(ROMNAME).symfull build/

buildActual: $(OFILES) $(ROMNAME).sfc

else

.PHONY: rom
rom:
	@echo "Run 'make rom' on its own (not combined with other targets)."
	@exit 1

endif

CORE_SRC  := $(wildcard src/core/*.c)
TEST_SRC  := $(wildcard tests/*.c)
HOSTFLAGS := -std=c89 -pedantic -Wall -Wextra -Werror -Isrc/core -Itests -g \
             -fsanitize=address,undefined
SIMFLAGS  := -std=c99 -Wall -Wextra -O2 -Isrc/core

.PHONY: test clean sim
test: build/run-tests
	./build/run-tests

build/run-tests: $(CORE_SRC) $(TEST_SRC) | build
	$(CC) $(HOSTFLAGS) $^ -o $@

sim: build/sim
build/sim: $(CORE_SRC) tools/sim.c | build
	$(CC) $(SIMFLAGS) $^ -o $@

build:
	mkdir -p build

clean:
	rm -rf build

# `make` seul lance les tests, pour que le cycle rouge-vert reste immédiat.
# `make all` y ajoute la ROM, une fois la tâche 0 passée.
.DEFAULT_GOAL := test
.PHONY: all
all: test
	$(MAKE) rom
