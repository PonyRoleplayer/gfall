# SPINTRI.EXE: extruded Glide 2 text for Windows 98 and a Voodoo 1.
# GFALL.EXE: Privateer's Hold, mouse look, WASD, Q up, E down.
# RENDERFALL.EXE: ARCH3D model list.
# LEVELFALL.EXE: indoor and outdoor place list. Fly through one.
# DATFALL.EXE: record-file viewer. DATA\MAIN.DAT, or a file you open.
# make
# Mount build/spintri.iso and run D:\SPINTRI.EXE, or mount build/gfall.iso
# and run D:\GFALL.EXE. The disc is the executables only. ARENA2 stays
# beside them on the machine. It is not copied onto the disc.
# The guest needs the 3dfx Voodoo Graphics driver (glide2x.dll). Escape quits.
# Do not copy glide2x.dll onto either disc. The system driver is the one
# that draws the 3dfx splash.
#
# Tools: sh tools/check-deps
#        i686-w64-mingw32-gcc, i686-w64-mingw32-objdump
#        (gcc-mingw-w64-i686), xorriso, file, and a native gcc for hostcheck.
# glide2x.dll is loaded at runtime. The driver export has no @N suffix,
# and a static import of grBufferClear@12 will not start.

CC := i686-w64-mingw32-gcc
OBJDUMP := i686-w64-mingw32-objdump
XORRISO := xorriso

BUILD_STAMP := $(shell date +%Y-%m-%dT%H:%M:%S)

CFLAGS := -std=gnu90 -Wall -Wextra -O2 \
	-march=pentium-mmx -mfpmath=387 -mno-sse \
	-fno-stack-protector -fno-asynchronous-unwind-tables -mno-stack-arg-probe \
	-DWINVER=0x0400 -D_WIN32_WINNT=0x0400 -D__WIN32__ \
	-Isrc -Ithird_party/glide2/include

LDFLAGS := -nostdlib \
	-Wl,--subsystem,windows:4.0 \
	-Wl,--major-os-version,4,--minor-os-version,0 \
	-Wl,--major-subsystem-version,4,--minor-subsystem-version,0 \
	-Wl,--disable-dynamicbase,--disable-nxcompat \
	-Wl,--disable-stdcall-fixup \
	-Wl,-e,_WinMainCRTStartup \
	-Wl,-s

HOST_CC := gcc
HOST_CFLAGS := -std=gnu90 -Wall -Wextra -O2 -DDF_HOST -Isrc

# Sources by layer (docs/architecture.md). A file includes only its own
# layer and those listed before it: the engine; then Daggerfall's
# formats, tables, places and play (its game and its player); then the
# programs in apps/.

# The engine that hostcheck runs: plain C, no Win32 or Glide.
ENGINE_HOST_SRC := \
	src/engine/core/mem.c \
	src/engine/core/log.c \
	src/engine/core/prof.c \
	src/engine/ui/cmd.c \
	src/engine/ui/pic.c \
	src/engine/platform/fs.c \
	src/engine/platform/keyname.c \
	src/engine/assets/vfs.c \
	src/engine/assets/loader.c \
	src/engine/assets/recfile.c \
	src/engine/scene/scene.c \
	src/engine/scene/light.c \
	src/engine/scene/clock.c \
	src/engine/scene/daylight.c \
	src/engine/scene/weather.c \
	src/engine/scene/collide.c \
	src/engine/scene/body.c \
	src/engine/scene/walker.c \
	src/engine/scene/interact.c \
	src/engine/scene/keep.c \
	src/engine/scene/entity.c \
	src/engine/scene/stream.c \
	src/engine/sound/mixer.c \
	src/engine/sound/audio.c \
	src/engine/sound/sound.c \
	src/engine/sound/music.c \
	src/engine/sound/ambience.c \
	src/engine/gfx/camera.c \
	src/engine/gfx/rig.c \
	src/engine/gfx/mip.c \
	src/engine/gfx/texres.c

# The rest of the engine: the window, the card, the screen.
ENGINE_SRC := $(ENGINE_HOST_SRC) \
	src/engine/platform/win32.c \
	src/engine/platform/input.c \
	src/engine/platform/winmm.c \
	src/engine/gfx/glide.c \
	src/engine/gfx/texcache.c \
	src/engine/gfx/draw.c \
	src/engine/gfx/sky.c \
	src/engine/gfx/gfx.c \
	src/engine/ui/hud.c \
	src/engine/ui/font.c \
	src/engine/ui/console.c \
	src/engine/ui/shot.c \
	src/engine/ui/build.c

# Daggerfall's files, read.
FORMATS_SRC := \
	src/game/dagger/formats/bsa.c \
	src/game/dagger/formats/arch3d.c \
	src/game/dagger/formats/texture.c \
	src/game/dagger/formats/palette.c \
	src/game/dagger/formats/fnt.c \
	src/game/dagger/formats/rdb.c \
	src/game/dagger/formats/rmb.c \
	src/game/dagger/formats/dungeon.c \
	src/game/dagger/formats/bind.c \
	src/game/dagger/formats/worldmap.c \
	src/game/dagger/formats/skyfile.c \
	src/game/dagger/formats/sndfile.c \
	src/game/dagger/formats/hmi.c \
	src/game/dagger/formats/img.c \
	src/game/dagger/formats/cif.c \
	src/game/dagger/formats/classcfg.c

# Daggerfall's rules for dressing a place.
TABLES_SRC := \
	src/game/dagger/tables/maindat.c \
	src/game/dagger/tables/classic.c \
	src/game/dagger/tables/calendar.c \
	src/game/dagger/tables/careers.c \
	src/game/dagger/tables/items.c \
	src/game/dagger/tables/swaps.c \
	src/game/dagger/tables/enemies.c \
	src/game/dagger/tables/forecast.c \
	src/game/dagger/tables/sounds.c

# A place built from the files: a Level the engine draws.
PLACES_SRC := \
	src/game/dagger/places/world.c \
	src/game/dagger/places/doors.c \
	src/game/dagger/places/wild.c \
	src/game/dagger/places/people.c \
	src/game/dagger/places/terrain.c \
	src/game/dagger/places/wres.c \
	src/game/dagger/places/dunstream.c

DAGGER_SRC := $(FORMATS_SRC) $(TABLES_SRC) $(PLACES_SRC)

# Daggerfall's game: its player, its rules. LEVELFALL does not link it.
PLAY_SRC := \
	src/game/dagger/play/rules.c \
	src/game/dagger/play/character.c \
	src/game/dagger/play/inventory.c \
	src/game/dagger/play/commands.c \
	src/game/dagger/play/weapon.c \
	src/game/dagger/play/combat.c \
	src/game/dagger/play/foes.c \
	src/game/dagger/play/sfx.c \
	src/game/dagger/play/player.c \
	src/game/dagger/play/screen.c \
	src/game/dagger/play/overlay.c \
	src/game/dagger/play/menu.c \
	src/game/dagger/play/keys.c \
	src/game/dagger/play/game.c

# What of the game draws, reaches the card or the keys: not built for the host.
PLAY_DRAW_SRC := \
	src/game/dagger/play/screen.c \
	src/game/dagger/play/overlay.c \
	src/game/dagger/play/menu.c \
	src/game/dagger/play/keys.c \
	src/game/dagger/play/game.c

GFALL_SRC := apps/gfall/main.c $(ENGINE_SRC) $(DAGGER_SRC) $(PLAY_SRC)

# RENDERFALL shows models alone: the readers they need and no place.
RENDER_SRC := apps/renderfall/main.c $(ENGINE_SRC) \
	src/game/dagger/formats/bsa.c \
	src/game/dagger/formats/arch3d.c \
	src/game/dagger/formats/texture.c \
	src/game/dagger/formats/palette.c

# LEVELFALL draws with the engine's own face, not FONT0000.FNT.
LEVEL_SRC := apps/levelfall/main.c $(ENGINE_SRC) \
	$(filter-out src/game/dagger/formats/fnt.c,$(DAGGER_SRC))

HOST_SRC := $(ENGINE_HOST_SRC) \
	$(filter-out src/game/dagger/formats/fnt.c,$(DAGGER_SRC)) $(filter-out $(PLAY_DRAW_SRC),$(PLAY_SRC))

.PHONY: all check clean hostcheck datacheck gfall-check renderfall-check levelfall-check datfall-check deps deps-host layers

# A bare make builds everything; deps alone is the first rule below.
.DEFAULT_GOAL := all

# The rules every executable must pass: a PE32 80386 GUI program for
# Windows 4.0 (95, 98, NT4), importing only KERNEL32 and USER32, binding
# glide2x.dll at runtime, with no P6 or SSE instruction.
# $(call pe_check,EXE,DUMP)
define pe_check
	file $(1) | grep -q 'PE32'
	file $(1) | grep -q '80386'
	file $(1) | grep -q 'GUI'
	$(OBJDUMP) -p $(1) > $(2)
	grep -q 'MajorOSystemVersion[[:space:]]*4' $(2)
	grep -q 'MinorOSystemVersion[[:space:]]*0' $(2)
	grep -q 'MajorSubsystemVersion[[:space:]]*4' $(2)
	grep -q 'MinorSubsystemVersion[[:space:]]*0' $(2)
	grep -q 'Subsystem[[:space:]]*00000002' $(2)
	awk 'BEGIN{bad=0} /^[[:space:]]*DLL Name:/{name=$$3; if (name!="KERNEL32.dll" && name!="USER32.dll") {print "unexpected import", name; bad=1}} END{exit bad}' $(2)
	grep -q 'LoadLibraryA' $(2)
	grep -q 'GetProcAddress' $(2)
	! grep -q 'glide2x.dll' $(2)
	! $(OBJDUMP) -d $(1) | grep -E -i -q '(^|[[:space:]])(cmov|fcmov|fucomi|fcomi|movaps|movss|addps|xmm)[[:digit:]]*'
endef

deps: layers
	sh tools/check-deps

deps-host: layers
	sh tools/check-deps host

layers:
	sh tools/check-layers

all: deps check build/spintri.iso gfall-check renderfall-check levelfall-check datfall-check build/gfall.iso

build/SPINTRI.EXE: apps/spintri/main.c third_party/glide2/include/glide.h | deps
	mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ apps/spintri/main.c -luser32 -lkernel32 -lgcc

check: build/SPINTRI.EXE
	$(call pe_check,build/SPINTRI.EXE,build/spintri.dump)

build/cdroot/SPINTRI.EXE: build/SPINTRI.EXE
	mkdir -p build/cdroot
	cp build/SPINTRI.EXE $@

build/spintri.iso: build/cdroot/SPINTRI.EXE | deps
	$(XORRISO) -as mkisofs -o $@ -V SPINTRI -J -iso-level 1 build/cdroot

build/hostcheck: tests/hostcheck.c tests/check_engine.c tests/check_dagger.c tests/check_play.c tests/check_util.c tests/host_card.c $(HOST_SRC) | deps-host
	mkdir -p build
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tests/hostcheck.c tests/check_engine.c tests/check_dagger.c tests/check_play.c tests/check_util.c tests/host_card.c $(HOST_SRC)

hostcheck: build/hostcheck datacheck
	./build/hostcheck

# dfrec, the .DAT tool (docs/data.md): native, from the same code as hostcheck.
build/dfrec: apps/dfrec/main.c tests/host_card.c $(HOST_SRC) | deps-host
	mkdir -p build
	$(HOST_CC) $(HOST_CFLAGS) -o $@ apps/dfrec/main.c tests/host_card.c $(HOST_SRC)

# MAIN.DAT cooked, dumped to text, built again from the text and merged
# to the same records; the example mod built; a misspelled field fails
# with its line.
datacheck: build/dfrec
	mkdir -p build/data
	./build/dfrec import daggerfall/DAGGER/ARENA2 build/data/MAIN.DAT
	./build/dfrec dump build/data/MAIN.DAT > build/data/main.txt
	./build/dfrec build build/data/main.txt build/data/MAIN2.DAT
	./build/dfrec same build/data/MAIN.DAT build/data/MAIN2.DAT
	./build/dfrec build tests/data/goblins.txt build/data/GOBLINS.DAT
	! ./build/dfrec build tests/data/misspelled.txt build/data/BAD.DAT 2> build/data/bad.log
	grep -q 'misspelled.txt:4: no such field' build/data/bad.log
	test ! -e build/data/BAD.DAT

build/GFALL.EXE: $(GFALL_SRC) third_party/glide2/include/glide.h | deps
	mkdir -p build
	$(CC) $(CFLAGS) -DBUILD_STAMP='"$(BUILD_STAMP)"' $(LDFLAGS) -o $@ $(GFALL_SRC) -luser32 -lkernel32 -lgcc

gfall-check: build/GFALL.EXE
	$(call pe_check,build/GFALL.EXE,build/gfall.dump)

build/RENDERFALL.EXE: $(RENDER_SRC) third_party/glide2/include/glide.h | deps
	mkdir -p build
	$(CC) $(CFLAGS) -DBUILD_STAMP='"$(BUILD_STAMP)"' $(LDFLAGS) -o $@ $(RENDER_SRC) -luser32 -lkernel32 -lgcc

renderfall-check: build/RENDERFALL.EXE
	$(call pe_check,build/RENDERFALL.EXE,build/renderfall.dump)

build/LEVELFALL.EXE: $(LEVEL_SRC) third_party/glide2/include/glide.h | deps
	mkdir -p build
	$(CC) $(CFLAGS) -DBUILD_STAMP='"$(BUILD_STAMP)"' $(LDFLAGS) -o $@ $(LEVEL_SRC) -luser32 -lkernel32 -lgcc

levelfall-check: build/LEVELFALL.EXE
	$(call pe_check,build/LEVELFALL.EXE,build/levelfall.dump)

# DATFALL browses races, enemies and weapons (docs/data.md). It does not
# link Glide. GDI and comctl32 are LoadLibrary'd, so the import table
# stays KERNEL32 and USER32. The host half checks a Breton body pixel,
# the rat's front frame and a dwarven weapon colour, without a window.
DAT_SRC := \
	apps/datfall/view.c \
	src/engine/core/mem.c \
	src/engine/core/log.c \
	src/engine/assets/recfile.c \
	src/engine/assets/vfs.c \
	src/engine/platform/fs.c \
	src/engine/scene/clock.c \
	src/engine/scene/weather.c \
	src/game/dagger/formats/classcfg.c \
	src/game/dagger/formats/bsa.c \
	src/game/dagger/tables/maindat.c \
	src/game/dagger/tables/classic.c \
	src/game/dagger/tables/calendar.c \
	src/game/dagger/tables/careers.c \
	src/game/dagger/tables/items.c \
	src/game/dagger/tables/enemies.c \
	src/game/dagger/tables/forecast.c \
	src/game/dagger/formats/texture.c \
	src/game/dagger/formats/palette.c \
	src/game/dagger/formats/img.c \
	src/game/dagger/formats/cif.c \
	src/engine/gfx/mip.c

build/DATFALL.EXE: apps/datfall/main.c $(DAT_SRC) | deps
	mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ apps/datfall/main.c $(DAT_SRC) -luser32 -lkernel32 -lgcc

build/datfall-host: apps/datfall/check.c $(DAT_SRC) | deps-host
	mkdir -p build
	$(HOST_CC) $(HOST_CFLAGS) -o $@ apps/datfall/check.c $(DAT_SRC)

datfall-check: build/DATFALL.EXE build/datfall-host
	$(call pe_check,build/DATFALL.EXE,build/datfall.dump)
	./build/datfall-host

build/gfall.iso: build/GFALL.EXE build/RENDERFALL.EXE build/LEVELFALL.EXE build/DATFALL.EXE build/hostcheck | deps
	./build/hostcheck
	$(XORRISO) -as mkisofs -o $@ -V GFALL -J -relaxed-filenames \
		-graft-points \
		/GFALL.EXE=build/GFALL.EXE \
		/RENDERFALL.EXE=build/RENDERFALL.EXE \
		/LEVELFALL.EXE=build/LEVELFALL.EXE \
		/DATFALL.EXE=build/DATFALL.EXE

clean:
	rm -rf build
