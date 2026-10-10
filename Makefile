# SPINTRI.EXE: extruded Glide 2 text for Windows 98 and a Voodoo 1.
# GFALL.EXE: one Daggerfall block, mouse look, WASD, Q up, E down.
# make
# Mount build/spintri.iso and run D:\SPINTRI.EXE, or mount build/gfall.iso
# and run D:\GFALL.EXE. The disc carries the whole ARENA2 tree beside the exe.
# The guest needs the 3dfx Voodoo Graphics driver (glide2x.dll). Escape quits.
# Do not copy glide2x.dll onto either disc. The system driver is the one
# that draws the 3dfx splash.
#
# Tools: i686-w64-mingw32-gcc, i686-w64-mingw32-objdump
#        (gcc-mingw-w64-i686-win32), xorriso, and a native gcc for hostcheck.
# glide2x.dll is loaded at runtime. The driver export has no @N suffix,
# and a static import of grBufferClear@12 will not start.

CC := i686-w64-mingw32-gcc
OBJDUMP := i686-w64-mingw32-objdump
XORRISO := xorriso

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
	src/main.c \
	src/platform/win32.c \
	src/platform/input.c \
	src/platform/fs.c \
	src/platform/mem.c \
	src/assets/bsa.c \
	src/assets/arch3d.c \
	src/assets/texture.c \
	src/assets/palette.c \
	src/assets/rdb.c \
	src/render/glide.c \
	src/render/camera.c \
	src/render/texcache.c \
	src/render/draw.c \
	src/render/hud.c \
	src/game/world.c \
	src/game/player.c

HOST_SRC := \
	src/platform/mem.c \
	src/platform/fs.c \
	src/assets/bsa.c \
	src/assets/palette.c \
	src/assets/texture.c \
	src/assets/arch3d.c \
	src/assets/rdb.c \
	src/game/world.c \
	src/game/player.c \
	src/render/camera.c

.PHONY: all check clean hostcheck gfall-check

all: check build/spintri.iso gfall-check build/gfall.iso

build/SPINTRI.EXE: src/spintri.c third_party/glide2/include/glide.h
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

build/spintri.iso: build/cdroot/SPINTRI.EXE
	$(XORRISO) -as mkisofs -o $@ -V SPINTRI -J -iso-level 1 build/cdroot

build/hostcheck: tools/hostcheck.c $(HOST_SRC)
	mkdir -p build
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tools/hostcheck.c $(HOST_SRC)

hostcheck: build/hostcheck
	./build/hostcheck

build/GFALL.EXE: $(GFALL_SRC) third_party/glide2/include/glide.h
	mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(GFALL_SRC) -luser32 -lkernel32 -lgcc

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

build/gfall.iso: build/GFALL.EXE build/hostcheck
	./build/hostcheck
	$(XORRISO) -as mkisofs -o $@ -V GFALL -J -relaxed-filenames \
		-graft-points \
		/GFALL.EXE=build/GFALL.EXE \
		/ARENA2/=daggerfall/DAGGER/ARENA2

clean:
	rm -rf build
