# SPINTRI.EXE: extruded Glide 2 text for Windows 98 and a Voodoo 1.
# GFALL.EXE: Privateer's Hold, mouse look, WASD, Q up, E down.
# RENDERFALL.EXE: ARCH3D model list.
# LEVELFALL.EXE: indoor and outdoor place list. Fly through one.
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

GFALL_SRC := \
	game/daggerfall/main.c \
	src/engine/platform/win32.c \
	src/engine/platform/input.c \
	src/engine/platform/fs.c \
	src/engine/core/mem.c \
	src/engine/core/log.c \
	src/engine/assets/vfs.c \
	src/engine/assets/loader.c \
	src/engine/scene/scene.c \
	src/engine/scene/light.c \
	src/engine/scene/clock.c \
	src/engine/scene/daylight.c \
	src/engine/scene/weather.c \
	src/engine/scene/collide.c \
	src/engine/scene/walker.c \
	src/engine/scene/interact.c \
	src/engine/platform/winmm.c \
	src/engine/sound/mixer.c \
	src/engine/sound/audio.c \
	src/engine/sound/sound.c \
	src/engine/sound/music.c \
	src/engine/sound/ambience.c \
	src/engine/gfx/glide.c \
	src/engine/gfx/camera.c \
	src/engine/gfx/texcache.c \
	src/engine/gfx/mip.c \
	src/engine/gfx/draw.c \
	src/engine/gfx/sky.c \
	src/engine/gfx/gfx.c \
	src/engine/ui/hud.c \
	src/engine/ui/font.c \
	src/engine/ui/console.c \
	src/engine/ui/cmd.c \
	src/engine/ui/shot.c \
	src/engine/ui/build.c \
	src/game/dagger/bsa.c \
	src/game/dagger/arch3d.c \
	src/game/dagger/texture.c \
	src/game/dagger/palette.c \
	src/game/dagger/fnt.c \
	src/game/dagger/rdb.c \
	src/game/dagger/rmb.c \
	src/game/dagger/dungeon.c \
	src/game/dagger/bind.c \
	src/game/dagger/world.c \
	src/game/dagger/doors.c \
	src/game/dagger/skyfile.c \
	src/game/dagger/forecast.c \
	src/game/dagger/player.c \
	src/game/dagger/sndfile.c \
	src/game/dagger/hmi.c \
	src/game/dagger/sounds.c

RENDER_SRC := \
	tools/renderfall/main.c \
	src/engine/platform/win32.c \
	src/engine/platform/input.c \
	src/engine/platform/fs.c \
	src/engine/core/mem.c \
	src/engine/core/log.c \
	src/engine/assets/vfs.c \
	src/engine/assets/loader.c \
	src/engine/scene/scene.c \
	src/engine/scene/light.c \
	src/engine/scene/clock.c \
	src/engine/scene/daylight.c \
	src/engine/scene/weather.c \
	src/engine/scene/collide.c \
	src/engine/scene/walker.c \
	src/engine/scene/interact.c \
	src/engine/platform/winmm.c \
	src/engine/sound/mixer.c \
	src/engine/sound/audio.c \
	src/engine/sound/sound.c \
	src/engine/sound/music.c \
	src/engine/sound/ambience.c \
	src/engine/gfx/glide.c \
	src/engine/gfx/camera.c \
	src/engine/gfx/texcache.c \
	src/engine/gfx/mip.c \
	src/engine/gfx/draw.c \
	src/engine/gfx/sky.c \
	src/engine/gfx/gfx.c \
	src/engine/ui/hud.c \
	src/engine/ui/font.c \
	src/engine/ui/console.c \
	src/engine/ui/cmd.c \
	src/engine/ui/shot.c \
	src/engine/ui/build.c \
	src/game/dagger/bsa.c \
	src/game/dagger/arch3d.c \
	src/game/dagger/texture.c \
	src/game/dagger/palette.c

LEVEL_SRC := \
	tools/levelfall/main.c \
	src/engine/platform/win32.c \
	src/engine/platform/input.c \
	src/engine/platform/fs.c \
	src/engine/core/mem.c \
	src/engine/core/log.c \
	src/engine/assets/vfs.c \
	src/engine/assets/loader.c \
	src/engine/scene/scene.c \
	src/engine/scene/light.c \
	src/engine/scene/clock.c \
	src/engine/scene/daylight.c \
	src/engine/scene/weather.c \
	src/engine/scene/collide.c \
	src/engine/scene/walker.c \
	src/engine/scene/interact.c \
	src/engine/platform/winmm.c \
	src/engine/sound/mixer.c \
	src/engine/sound/audio.c \
	src/engine/sound/sound.c \
	src/engine/sound/music.c \
	src/engine/sound/ambience.c \
	src/engine/gfx/glide.c \
	src/engine/gfx/camera.c \
	src/engine/gfx/texcache.c \
	src/engine/gfx/mip.c \
	src/engine/gfx/draw.c \
	src/engine/gfx/sky.c \
	src/engine/gfx/gfx.c \
	src/engine/ui/hud.c \
	src/engine/ui/font.c \
	src/engine/ui/console.c \
	src/engine/ui/cmd.c \
	src/engine/ui/shot.c \
	src/engine/ui/build.c \
	src/game/dagger/bsa.c \
	src/game/dagger/arch3d.c \
	src/game/dagger/texture.c \
	src/game/dagger/palette.c \
	src/game/dagger/rdb.c \
	src/game/dagger/rmb.c \
	src/game/dagger/dungeon.c \
	src/game/dagger/bind.c \
	src/game/dagger/world.c \
	src/game/dagger/doors.c \
	src/game/dagger/skyfile.c \
	src/game/dagger/forecast.c \
	src/game/dagger/sndfile.c \
	src/game/dagger/hmi.c \
	src/game/dagger/sounds.c

HOST_SRC := \
	src/engine/core/mem.c \
	src/engine/core/log.c \
	src/engine/ui/cmd.c \
	src/engine/platform/fs.c \
	src/engine/assets/vfs.c \
	src/engine/assets/loader.c \
	src/engine/scene/scene.c \
	src/engine/scene/light.c \
	src/engine/scene/clock.c \
	src/engine/scene/daylight.c \
	src/engine/scene/weather.c \
	src/engine/scene/collide.c \
	src/engine/scene/walker.c \
	src/engine/scene/interact.c \
	src/engine/sound/mixer.c \
	src/engine/sound/audio.c \
	src/engine/sound/sound.c \
	src/engine/sound/music.c \
	src/engine/sound/ambience.c \
	src/engine/gfx/camera.c \
	src/engine/gfx/mip.c \
	src/game/dagger/bsa.c \
	src/game/dagger/palette.c \
	src/game/dagger/texture.c \
	src/game/dagger/arch3d.c \
	src/game/dagger/rdb.c \
	src/game/dagger/rmb.c \
	src/game/dagger/dungeon.c \
	src/game/dagger/bind.c \
	src/game/dagger/world.c \
	src/game/dagger/doors.c \
	src/game/dagger/skyfile.c \
	src/game/dagger/forecast.c \
	src/game/dagger/player.c \
	src/game/dagger/sndfile.c \
	src/game/dagger/hmi.c \
	src/game/dagger/sounds.c

.PHONY: all check clean hostcheck gfall-check renderfall-check levelfall-check deps deps-host

deps:
	sh tools/check-deps

deps-host:
	sh tools/check-deps host

all: deps check build/spintri.iso gfall-check renderfall-check levelfall-check build/gfall.iso

build/SPINTRI.EXE: src/spintri.c third_party/glide2/include/glide.h | deps
	mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ src/spintri.c -luser32 -lkernel32 -lgcc

check: build/SPINTRI.EXE
	file build/SPINTRI.EXE | grep -q 'PE32' 
	file build/SPINTRI.EXE | grep -q '80386'
	file build/SPINTRI.EXE | grep -q 'GUI'
	$(OBJDUMP) -p build/SPINTRI.EXE > build/spintri.dump
	grep -q 'MajorOSystemVersion[[:space:]]*4' build/spintri.dump
	grep -q 'MinorOSystemVersion[[:space:]]*0' build/spintri.dump
	grep -q 'MajorSubsystemVersion[[:space:]]*4' build/spintri.dump
	grep -q 'MinorSubsystemVersion[[:space:]]*0' build/spintri.dump
	grep -q 'Subsystem[[:space:]]*00000002' build/spintri.dump
	awk 'BEGIN{bad=0} /^[[:space:]]*DLL Name:/{name=$$3; if (name!="KERNEL32.dll" && name!="USER32.dll") {print "unexpected import", name; bad=1}} END{exit bad}' build/spintri.dump
	grep -q 'LoadLibraryA' build/spintri.dump
	grep -q 'GetProcAddress' build/spintri.dump
	! grep -q 'glide2x.dll' build/spintri.dump
	! $(OBJDUMP) -d build/SPINTRI.EXE | grep -E -i -q '(^|[[:space:]])(cmov|fcmov|fucomi|fcomi|movaps|movss|addps|xmm)[[:digit:]]*'

build/cdroot/SPINTRI.EXE: build/SPINTRI.EXE
	mkdir -p build/cdroot
	cp build/SPINTRI.EXE $@

build/spintri.iso: build/cdroot/SPINTRI.EXE | deps
	$(XORRISO) -as mkisofs -o $@ -V SPINTRI -J -iso-level 1 build/cdroot

build/hostcheck: tools/hostcheck.c tools/check_engine.c tools/check_dagger.c tools/check_util.c $(HOST_SRC) | deps-host
	mkdir -p build
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tools/hostcheck.c tools/check_engine.c tools/check_dagger.c tools/check_util.c $(HOST_SRC)

hostcheck: build/hostcheck
	./build/hostcheck

build/GFALL.EXE: $(GFALL_SRC) third_party/glide2/include/glide.h | deps
	mkdir -p build
	$(CC) $(CFLAGS) -DBUILD_STAMP='"$(BUILD_STAMP)"' $(LDFLAGS) -o $@ $(GFALL_SRC) -luser32 -lkernel32 -lgcc

gfall-check: build/GFALL.EXE
	file build/GFALL.EXE | grep -q 'PE32'
	file build/GFALL.EXE | grep -q '80386'
	file build/GFALL.EXE | grep -q 'GUI'
	$(OBJDUMP) -p build/GFALL.EXE > build/gfall.dump
	grep -q 'MajorOSystemVersion[[:space:]]*4' build/gfall.dump
	grep -q 'MinorOSystemVersion[[:space:]]*0' build/gfall.dump
	grep -q 'MajorSubsystemVersion[[:space:]]*4' build/gfall.dump
	grep -q 'MinorSubsystemVersion[[:space:]]*0' build/gfall.dump
	grep -q 'Subsystem[[:space:]]*00000002' build/gfall.dump
	awk 'BEGIN{bad=0} /^[[:space:]]*DLL Name:/{name=$$3; if (name!="KERNEL32.dll" && name!="USER32.dll") {print "unexpected import", name; bad=1}} END{exit bad}' build/gfall.dump
	grep -q 'LoadLibraryA' build/gfall.dump
	grep -q 'GetProcAddress' build/gfall.dump
	! grep -q 'glide2x.dll' build/gfall.dump
	! $(OBJDUMP) -d build/GFALL.EXE | grep -E -i -q '(^|[[:space:]])(cmov|fcmov|fucomi|fcomi|movaps|movss|addps|xmm)[[:digit:]]*'

build/RENDERFALL.EXE: $(RENDER_SRC) third_party/glide2/include/glide.h | deps
	mkdir -p build
	$(CC) $(CFLAGS) -DBUILD_STAMP='"$(BUILD_STAMP)"' $(LDFLAGS) -o $@ $(RENDER_SRC) -luser32 -lkernel32 -lgcc

renderfall-check: build/RENDERFALL.EXE
	file build/RENDERFALL.EXE | grep -q 'PE32'
	file build/RENDERFALL.EXE | grep -q '80386'
	file build/RENDERFALL.EXE | grep -q 'GUI'
	$(OBJDUMP) -p build/RENDERFALL.EXE > build/renderfall.dump
	grep -q 'MajorOSystemVersion[[:space:]]*4' build/renderfall.dump
	grep -q 'MinorOSystemVersion[[:space:]]*0' build/renderfall.dump
	grep -q 'MajorSubsystemVersion[[:space:]]*4' build/renderfall.dump
	grep -q 'MinorSubsystemVersion[[:space:]]*0' build/renderfall.dump
	grep -q 'Subsystem[[:space:]]*00000002' build/renderfall.dump
	awk 'BEGIN{bad=0} /^[[:space:]]*DLL Name:/{name=$$3; if (name!="KERNEL32.dll" && name!="USER32.dll") {print "unexpected import", name; bad=1}} END{exit bad}' build/renderfall.dump
	grep -q 'LoadLibraryA' build/renderfall.dump
	grep -q 'GetProcAddress' build/renderfall.dump
	! grep -q 'glide2x.dll' build/renderfall.dump
	! $(OBJDUMP) -d build/RENDERFALL.EXE | grep -E -i -q '(^|[[:space:]])(cmov|fcmov|fucomi|fcomi|movaps|movss|addps|xmm)[[:digit:]]*'

build/LEVELFALL.EXE: $(LEVEL_SRC) third_party/glide2/include/glide.h | deps
	mkdir -p build
	$(CC) $(CFLAGS) -DBUILD_STAMP='"$(BUILD_STAMP)"' $(LDFLAGS) -o $@ $(LEVEL_SRC) -luser32 -lkernel32 -lgcc

levelfall-check: build/LEVELFALL.EXE
	file build/LEVELFALL.EXE | grep -q 'PE32'
	file build/LEVELFALL.EXE | grep -q '80386'
	file build/LEVELFALL.EXE | grep -q 'GUI'
	$(OBJDUMP) -p build/LEVELFALL.EXE > build/levelfall.dump
	grep -q 'MajorOSystemVersion[[:space:]]*4' build/levelfall.dump
	grep -q 'MinorOSystemVersion[[:space:]]*0' build/levelfall.dump
	grep -q 'MajorSubsystemVersion[[:space:]]*4' build/levelfall.dump
	grep -q 'MinorSubsystemVersion[[:space:]]*0' build/levelfall.dump
	grep -q 'Subsystem[[:space:]]*00000002' build/levelfall.dump
	awk 'BEGIN{bad=0} /^[[:space:]]*DLL Name:/{name=$$3; if (name!="KERNEL32.dll" && name!="USER32.dll") {print "unexpected import", name; bad=1}} END{exit bad}' build/levelfall.dump
	grep -q 'LoadLibraryA' build/levelfall.dump
	grep -q 'GetProcAddress' build/levelfall.dump
	! grep -q 'glide2x.dll' build/levelfall.dump
	! $(OBJDUMP) -d build/LEVELFALL.EXE | grep -E -i -q '(^|[[:space:]])(cmov|fcmov|fucomi|fcomi|movaps|movss|addps|xmm)[[:digit:]]*'

build/gfall.iso: build/GFALL.EXE build/RENDERFALL.EXE build/LEVELFALL.EXE build/hostcheck | deps
	./build/hostcheck
	$(XORRISO) -as mkisofs -o $@ -V GFALL -J -relaxed-filenames \
		-graft-points \
		/GFALL.EXE=build/GFALL.EXE \
		/RENDERFALL.EXE=build/RENDERFALL.EXE \
		/LEVELFALL.EXE=build/LEVELFALL.EXE

clean:
	rm -rf build
