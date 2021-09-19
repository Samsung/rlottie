#if defined(__ARM_NEON__)

#include "vdrawhelper.h"

// Replaces file `vector/vdrawhelper_neon.c`
// rlottie uses the pixelman NEON ASM to implement the `memfill32` function, however, Xcode doesn't support Nasm nor Yasm.
// Ideally, we would replace the ASM implementation with the C equivalent, which we can find by following the clue in a
// comment from `pixman-arm-neon-asm.S`:
/*
 This function takes a8r8g8b8 source buffer, r5g6b5 destination buffer and
 performs OVER compositing operation. Function fast_composite_over_8888_0565
 from pixman-fast-path.c does the same in C and can be used as a reference.
*/
// The file `pixman-fast-path.c` can be found here:  https://github.com/freedesktop/pixman/blob/master/pixman/pixman-fast-path.c

void memfill32(uint32_t *dest, uint32_t value, int length)
{
    for (int i = 0 ; i < length; i++) {
        *dest++ = value;
    }
}

void RenderFuncTable::neon()
{
    // This function doesn't need to do anything because we are using the C implementation for `memfill32`,
    // which doesn't require any extra configuration.
}

#endif
