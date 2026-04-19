/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <algorithm>
#include <cmath>
#include <cstring>
#include "vdrawhelper.h"

/*
result = s
dest = s * ca + d * cia
*/
static void color_Source(uint32_t *dest, int length, uint32_t color,
                         uint32_t alpha)
{
    int ialpha, i;

    if (alpha == 255) {
        memfill32(dest, color, length);
    } else {
        ialpha = 255 - alpha;
        color = BYTE_MUL(color, alpha);
        for (i = 0; i < length; ++i)
            dest[i] = color + BYTE_MUL(dest[i], ialpha);
    }
}

/*
  r = s + d * sia
  dest = r * ca + d * cia
       =  (s + d * sia) * ca + d * cia
       = s * ca + d * (sia * ca + cia)
       = s * ca + d * (1 - sa*ca)
       = s' + d ( 1 - s'a)
*/
static void color_SourceOver(uint32_t *dest, int length, uint32_t color,
                             uint32_t alpha)
{
    int ialpha, i;

    if (alpha != 255) color = BYTE_MUL(color, alpha);
    ialpha = 255 - vAlpha(color);
    for (i = 0; i < length; ++i) dest[i] = color + BYTE_MUL(dest[i], ialpha);
}

/*
  result = d * sa
  dest = d * sa * ca + d * cia
       = d * (sa * ca + cia)
*/
static void color_DestinationIn(uint32_t *dest, int length, uint32_t color,
                                uint32_t alpha)
{
    uint32_t a = vAlpha(color);
    if (alpha != 255) {
        a = BYTE_MUL(a, alpha) + 255 - alpha;
    }
    for (int i = 0; i < length; ++i) {
        dest[i] = BYTE_MUL(dest[i], a);
    }
}

/*
  result = d * sia
  dest = d * sia * ca + d * cia
       = d * (sia * ca + cia)
*/
static void color_DestinationOut(uint32_t *dest, int length, uint32_t color,
                                 uint32_t alpha)
{
    uint32_t a = vAlpha(~color);
    if (alpha != 255) a = BYTE_MUL(a, alpha) + 255 - alpha;
    for (int i = 0; i < length; ++i) {
        dest[i] = BYTE_MUL(dest[i], a);
    }
}

static void src_Source(uint32_t *dest, int length, const uint32_t *src,
                       uint32_t alpha)
{
    if (alpha == 255) {
        memcpy(dest, src, size_t(length) * sizeof(uint32_t));
    } else {
        uint32_t ialpha = 255 - alpha;
        for (int i = 0; i < length; ++i) {
            dest[i] =
                interpolate_pixel(src[i], alpha, dest[i], ialpha);
        }
    }
}

/* s' = s * ca
 * d' = s' + d (1 - s'a)
 */
static void src_SourceOver(uint32_t *dest, int length, const uint32_t *src,
                           uint32_t alpha)
{
    uint32_t s, sia;

    if (alpha == 255) {
        for (int i = 0; i < length; ++i) {
            s = src[i];
            if (s >= 0xff000000)
                dest[i] = s;
            else if (s != 0) {
                sia = vAlpha(~s);
                dest[i] = s + BYTE_MUL(dest[i], sia);
            }
        }
    } else {
        /* source' = source * const_alpha
         * dest = source' + dest ( 1- source'a)
         */
        for (int i = 0; i < length; ++i) {
            s = BYTE_MUL(src[i], alpha);
            sia = vAlpha(~s);
            dest[i] = s + BYTE_MUL(dest[i], sia);
        }
    }
}

static void src_DestinationIn(uint32_t *dest, int length, const uint32_t *src,
                              uint32_t alpha)
{
    if (alpha == 255) {
        for (int i = 0; i < length; ++i) {
            dest[i] = BYTE_MUL(dest[i], vAlpha(src[i]));
        }
    } else {
        uint32_t cia = 255 - alpha;
        for (int i = 0; i < length; ++i) {
            uint32_t a = BYTE_MUL(vAlpha(src[i]), alpha) + cia;
            dest[i] = BYTE_MUL(dest[i], a);
        }
    }
}

static void src_DestinationOut(uint32_t *dest, int length, const uint32_t *src,
                               uint32_t alpha)
{
    if (alpha == 255) {
        for (int i = 0; i < length; ++i) {
            dest[i] = BYTE_MUL(dest[i], vAlpha(~src[i]));
        }
    } else {
        uint32_t cia = 255 - alpha;
        for (int i = 0; i < length; ++i) {
            uint32_t sia = BYTE_MUL(vAlpha(~src[i]), alpha) + cia;
            dest[i] = BYTE_MUL(dest[i], sia);
        }
    }
}

static inline uint32_t multiply_pixel(uint32_t s, uint32_t d)
{
    const int sa = vAlpha(s);
    const int da = vAlpha(d);
    const int a = sa + da - ((sa * da) / 255);

    const int r = (vRed(s) * (255 - da) + vRed(d) * (255 - sa) +
                   vRed(s) * vRed(d)) / 255;
    const int g = (vGreen(s) * (255 - da) + vGreen(d) * (255 - sa) +
                   vGreen(s) * vGreen(d)) / 255;
    const int b = (vBlue(s) * (255 - da) + vBlue(d) * (255 - sa) +
                   vBlue(s) * vBlue(d)) / 255;

    return uint32_t((a << 24) | (r << 16) | (g << 8) | b);
}

static inline uint32_t screen_pixel(uint32_t s, uint32_t d)
{
    const int sa = vAlpha(s);
    const int da = vAlpha(d);
    const int a = sa + da - ((sa * da) / 255);

    const int r = vRed(s) + vRed(d) - ((vRed(s) * vRed(d)) / 255);
    const int g = vGreen(s) + vGreen(d) - ((vGreen(s) * vGreen(d)) / 255);
    const int b = vBlue(s) + vBlue(d) - ((vBlue(s) * vBlue(d)) / 255);

    return uint32_t((a << 24) | (r << 16) | (g << 8) | b);
}

static inline int overlay_channel(int sc, int sa, int dc, int da)
{
    if (2 * dc <= da) {
        return (sc * (255 - da) + dc * (255 - sa) + 2 * sc * dc) / 255;
    }

    return (sc * (255 - da) + dc * (255 - sa) + sa * da -
            (2 * (sa - sc) * (da - dc))) / 255;
}

static inline uint32_t overlay_pixel(uint32_t s, uint32_t d)
{
    const int sa = vAlpha(s);
    const int da = vAlpha(d);
    const int a = sa + da - ((sa * da) / 255);

    const int r = overlay_channel(vRed(s), sa, vRed(d), da);
    const int g = overlay_channel(vGreen(s), sa, vGreen(d), da);
    const int b = overlay_channel(vBlue(s), sa, vBlue(d), da);

    return uint32_t((a << 24) | (r << 16) | (g << 8) | b);
}

static inline int darken_channel(int sc, int sa, int dc, int da)
{
    const int blend = (sc * da < dc * sa) ? (sc * da) : (dc * sa);
    return (sc * (255 - da) + dc * (255 - sa) + blend) / 255;
}

static inline uint32_t darken_pixel(uint32_t s, uint32_t d)
{
    const int sa = vAlpha(s);
    const int da = vAlpha(d);
    const int a = sa + da - ((sa * da) / 255);

    const int r = darken_channel(vRed(s), sa, vRed(d), da);
    const int g = darken_channel(vGreen(s), sa, vGreen(d), da);
    const int b = darken_channel(vBlue(s), sa, vBlue(d), da);

    return uint32_t((a << 24) | (r << 16) | (g << 8) | b);
}

static inline int lighten_channel(int sc, int sa, int dc, int da)
{
    const int blend = (sc * da > dc * sa) ? (sc * da) : (dc * sa);
    return (sc * (255 - da) + dc * (255 - sa) + blend) / 255;
}

static inline uint32_t lighten_pixel(uint32_t s, uint32_t d)
{
    const int sa = vAlpha(s);
    const int da = vAlpha(d);
    const int a = sa + da - ((sa * da) / 255);

    const int r = lighten_channel(vRed(s), sa, vRed(d), da);
    const int g = lighten_channel(vGreen(s), sa, vGreen(d), da);
    const int b = lighten_channel(vBlue(s), sa, vBlue(d), da);

    return uint32_t((a << 24) | (r << 16) | (g << 8) | b);
}

static inline int difference_channel(int sc, int sa, int dc, int da)
{
    int blend = sc * da - dc * sa;
    if (blend < 0) blend = -blend;
    return (sc * (255 - da) + dc * (255 - sa) + blend) / 255;
}

static inline uint32_t difference_pixel(uint32_t s, uint32_t d)
{
    const int sa = vAlpha(s);
    const int da = vAlpha(d);
    const int a = sa + da - ((sa * da) / 255);

    const int r = difference_channel(vRed(s), sa, vRed(d), da);
    const int g = difference_channel(vGreen(s), sa, vGreen(d), da);
    const int b = difference_channel(vBlue(s), sa, vBlue(d), da);

    return uint32_t((a << 24) | (r << 16) | (g << 8) | b);
}

static inline uint32_t exclusion_pixel(uint32_t s, uint32_t d)
{
    const int sa = vAlpha(s);
    const int da = vAlpha(d);
    const int a = sa + da - ((sa * da) / 255);

    const int r = vRed(s) + vRed(d) - ((2 * vRed(s) * vRed(d)) / 255);
    const int g = vGreen(s) + vGreen(d) - ((2 * vGreen(s) * vGreen(d)) / 255);
    const int b = vBlue(s) + vBlue(d) - ((2 * vBlue(s) * vBlue(d)) / 255);

    return uint32_t((a << 24) | (r << 16) | (g << 8) | b);
}

struct StraightColor
{
    float r;
    float g;
    float b;
};

static inline float clamp01(float value)
{
    return vMin(1.0f, vMax(0.0f, value));
}

static inline StraightColor to_straight_color(uint32_t pixel, int alpha)
{
    if (alpha == 0) return {0.0f, 0.0f, 0.0f};
    const float invAlpha = 1.0f / float(alpha);
    return {float(vRed(pixel)) * invAlpha, float(vGreen(pixel)) * invAlpha,
            float(vBlue(pixel)) * invAlpha};
}

static inline float color_lum(const StraightColor &color)
{
    return 0.3f * color.r + 0.59f * color.g + 0.11f * color.b;
}

static inline float color_sat(const StraightColor &color)
{
    return vMax(color.r, vMax(color.g, color.b)) -
           vMin(color.r, vMin(color.g, color.b));
}

static inline StraightColor clip_color(StraightColor color)
{
    const float lum = color_lum(color);
    const float minValue = vMin(color.r, vMin(color.g, color.b));
    const float maxValue = vMax(color.r, vMax(color.g, color.b));

    if (minValue < 0.0f && !vIsZero(lum - minValue)) {
        color.r = lum + ((color.r - lum) * lum) / (lum - minValue);
        color.g = lum + ((color.g - lum) * lum) / (lum - minValue);
        color.b = lum + ((color.b - lum) * lum) / (lum - minValue);
    }

    if (maxValue > 1.0f && !vIsZero(maxValue - lum)) {
        color.r =
            lum + ((color.r - lum) * (1.0f - lum)) / (maxValue - lum);
        color.g =
            lum + ((color.g - lum) * (1.0f - lum)) / (maxValue - lum);
        color.b =
            lum + ((color.b - lum) * (1.0f - lum)) / (maxValue - lum);
    }

    color.r = clamp01(color.r);
    color.g = clamp01(color.g);
    color.b = clamp01(color.b);
    return color;
}

static inline StraightColor set_lum(StraightColor color, float lum)
{
    const float delta = lum - color_lum(color);
    color.r += delta;
    color.g += delta;
    color.b += delta;
    return clip_color(color);
}

static inline StraightColor set_sat(StraightColor color, float sat)
{
    float *channels[3] = {&color.r, &color.g, &color.b};
    int minIndex = 0;
    int midIndex = 1;
    int maxIndex = 2;

    if (*channels[minIndex] > *channels[midIndex]) {
        std::swap(minIndex, midIndex);
    }
    if (*channels[midIndex] > *channels[maxIndex]) {
        std::swap(midIndex, maxIndex);
    }
    if (*channels[minIndex] > *channels[midIndex]) {
        std::swap(minIndex, midIndex);
    }

    float &minValue = *channels[minIndex];
    float &midValue = *channels[midIndex];
    float &maxValue = *channels[maxIndex];

    if (!vIsZero(maxValue - minValue)) {
        midValue = ((midValue - minValue) * sat) / (maxValue - minValue);
        maxValue = sat;
    } else {
        midValue = 0.0f;
        maxValue = 0.0f;
    }
    minValue = 0.0f;
    return color;
}

template <typename BlendColorFunc>
static inline uint32_t blend_nonseparable_pixel(uint32_t s, uint32_t d,
                                                BlendColorFunc blendColor)
{
    const int sa = vAlpha(s);
    const int da = vAlpha(d);
    const int a = sa + da - ((sa * da) / 255);

    if (sa == 0) return d;
    if (da == 0) return s;

    const StraightColor src = to_straight_color(s, sa);
    const StraightColor dst = to_straight_color(d, da);
    const StraightColor blended = blendColor(src, dst);

    auto blendChannel = [&](int sc, int dc, float value) -> int {
        float result = float(sc * (255 - da) + dc * (255 - sa));
        result += float(sa) * float(da) * clamp01(value);
        result /= 255.0f;
        result = vMin(255.0f, vMax(0.0f, result));
        return int(result + 0.5f);
    };

    const int r = blendChannel(vRed(s), vRed(d), blended.r);
    const int g = blendChannel(vGreen(s), vGreen(d), blended.g);
    const int b = blendChannel(vBlue(s), vBlue(d), blended.b);

    return uint32_t((a << 24) | (r << 16) | (g << 8) | b);
}

static inline uint32_t hue_pixel(uint32_t s, uint32_t d)
{
    return blend_nonseparable_pixel(
        s, d, [](const StraightColor &src, const StraightColor &dst) {
            return set_lum(set_sat(src, color_sat(dst)), color_lum(dst));
        });
}

static inline uint32_t saturation_pixel(uint32_t s, uint32_t d)
{
    return blend_nonseparable_pixel(
        s, d, [](const StraightColor &src, const StraightColor &dst) {
            return set_lum(set_sat(dst, color_sat(src)), color_lum(dst));
        });
}

static inline uint32_t color_pixel(uint32_t s, uint32_t d)
{
    return blend_nonseparable_pixel(
        s, d, [](const StraightColor &src, const StraightColor &dst) {
            return set_lum(src, color_lum(dst));
        });
}

static inline uint32_t luminosity_pixel(uint32_t s, uint32_t d)
{
    return blend_nonseparable_pixel(
        s, d, [](const StraightColor &src, const StraightColor &dst) {
            return set_lum(dst, color_lum(src));
        });
}

template <typename BlendFunc>
static inline int blend_premul_channel(int sc, int sa, int dc, int da,
                                       BlendFunc blend)
{
    if (sa == 0) return dc;
    if (da == 0) return sc;

    float s = float(sc) / float(sa);
    float d = float(dc) / float(da);
    float value = blend(s, d);
    value = vMin(1.0f, vMax(0.0f, value));

    float result = float(sc * (255 - da) + dc * (255 - sa));
    result += (float(sa) * float(da) * value);
    result /= 255.0f;
    result = vMin(255.0f, vMax(0.0f, result));
    return int(result + 0.5f);
}

static inline uint32_t color_dodge_pixel(uint32_t s, uint32_t d)
{
    const int sa = vAlpha(s);
    const int da = vAlpha(d);
    const int a = sa + da - ((sa * da) / 255);

    auto blend = [](float src, float dst) -> float {
        if (vIsZero(dst)) return 0.0f;
        if (src >= 1.0f) return 1.0f;
        return vMin(1.0f, dst / (1.0f - src));
    };

    const int r = blend_premul_channel(vRed(s), sa, vRed(d), da, blend);
    const int g = blend_premul_channel(vGreen(s), sa, vGreen(d), da, blend);
    const int b = blend_premul_channel(vBlue(s), sa, vBlue(d), da, blend);

    return uint32_t((a << 24) | (r << 16) | (g << 8) | b);
}

static inline uint32_t color_burn_pixel(uint32_t s, uint32_t d)
{
    const int sa = vAlpha(s);
    const int da = vAlpha(d);
    const int a = sa + da - ((sa * da) / 255);

    auto blend = [](float src, float dst) -> float {
        if (dst >= 1.0f) return 1.0f;
        if (vIsZero(src)) return 0.0f;
        return 1.0f - vMin(1.0f, (1.0f - dst) / src);
    };

    const int r = blend_premul_channel(vRed(s), sa, vRed(d), da, blend);
    const int g = blend_premul_channel(vGreen(s), sa, vGreen(d), da, blend);
    const int b = blend_premul_channel(vBlue(s), sa, vBlue(d), da, blend);

    return uint32_t((a << 24) | (r << 16) | (g << 8) | b);
}

static inline uint32_t hard_light_pixel(uint32_t s, uint32_t d)
{
    const int sa = vAlpha(s);
    const int da = vAlpha(d);
    const int a = sa + da - ((sa * da) / 255);

    const int r = overlay_channel(vRed(d), da, vRed(s), sa);
    const int g = overlay_channel(vGreen(d), da, vGreen(s), sa);
    const int b = overlay_channel(vBlue(d), da, vBlue(s), sa);

    return uint32_t((a << 24) | (r << 16) | (g << 8) | b);
}

static inline float soft_light_curve(float value)
{
    if (value <= 0.25f) {
        return ((16.0f * value - 12.0f) * value + 4.0f) * value;
    }
    return std::sqrt(value);
}

static inline uint32_t soft_light_pixel(uint32_t s, uint32_t d)
{
    const int sa = vAlpha(s);
    const int da = vAlpha(d);
    const int a = sa + da - ((sa * da) / 255);

    auto blend = [](float src, float dst) -> float {
        if (src <= 0.5f) {
            return dst - (1.0f - 2.0f * src) * dst * (1.0f - dst);
        }
        return dst + (2.0f * src - 1.0f) * (soft_light_curve(dst) - dst);
    };

    const int r = blend_premul_channel(vRed(s), sa, vRed(d), da, blend);
    const int g = blend_premul_channel(vGreen(s), sa, vGreen(d), da, blend);
    const int b = blend_premul_channel(vBlue(s), sa, vBlue(d), da, blend);

    return uint32_t((a << 24) | (r << 16) | (g << 8) | b);
}

template <uint32_t (*BlendPixel)(uint32_t, uint32_t)>
static void color_Blend(uint32_t *dest, int length, uint32_t color,
                        uint32_t alpha)
{
    if (alpha != 255) color = BYTE_MUL(color, alpha);
    for (int i = 0; i < length; ++i) {
        dest[i] = BlendPixel(color, dest[i]);
    }
}

template <uint32_t (*BlendPixel)(uint32_t, uint32_t)>
static void src_Blend(uint32_t *dest, int length, const uint32_t *src,
                      uint32_t alpha)
{
    if (alpha == 255) {
        for (int i = 0; i < length; ++i) {
            dest[i] = BlendPixel(src[i], dest[i]);
        }
    } else {
        for (int i = 0; i < length; ++i) {
            dest[i] = BlendPixel(BYTE_MUL(src[i], alpha), dest[i]);
        }
    }
}

static void color_Multiply(uint32_t *dest, int length, uint32_t color,
                           uint32_t alpha)
{
    color_Blend<multiply_pixel>(dest, length, color, alpha);
}

static void src_Multiply(uint32_t *dest, int length, const uint32_t *src,
                         uint32_t alpha)
{
    src_Blend<multiply_pixel>(dest, length, src, alpha);
}

static void color_Screen(uint32_t *dest, int length, uint32_t color,
                         uint32_t alpha)
{
    color_Blend<screen_pixel>(dest, length, color, alpha);
}

static void src_Screen(uint32_t *dest, int length, const uint32_t *src,
                       uint32_t alpha)
{
    src_Blend<screen_pixel>(dest, length, src, alpha);
}

static void color_Overlay(uint32_t *dest, int length, uint32_t color,
                          uint32_t alpha)
{
    color_Blend<overlay_pixel>(dest, length, color, alpha);
}

static void src_Overlay(uint32_t *dest, int length, const uint32_t *src,
                        uint32_t alpha)
{
    src_Blend<overlay_pixel>(dest, length, src, alpha);
}

static void color_Darken(uint32_t *dest, int length, uint32_t color,
                         uint32_t alpha)
{
    color_Blend<darken_pixel>(dest, length, color, alpha);
}

static void src_Darken(uint32_t *dest, int length, const uint32_t *src,
                       uint32_t alpha)
{
    src_Blend<darken_pixel>(dest, length, src, alpha);
}

static void color_Lighten(uint32_t *dest, int length, uint32_t color,
                          uint32_t alpha)
{
    color_Blend<lighten_pixel>(dest, length, color, alpha);
}

static void src_Lighten(uint32_t *dest, int length, const uint32_t *src,
                        uint32_t alpha)
{
    src_Blend<lighten_pixel>(dest, length, src, alpha);
}

static void color_ColorDodge(uint32_t *dest, int length, uint32_t color,
                             uint32_t alpha)
{
    color_Blend<color_dodge_pixel>(dest, length, color, alpha);
}

static void src_ColorDodge(uint32_t *dest, int length, const uint32_t *src,
                           uint32_t alpha)
{
    src_Blend<color_dodge_pixel>(dest, length, src, alpha);
}

static void color_ColorBurn(uint32_t *dest, int length, uint32_t color,
                            uint32_t alpha)
{
    color_Blend<color_burn_pixel>(dest, length, color, alpha);
}

static void src_ColorBurn(uint32_t *dest, int length, const uint32_t *src,
                          uint32_t alpha)
{
    src_Blend<color_burn_pixel>(dest, length, src, alpha);
}

static void color_HardLight(uint32_t *dest, int length, uint32_t color,
                            uint32_t alpha)
{
    color_Blend<hard_light_pixel>(dest, length, color, alpha);
}

static void src_HardLight(uint32_t *dest, int length, const uint32_t *src,
                          uint32_t alpha)
{
    src_Blend<hard_light_pixel>(dest, length, src, alpha);
}

static void color_SoftLight(uint32_t *dest, int length, uint32_t color,
                            uint32_t alpha)
{
    color_Blend<soft_light_pixel>(dest, length, color, alpha);
}

static void src_SoftLight(uint32_t *dest, int length, const uint32_t *src,
                          uint32_t alpha)
{
    src_Blend<soft_light_pixel>(dest, length, src, alpha);
}

static void color_Difference(uint32_t *dest, int length, uint32_t color,
                             uint32_t alpha)
{
    color_Blend<difference_pixel>(dest, length, color, alpha);
}

static void src_Difference(uint32_t *dest, int length, const uint32_t *src,
                           uint32_t alpha)
{
    src_Blend<difference_pixel>(dest, length, src, alpha);
}

static void color_Exclusion(uint32_t *dest, int length, uint32_t color,
                            uint32_t alpha)
{
    color_Blend<exclusion_pixel>(dest, length, color, alpha);
}

static void src_Exclusion(uint32_t *dest, int length, const uint32_t *src,
                          uint32_t alpha)
{
    src_Blend<exclusion_pixel>(dest, length, src, alpha);
}

static void color_Hue(uint32_t *dest, int length, uint32_t color,
                      uint32_t alpha)
{
    color_Blend<hue_pixel>(dest, length, color, alpha);
}

static void src_Hue(uint32_t *dest, int length, const uint32_t *src,
                    uint32_t alpha)
{
    src_Blend<hue_pixel>(dest, length, src, alpha);
}

static void color_Saturation(uint32_t *dest, int length, uint32_t color,
                             uint32_t alpha)
{
    color_Blend<saturation_pixel>(dest, length, color, alpha);
}

static void src_Saturation(uint32_t *dest, int length, const uint32_t *src,
                           uint32_t alpha)
{
    src_Blend<saturation_pixel>(dest, length, src, alpha);
}

static void color_Color(uint32_t *dest, int length, uint32_t color,
                        uint32_t alpha)
{
    color_Blend<color_pixel>(dest, length, color, alpha);
}

static void src_Color(uint32_t *dest, int length, const uint32_t *src,
                      uint32_t alpha)
{
    src_Blend<color_pixel>(dest, length, src, alpha);
}

static void color_Luminosity(uint32_t *dest, int length, uint32_t color,
                             uint32_t alpha)
{
    color_Blend<luminosity_pixel>(dest, length, color, alpha);
}

static void src_Luminosity(uint32_t *dest, int length, const uint32_t *src,
                           uint32_t alpha)
{
    src_Blend<luminosity_pixel>(dest, length, src, alpha);
}

RenderFuncTable::RenderFuncTable()
{
    updateColor(BlendMode::Src, color_Source);
    updateColor(BlendMode::SrcOver, color_SourceOver);
    updateColor(BlendMode::Multiply, color_Multiply);
    updateColor(BlendMode::Screen, color_Screen);
    updateColor(BlendMode::Overlay, color_Overlay);
    updateColor(BlendMode::Darken, color_Darken);
    updateColor(BlendMode::Lighten, color_Lighten);
    updateColor(BlendMode::ColorDodge, color_ColorDodge);
    updateColor(BlendMode::ColorBurn, color_ColorBurn);
    updateColor(BlendMode::HardLight, color_HardLight);
    updateColor(BlendMode::SoftLight, color_SoftLight);
    updateColor(BlendMode::Difference, color_Difference);
    updateColor(BlendMode::Exclusion, color_Exclusion);
    updateColor(BlendMode::Hue, color_Hue);
    updateColor(BlendMode::Saturation, color_Saturation);
    updateColor(BlendMode::Color, color_Color);
    updateColor(BlendMode::Luminosity, color_Luminosity);
    updateColor(BlendMode::DestIn, color_DestinationIn);
    updateColor(BlendMode::DestOut, color_DestinationOut);

    updateSrc(BlendMode::Src, src_Source);
    updateSrc(BlendMode::SrcOver, src_SourceOver);
    updateSrc(BlendMode::Multiply, src_Multiply);
    updateSrc(BlendMode::Screen, src_Screen);
    updateSrc(BlendMode::Overlay, src_Overlay);
    updateSrc(BlendMode::Darken, src_Darken);
    updateSrc(BlendMode::Lighten, src_Lighten);
    updateSrc(BlendMode::ColorDodge, src_ColorDodge);
    updateSrc(BlendMode::ColorBurn, src_ColorBurn);
    updateSrc(BlendMode::HardLight, src_HardLight);
    updateSrc(BlendMode::SoftLight, src_SoftLight);
    updateSrc(BlendMode::Difference, src_Difference);
    updateSrc(BlendMode::Exclusion, src_Exclusion);
    updateSrc(BlendMode::Hue, src_Hue);
    updateSrc(BlendMode::Saturation, src_Saturation);
    updateSrc(BlendMode::Color, src_Color);
    updateSrc(BlendMode::Luminosity, src_Luminosity);
    updateSrc(BlendMode::DestIn, src_DestinationIn);
    updateSrc(BlendMode::DestOut, src_DestinationOut);

#if defined(__ARM_NEON__) && defined(LOTTIE_PIXMAN_ARM_NEON)
    neon();
#endif
#if defined(__SSE2__)
    sse();
#endif
}
