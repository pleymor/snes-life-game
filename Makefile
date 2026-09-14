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
