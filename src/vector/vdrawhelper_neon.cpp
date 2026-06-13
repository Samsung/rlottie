#if defined(__ARM_NEON__)

#include "vdrawhelper.h"

void memfill32(uint32_t *dest, uint32_t value, int length)
{
    memset(dest, value, length);
}

static void color_SourceOver(uint32_t *dest, int length,
                                      uint32_t color,
                                     uint32_t alpha)
{
    int ialpha, i;

    if (alpha != 255) color = BYTE_MUL(color, alpha);
    ialpha = 255 - vAlpha(color);
    for (i = 0; i < length; ++i) dest[i] = color + BYTE_MUL(dest[i], ialpha);
}

void RenderFuncTable::neon()
{
    updateColor(BlendMode::Src , color_SourceOver);
}
#endif
