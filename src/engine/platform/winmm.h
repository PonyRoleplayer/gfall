#ifndef DF_WINMM_H
#define DF_WINMM_H

/*
 * WINMM.DLL, loaded at runtime like glide2x.dll: the programs import
 * only KERNEL32 and USER32. The wave calls are the sound's last resort
 * (engine/sound/audio.c); the MIDI stream calls play the music
 * (engine/sound/music.c). Include after windows.h, Win32 builds only.
 */

#ifndef DF_HOST
#include <mmsystem.h>

typedef struct Winmm {
    MMRESULT (WINAPI *waveOutOpen)(LPHWAVEOUT, UINT, LPCWAVEFORMATEX, DWORD_PTR, DWORD_PTR, DWORD);
    MMRESULT (WINAPI *waveOutClose)(HWAVEOUT);
    MMRESULT (WINAPI *waveOutPrepareHeader)(HWAVEOUT, LPWAVEHDR, UINT);
    MMRESULT (WINAPI *waveOutUnprepareHeader)(HWAVEOUT, LPWAVEHDR, UINT);
    MMRESULT (WINAPI *waveOutWrite)(HWAVEOUT, LPWAVEHDR, UINT);
    MMRESULT (WINAPI *waveOutReset)(HWAVEOUT);
    UINT (WINAPI *midiOutGetNumDevs)(void);
    MMRESULT (WINAPI *midiOutGetDevCapsA)(UINT_PTR, LPMIDIOUTCAPSA, UINT);
    MMRESULT (WINAPI *midiStreamOpen)(LPHMIDISTRM, LPUINT, DWORD, DWORD_PTR, DWORD_PTR, DWORD);
    MMRESULT (WINAPI *midiStreamClose)(HMIDISTRM);
    MMRESULT (WINAPI *midiStreamProperty)(HMIDISTRM, LPBYTE, DWORD);
    MMRESULT (WINAPI *midiStreamOut)(HMIDISTRM, LPMIDIHDR, UINT);
    MMRESULT (WINAPI *midiStreamRestart)(HMIDISTRM);
    MMRESULT (WINAPI *midiStreamStop)(HMIDISTRM);
    MMRESULT (WINAPI *midiOutPrepareHeader)(HMIDIOUT, LPMIDIHDR, UINT);
    MMRESULT (WINAPI *midiOutUnprepareHeader)(HMIDIOUT, LPMIDIHDR, UINT);
    MMRESULT (WINAPI *midiOutShortMsg)(HMIDIOUT, DWORD);
    MMRESULT (WINAPI *midiOutReset)(HMIDIOUT);
    MMRESULT (WINAPI *midiOutSetVolume)(HMIDIOUT, DWORD);
} Winmm;

/* The table, loaded on first call; 0 when WINMM.DLL or a call is missing. */
const Winmm *winmm(void);
#endif

#endif
