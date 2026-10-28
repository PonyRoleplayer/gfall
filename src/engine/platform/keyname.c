/*
 * Key names for a bind screen (input_key_name, docs/input.md): a table
 * of our own over Windows' virtual key numbers, not GetKeyNameText, so
 * the names are the same on any layout and hostcheck reads them. The
 * punctuation keys are a US board's; another layout's are still named,
 * by their US mark.
 */

#include "engine/platform/input.h"

static const struct {
    unsigned char vk;
    const char *name;
} g_names[] = {
    { 0x01, "Left Button" }, { 0x02, "Right Button" }, { 0x04, "Middle Button" },
    { 0x08, "Backspace" }, { 0x09, "Tab" }, { 0x0D, "Enter" },
    { 0x10, "Shift" }, { 0x11, "Ctrl" }, { 0x12, "Alt" }, { 0x13, "Pause" },
    { 0x14, "Caps Lock" }, { 0x1B, "Escape" }, { 0x20, "Space" },
    { 0x21, "Page Up" }, { 0x22, "Page Down" }, { 0x23, "End" }, { 0x24, "Home" },
    { 0x25, "Left" }, { 0x26, "Up" }, { 0x27, "Right" }, { 0x28, "Down" },
    { 0x2D, "Insert" }, { 0x2E, "Delete" },
    { 0x6A, "Num *" }, { 0x6B, "Num +" }, { 0x6D, "Num -" }, { 0x6E, "Num ." }, { 0x6F, "Num /" },
    { 0x90, "Num Lock" }, { 0x91, "Scroll Lock" },
    { 0xBA, ";" }, { 0xBB, "=" }, { 0xBC, "," }, { 0xBD, "-" }, { 0xBE, "." }, { 0xBF, "/" },
    { 0xC0, "`" }, { 0xDB, "[" }, { 0xDC, "\\" }, { 0xDD, "]" }, { 0xDE, "'" }
};

/* Appends s at *n, keeping room for the terminator. */
static void put(char *dst, int cap, int *n, const char *s)
{
    while (*s && *n < cap - 1) {
        dst[(*n)++] = *s++;
    }
    dst[*n] = 0;
}

static void put_num(char *dst, int cap, int *n, int v)
{
    char digits[4];
    int k;

    k = 0;
    do {
        digits[k++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && k < 4);
    while (k > 0 && *n < cap - 1) {
        dst[(*n)++] = digits[--k];
    }
    dst[*n] = 0;
}

void input_key_name(int vk, char *dst, int cap)
{
    int n;
    int i;
    char one[2];

    if (!dst || cap < 1) {
        return;
    }
    n = 0;
    dst[0] = 0;
    /* Letters and digits are their own name. */
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        one[0] = (char)vk;
        one[1] = 0;
        put(dst, cap, &n, one);
        return;
    }
    if (vk >= 0x70 && vk <= 0x87) {
        put(dst, cap, &n, "F");
        put_num(dst, cap, &n, vk - 0x70 + 1);
        return;
    }
    if (vk >= 0x60 && vk <= 0x69) {
        put(dst, cap, &n, "Num ");
        put_num(dst, cap, &n, vk - 0x60);
        return;
    }
    for (i = 0; i < (int)(sizeof(g_names) / sizeof(g_names[0])); i++) {
        if (g_names[i].vk == vk) {
            put(dst, cap, &n, g_names[i].name);
            return;
        }
    }
    put(dst, cap, &n, "Key ");
    put_num(dst, cap, &n, vk & 0xff);
}
