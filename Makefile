PVSNESLIB_HOME ?= $(HOME)/pvsneslib

# The PVSnesLib toolchain (snes_rules) has side effects at parse time (it
# shells out to generate hdr.asm) and requires PVSNESLIB_HOME to point at a
# real install. Task 1-7 targets (test/clean/all, appended below this file)
# are pure host-side C and must keep working even when PVSnesLib is not
# installed, so the include only happens when `rom`/`rom-script`/
# `rom-measure` is the goal. snes_rules' own recipes re-invoke `make` as a
# child process (e.g. `make buildActual`), so BUILD_ROM (and, for the
# scripted and measurement builds, BUILD_ROM_SCRIPT/BUILD_ROM_MEASURE below)
# is exported to the environment to keep the guard true there too.
ifneq (,$(filter rom rom-script rom-measure rom-tutorial,$(MAKECMDGOALS))$(BUILD_ROM))

export PVSNESLIB_HOME
export BUILD_ROM := 1

# src/snes/*.c (main.c, render.c, input.c) #include headers from src/core/
# by quoted name (e.g. "match.h") without a relative path; snes_rules' own
# CFLAGS only adds $(CURDIR) (the repo root), so src/core needs its own -I
# here.
CFLAGS += -Isrc/core

# Task 10: `rom-script` builds a second ROM, life-script.sfc, from the same
# sources but with input.c's INPUT_SCRIPT path compiled in (a fixed replay
# table instead of padsCurrent(0)) for headless screenshot verification
# (docs/snes-notes.md §2). Its own buildWithSummary
# recipe re-invokes `make buildActual` exactly like `rom`'s does, so
# BUILD_ROM_SCRIPT is exported the same way BUILD_ROM is above, and this
# branch re-evaluates identically in that child process.
# `rom-measure` builds a third ROM, life-measure.sfc: the scripted input
# above plus main.c's AI_MEASURE_FRAMES counters, which show the frames and
# loop iterations of the last CPU turn in place of the round counter
# (docs/snes-notes.md §9). Opt-in only: `make rom` never defines it.
# `rom-tutorial` builds life-tutorial.sfc, which boots straight into the
# tutorial (main.c, BOOT_TUTORIAL) so it can be captured without a pad.
ifneq (,$(filter rom-tutorial,$(MAKECMDGOALS))$(BUILD_ROM_TUTORIAL))
export BUILD_ROM_TUTORIAL := 1
export ROMNAME := life-tutorial
export ROMTITLE := LIFE GAME TUTORIAL
CFLAGS += -DBOOT_TUTORIAL
else ifneq (,$(filter rom-measure,$(MAKECMDGOALS))$(BUILD_ROM_MEASURE))
export BUILD_ROM_MEASURE := 1
export ROMNAME := life-measure
export ROMTITLE := LIFE GAME MEASURE
CFLAGS += -DINPUT_SCRIPT -DAI_MEASURE_FRAMES
else ifneq (,$(filter rom-script,$(MAKECMDGOALS))$(BUILD_ROM_SCRIPT))
export BUILD_ROM_SCRIPT := 1
export ROMNAME := life-script
# hdr.asm's NAME directive caps out at 21 letters; "SNES LIFE GAME" (14) has
# no room left for a "script" suffix.
export ROMTITLE := LIFE GAME SCRIPT
CFLAGS += -DINPUT_SCRIPT
else
export ROMNAME := life
export ROMTITLE := SNES LIFE GAME
endif

GFX4SNES := $(PVSNESLIB_HOME)/devkitsnes/tools/gfx4snes

# A single explicit multi-target rule would run its recipe once per
# out-of-date target under GNU Make 3.81 (no grouped-target `&:` support
# here), invoking gfx4snes twice on a clean build and racing under `-j`;
# gfx4snes writes both files from one invocation, so only .pic runs the
# recipe and .pal is declared to come along for free. Same reasoning for
# data/sprites.bmp: tools/mktiles.py writes both data/tiles.bmp and
# data/sprites.bmp in the one invocation triggered by data/tiles.bmp's own
# rule below, so data/sprites.bmp is declared to come along for free too.
data/tiles.pic: data/tiles.bmp
	$(GFX4SNES) -s 8 -o 16 -u 16 -t bmp -e 0 -p -i $<

data/tiles.pal: data/tiles.pic ;

data/tiles.bmp: tools/mktiles.py
	python3 $<

data/sprites.bmp: data/tiles.bmp ;

data/sprites.pic: data/sprites.bmp
	$(GFX4SNES) -s 8 -o 16 -u 16 -t bmp -e 0 -p -i $<

data/sprites.pal: data/sprites.pic ;

# snesmod : le premier module de la liste fournit les effets (smconv -f
# vérifie que chaque musique tient avec eux), puis les trois musiques.
AUDIOFILES := data/audio/sfx.it data/audio/alonely.it data/audio/offerthelight.it data/audio/purity.it
export SOUNDBANK := data/audio/soundbank
SMCONVFLAGS := -s -o $(SOUNDBANK) -V -b 5 -f
CFLAGS += -Idata/audio

data/audio/sfx.it: tools/mksfx.py $(wildcard data/audio/sfx/*.wav)
	python3 tools/mksfx.py

include $(PVSNESLIB_HOME)/devkitsnes/snes_rules

# sound.c inclut soundbank.h, produit par smconv avec la banque.
src/snes/sound.ps: $(SOUNDBANK).asm

# src/snes/tiles.asm/sprites.asm .incbin data/tiles.{pic,pal} and
# data/sprites.{pic,pal} (via the gfx4snes-generated data/*_data.as); make
# does not see through that .include, so the assembler's own prerequisite
# (data/*.bmp -> ... -> the .obj rules below) must be declared explicitly
# or a from-clean build tries to assemble before the art is converted.
src/snes/tiles.obj: data/tiles.pic data/tiles.pal
src/snes/sprites.obj: data/sprites.pic data/sprites.pal

# CFLAGS (in particular -DINPUT_SCRIPT above) only takes effect at the
# .c -> .ps compile step of snes_rules' own pattern rules; the resulting
# .ps -> .asm -> .obj chain is otherwise ordinary mtime-based make, with no
# dependency on CFLAGS at all. Switching between `rom` and `rom-script`
# without touching any source would therefore leave stale intermediates
# compiled with the *other* target's flags lying around and silently link
# them into the new ROM. Force every C source through the whole chain again
# on every `rom`/`rom-script`/`rom-measure` build so they never
# cross-contaminate —
# this is also what keeps a plain `make rom` byte-identical regardless of
# whether `rom-script` ran first. Hand-written .asm sources (tiles.asm,
# sprites.asm — no matching .c file) are left alone: their .obj never
# depends on CFLAGS.
SNES_CFILES := $(wildcard src/*.c) $(wildcard src/*/*.c) $(wildcard src/*/*/*.c)
SNES_C_INTERMEDIATES := $(SNES_CFILES:.c=.obj) $(SNES_CFILES:.c=.asm) $(SNES_CFILES:.c=.ps)

.PHONY: clean-snes-intermediates
clean-snes-intermediates:
	rm -f $(SNES_C_INTERMEDIATES)

# C statics live in bank 7E from 7E:2000 up (the .bss sections of the C
# objects); PVSnesLib's own RAM sections start at 7E:8000 and wlalink does
# not report an overlap between the two. Every ROM recipe therefore reads the
# end of the last C .bss section from the generated .symfull and fails if it
# goes past RAM_LIMIT (docs/snes-notes.md section 10). Addresses are 8
# lowercase hex digits, so a plain string comparison orders them.
# Mémoire son : musique + effets <= 58 Ko (59392 octets) pour chaque
# musique, lu dans l'en-tête que smconv génère (MOD_*_SIZE).
SOUND_LIMIT ?= 59392
define check_sound
	@awk -v lim=$(SOUND_LIMIT) ' \
	    $$1 ~ /define$$/ && $$2 ~ /^MOD_.*_SIZE$$/ { size[$$2] = $$3 } \
	    END { \
	        sfx = size["MOD_SFX_SIZE"]; if (sfx == "") { print "Sound check: no MOD_SFX_SIZE"; exit 1 } \
	        bad = 0; \
	        for (k in size) if (k != "MOD_SFX_SIZE") { \
	            t = size[k] + sfx; \
	            if (t > lim) { print "Sound check FAILED: " k " + effects = " t " > " lim; bad = 1 } \
	            else print "Sound check: " k " + effects = " t " (limit " lim ")" } \
	        exit bad }' $(SOUNDBANK).h
endef

RAM_LIMIT ?= 007e8000
define check_ram
	@awk -v lim=$(RAM_LIMIT) -v f=$(1) ' \
	    $$2 == "SECTIONEND_.bss" { a = tolower($$1); if (a > top) top = a } \
	    END { \
	        if (top == "") { print "RAM check: no .bss section found in " f; exit 1 } \
	        if (top > lim) { \
	            print "RAM check FAILED: C statics end at " top ", past the limit " lim \
	                  " (PVSnesLib RAM starts at 7E:8000, see docs/snes-notes.md section 10)"; \
	            exit 1 } \
	        print "RAM check: C statics end at " top " (limit " lim ")" }' $(1)
endef

.PHONY: rom rom-script rom-measure rom-tutorial
rom: clean-snes-intermediates buildWithSummary
	$(call check_sound)
	$(call check_ram,$(ROMNAME).symfull)
	mkdir -p build
	mv $(ROMNAME).sfc build/
	mv $(ROMNAME).sym build/
	mv $(ROMNAME).symfull build/

rom-script: clean-snes-intermediates buildWithSummary
	$(call check_sound)
	$(call check_ram,$(ROMNAME).symfull)
	mkdir -p build
	mv $(ROMNAME).sfc build/
	mv $(ROMNAME).sym build/
	mv $(ROMNAME).symfull build/

rom-measure: clean-snes-intermediates buildWithSummary
	$(call check_sound)
	$(call check_ram,$(ROMNAME).symfull)
	mkdir -p build
	mv $(ROMNAME).sfc build/
	mv $(ROMNAME).sym build/
	mv $(ROMNAME).symfull build/

rom-tutorial: clean-snes-intermediates buildWithSummary
	$(call check_sound)
	$(call check_ram,$(ROMNAME).symfull)
	mkdir -p build
	mv $(ROMNAME).sfc build/
	mv $(ROMNAME).sym build/
	mv $(ROMNAME).symfull build/

buildActual: $(OFILES) $(ROMNAME).sfc

else

.PHONY: rom rom-script rom-measure rom-tutorial
rom rom-script rom-measure rom-tutorial:
	@echo "Run 'make rom', 'make rom-script', 'make rom-measure' or 'make rom-tutorial' on its own (not combined with other targets)."
	@exit 1

endif

CORE_SRC  := $(wildcard src/core/*.c)
CORE_HDR  := $(wildcard src/core/*.h)
TEST_SRC  := $(wildcard tests/*.c)
TEST_HDR  := $(wildcard tests/*.h)
# AI_TEST_HOOKS : points d'entrée réservés aux tests hôtes (ai.h), jamais
# compilés dans une ROM.
HOSTFLAGS := -std=c89 -pedantic -Wall -Wextra -Werror -Isrc/core -Itests -g -DAI_TEST_HOOKS \
             -fsanitize=address,undefined
SIMFLAGS  := -std=c99 -Wall -Wextra -O2 -Isrc/core

.PHONY: test clean sim
test: build/run-tests
	./build/run-tests
	python3 tools/test_mksfx.py

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
