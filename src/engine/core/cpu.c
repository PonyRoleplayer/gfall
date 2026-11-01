#include "engine/core/cpu.h"
#include "engine/core/df.h"

/* The engine runs on one thread; capability discovery happens once. */
static int g_mmx = -1;

#if defined(__i386__) || defined(__x86_64__)
static int detect_mmx(void)
{
    df_u32 eax;
    df_u32 ebx;
    df_u32 ecx;
    df_u32 edx;

#if defined(__i386__)
    df_u32 changed;
    df_u32 saved;

    /*
     * A 386 or early 486 cannot toggle EFLAGS.ID (bit 21). Test that
     * before CPUID, restoring the original flags even when it fails.
     * Early-clobber outputs must be distinct: saved survives the test.
     * x86-64 guarantees CPUID, so it needs no flags probe.
     */
    __asm__ __volatile__(
        "pushfl\n\t"
        "popl %0\n\t"
        "movl %0, %1\n\t"
        "xorl $0x200000, %0\n\t"
        "pushl %0\n\t"
        "popfl\n\t"
        "pushfl\n\t"
        "popl %0\n\t"
        "pushl %1\n\t"
        "popfl"
        : "=&r"(changed), "=&r"(saved)
        :
        : "cc");
    if (((changed ^ saved) & 0x200000u) == 0) {
        return 0;
    }
#endif
    /* A CPU with CPUID need not implement its feature leaf (1). */
    eax = 0;
    ecx = 0;
    __asm__ __volatile__("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));
    if (eax < 1) {
        return 0;
    }
    eax = 1;
    ecx = 0;
    __asm__ __volatile__("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));
    (void)ebx;
    (void)ecx;
    return (int)((edx >> 23) & 1u);
}
#endif

int cpu_has_mmx(void)
{
    if (g_mmx < 0) {
#if defined(__i386__) || defined(__x86_64__)
        g_mmx = detect_mmx();
#else
        g_mmx = 0;
#endif
    }
    return g_mmx;
}
