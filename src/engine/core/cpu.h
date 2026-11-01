#ifndef ENG_CPU_H
#define ENG_CPU_H

/*
 * Runtime CPU capabilities shared by integer kernels, with no platform
 * or sound dependency. See docs/glide.md, "Runtime mip averaging".
 * Detection alone never executes MMX. Callers must test the capability
 * before entering an MMX batch and empty its shared x87 state with EMMS
 * before returning to C code that can use floating point.
 */

/* Cached CPUID MMX bit; false without CPUID, or on a non-x86 host. */
int cpu_has_mmx(void);

#endif
