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

# src/snes/*.c (main.c, render.c) #include headers from src/core/ by quoted
# name (e.g. "match.h") without a relative path; snes_rules' own CFLAGS only
# adds $(CURDIR) (the repo root), so src/core needs its own -I here.
CFLAGS += -Isrc/core

GFX4SNES := $(PVSNESLIB_HOME)/devkitsnes/tools/gfx4snes

data/tiles.pic data/tiles.pal: data/tiles.bmp
	$(GFX4SNES) -s 8 -o 16 -u 16 -t bmp -e 0 -p -i $<

data/tiles.bmp: tools/mktiles.py
	python3 $<

include $(PVSNESLIB_HOME)/devkitsnes/snes_rules

# src/snes/tiles.asm .incbin's data/tiles.pic and data/tiles.pal (via the
# gfx4snes-generated data/tiles_data.as); make does not see through that
# .include, so the assembler's own prerequisite (data/tiles.bmp -> ... ->
# the .obj rule below) must be declared explicitly or a from-clean build
# tries to assemble before the art is converted.
src/snes/tiles.obj: data/tiles.pic data/tiles.pal

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
CORE_HDR  := $(wildcard src/core/*.h)
TEST_SRC  := $(wildcard tests/*.c)
TEST_HDR  := $(wildcard tests/*.h)
HOSTFLAGS := -std=c89 -pedantic -Wall -Wextra -Werror -Isrc/core -Itests -g \
             -fsanitize=address,undefined
SIMFLAGS  := -std=c99 -Wall -Wextra -O2 -Isrc/core

.PHONY: test clean sim
test: build/run-tests
	./build/run-tests

# Les en-têtes sont des prérequis (config.h en particulier : un balayage de
# réglages qui ne touche que config.h doit forcer la recompilation), mais ne
# vont pas sur la ligne de commande du compilateur : $(filter %.c,$^) ne
# retient que les sources.
build/run-tests: $(CORE_SRC) $(CORE_HDR) $(TEST_SRC) $(TEST_HDR) | build
	$(CC) $(HOSTFLAGS) $(filter %.c,$^) -o $@

sim: build/sim
build/sim: $(CORE_SRC) $(CORE_HDR) tools/sim.c | build
	$(CC) $(SIMFLAGS) $(filter %.c,$^) -o $@

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
