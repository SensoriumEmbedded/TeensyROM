// SPDX-License-Identifier: MIT
//
// The handful of functions GCC emits calls to on its own, even for code that
// never mentions them: zeroing a struct becomes memset, copying one becomes
// memcpy. A freestanding module has no libc to supply them, so the module
// toolchain links this file into every extension.
//
// Every definition is weak. A module that wants a faster or instrumented
// version simply defines its own, and that one wins with no build changes.
//
// Compiled with -fno-tree-loop-distribute-patterns: without it GCC recognises
// the loop inside memset as a memset and rewrites it into a call to itself.
#include <stddef.h>

__attribute__((weak)) void *memset(void *destination, int value, size_t count) {
    unsigned char *out = (unsigned char *)destination;
    while (count--) *out++ = (unsigned char)value;
    return destination;
}

__attribute__((weak)) void *memcpy(void *destination, const void *source, size_t count) {
    unsigned char *out = (unsigned char *)destination;
    const unsigned char *in = (const unsigned char *)source;
    while (count--) *out++ = *in++;
    return destination;
}

__attribute__((weak)) void *memmove(void *destination, const void *source, size_t count) {
    unsigned char *out = (unsigned char *)destination;
    const unsigned char *in = (const unsigned char *)source;
    if (out == in || !count) return destination;
    if (out < in) { while (count--) *out++ = *in++; return destination; }
    out += count; in += count;
    while (count--) *--out = *--in;
    return destination;
}

__attribute__((weak)) int memcmp(const void *left, const void *right, size_t count) {
    const unsigned char *a = (const unsigned char *)left, *b = (const unsigned char *)right;
    while (count--) { if (*a != *b) return *a < *b ? -1 : 1; ++a; ++b; }
    return 0;
}

__attribute__((weak)) size_t strlen(const char *text) {
    const char *start = text;
    while (*text) ++text;
    return (size_t)(text - start);
}

__attribute__((weak)) int strcmp(const char *left, const char *right) {
    while (*left && *left == *right) { ++left; ++right; }
    return (int)(unsigned char)*left - (int)(unsigned char)*right;
}
