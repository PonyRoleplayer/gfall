#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/platform/winmm.h"
#include "engine/core/log.h"

/*
 * See winmm.h. Every Windows 95 and 98 has WINMM.DLL; the stream calls
 * are there from Windows 95 on. A missing one fails the whole table, so
 * a caller checks one pointer.
 */

#ifndef DF_HOST
static Winmm g_mm;
static int g_tried;
static int g_ok;

#define BIND(field) \
    do { \
        FARPROC sym; \
        sym = GetProcAddress(dll, #field); \
        if (!sym) { \
            log_print(LOG_WARN, "winmm: no %s", #field); \
            return 0; \
        } \
        *(FARPROC *)(void *)&g_mm.field = sym; \
    } while (0)

static int bind_all(void)
{
    HMODULE dll;

    dll = LoadLibraryA("WINMM.DLL");
    if (!dll) {
        log_text(LOG_WARN, "winmm: WINMM.DLL did not load");
        return 0;
    }
    BIND(waveOutOpen);
    BIND(waveOutClose);
    BIND(waveOutPrepareHeader);
    BIND(waveOutUnprepareHeader);
    BIND(waveOutWrite);
    BIND(waveOutReset);
    BIND(midiOutGetNumDevs);
    BIND(midiOutGetDevCapsA);
    BIND(midiStreamOpen);
    BIND(midiStreamClose);
    BIND(midiStreamProperty);
    BIND(midiStreamOut);
    BIND(midiStreamRestart);
    BIND(midiStreamStop);
    BIND(midiOutPrepareHeader);
    BIND(midiOutUnprepareHeader);
    BIND(midiOutShortMsg);
    BIND(midiOutReset);
    BIND(midiOutSetVolume);
    return 1;
}

const Winmm *winmm(void)
{
    if (!g_tried) {
        g_tried = 1;
        g_ok = bind_all();
    }
    return g_ok ? &g_mm : 0;
}
#endif
