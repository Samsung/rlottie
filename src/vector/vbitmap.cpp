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

#include "vbitmap.h"
#include <algorithm>
#include <string>
#include <memory>
#include <vector>
#include "vdrawhelper.h"
#include "vglobal.h"

V_BEGIN_NAMESPACE

namespace
{

inline float cross2d(const VPointF &lhs, const VPointF &rhs)
{
    return lhs.x() * rhs.y() - lhs.y() * rhs.x();
}

inline VPointF pointSub(const VPointF &lhs, const VPointF &rhs)
{
    return {lhs.x() - rhs.x(), lhs.y() - rhs.y()};
}

inline VPointF pointAdd(const VPointF &lhs, const VPointF &rhs)
{
    return {lhs.x() + rhs.x(), lhs.y() + rhs.y()};
}

inline VPointF pointScale(const VPointF &point, float scalar)
{
    return {point.x() * scalar, point.y() * scalar};
}

inline float lengthSquared(const VPointF &point)
{
    return point.x() * point.x() + point.y() * point.y();
}

inline float clampUnit(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

struct FourColorGradientQuad {
    VPointF tl;
    VPointF tr;
    VPointF bl;
    VPointF br;
    VPointF affineU;
    VPointF affineV;
    VPointF bilinearU;
    VPointF bilinearV;
    VPointF bilinearUV;
    bool    preferAffine{true};

    explicit FourColorGradientQuad(const VPointF points[4])
        : tl(points[2]),
          tr(points[3]),
          bl(points[0]),
          br(points[1]),
          affineU(pointScale(pointAdd(pointSub(tr, tl), pointSub(br, bl)),
                             0.5f)),
          affineV(pointScale(pointAdd(pointSub(bl, tl), pointSub(br, tr)),
                             0.5f)),
          bilinearU(pointSub(tr, tl)),
          bilinearV(pointSub(bl, tl)),
          bilinearUV(pointAdd(pointSub(pointSub(tl, tr), bl), br))
    {
        const auto affineScale =
            std::max(lengthSquared(affineU) + lengthSquared(affineV), 1.0f);
        preferAffine = lengthSquared(bilinearUV) <= affineScale * 0.0f;
    }

    bool affineCoords(const VPointF &point, float &u, float &v) const
    {
        const auto q = pointSub(point, tl);
        const auto det = cross2d(affineU, affineV);
        if (std::fabs(det) < 1e-5f) return false;

        u = clampUnit(cross2d(q, affineV) / det);
        v = clampUnit(cross2d(affineU, q) / det);
        return true;
    }

    bool bilinearCoords(const VPointF &point, float &u, float &v) const
    {
        if (preferAffine) return affineCoords(point, u, v);

        const auto q = pointSub(point, tl);
        const auto a = cross2d(bilinearV, bilinearUV);
        const auto b = cross2d(bilinearUV, q) - cross2d(bilinearU, bilinearV);
        const auto c = cross2d(bilinearU, q);

        if (std::fabs(a) < 1e-5f) {
            if (std::fabs(b) < 1e-5f) return affineCoords(point, u, v);
            v = -c / b;
        } else {
            const auto disc = std::max(0.0f, b * b - 4.0f * a * c);
            const auto sqrtDisc = std::sqrt(disc);
            const auto v0 = (-b - sqrtDisc) / (2.0f * a);
            const auto v1 = (-b + sqrtDisc) / (2.0f * a);

            const bool v0Valid = v0 >= -1e-3f && v0 <= 1.001f;
            const bool v1Valid = v1 >= -1e-3f && v1 <= 1.001f;
            if (v0Valid && !v1Valid) {
                v = v0;
            } else if (!v0Valid && v1Valid) {
                v = v1;
            } else {
                v = std::fabs(v0 - 0.5f) < std::fabs(v1 - 0.5f) ? v0 : v1;
            }
        }

        const auto denomX = bilinearU.x() + bilinearUV.x() * v;
        const auto denomY = bilinearU.y() + bilinearUV.y() * v;
        if (std::fabs(denomX) > std::fabs(denomY)) {
            if (std::fabs(denomX) < 1e-5f) return affineCoords(point, u, v);
            u = (q.x() - bilinearV.x() * v) / denomX;
        } else {
            if (std::fabs(denomY) < 1e-5f) return affineCoords(point, u, v);
            u = (q.y() - bilinearV.y() * v) / denomY;
        }

        u = clampUnit(u);
        v = clampUnit(v);
        return true;
    }
};

inline int bilinearChannel(const VColor colors[4], float u, float v,
                           uint8_t (VColor::*getter)() const)
{
    const auto top = (colors[2].*getter)() +
                     ((colors[3].*getter)() - (colors[2].*getter)()) * u;
    const auto bottom = (colors[0].*getter)() +
                        ((colors[1].*getter)() - (colors[0].*getter)()) * u;
    return int(std::lround(top + (bottom - top) * v));
}

inline uint32_t premulPixel(uint8_t red, uint8_t green, uint8_t blue,
                            uint8_t alpha)
{
    const auto premulRed = (uint32_t(red) * alpha + 127) / 255;
    const auto premulGreen = (uint32_t(green) * alpha + 127) / 255;
    const auto premulBlue = (uint32_t(blue) * alpha + 127) / 255;
    return (uint32_t(alpha) << 24) | (premulRed << 16) | (premulGreen << 8) |
           premulBlue;
}

inline uint32_t srcOverPixel(uint32_t src, uint32_t dst)
{
    const auto srcAlpha = uint32_t(vAlpha(src));
    if (srcAlpha == 255) return src;
    if (srcAlpha == 0) return dst;

    const auto invAlpha = 255 - srcAlpha;
    const auto outAlpha = srcAlpha + (uint32_t(vAlpha(dst)) * invAlpha + 127) / 255;
    const auto outRed = uint32_t(vRed(src)) +
                        (uint32_t(vRed(dst)) * invAlpha + 127) / 255;
    const auto outGreen = uint32_t(vGreen(src)) +
                          (uint32_t(vGreen(dst)) * invAlpha + 127) / 255;
    const auto outBlue = uint32_t(vBlue(src)) +
                         (uint32_t(vBlue(dst)) * invAlpha + 127) / 255;
    return (outAlpha << 24) | (outRed << 16) | (outGreen << 8) | outBlue;
}

}  // namespace

void VBitmap::Impl::reset(size_t width, size_t height, VBitmap::Format format)
{
    mRoData = nullptr;
    mWidth = uint32_t(width);
    mHeight = uint32_t(height);
    mFormat = format;

    mDepth = depth(format);
    mStride = ((mWidth * mDepth + 31) >> 5)
                  << 2;  // bytes per scanline (must be multiple of 4)
    mOwnData = std::make_unique<uint8_t[]>(mStride * mHeight);
}

void VBitmap::Impl::reset(uint8_t *data, size_t width, size_t height,
                          size_t bytesPerLine, VBitmap::Format format)
{
    mRoData = data;
    mWidth = uint32_t(width);
    mHeight = uint32_t(height);
    mStride = uint32_t(bytesPerLine);
    mFormat = format;
    mDepth = depth(format);
    mOwnData = nullptr;
}

uint8_t VBitmap::Impl::depth(VBitmap::Format format)
{
    uint8_t depth = 1;
    switch (format) {
    case VBitmap::Format::Alpha8:
        depth = 8;
        break;
    case VBitmap::Format::ARGB32:
    case VBitmap::Format::ARGB32_Premultiplied:
        depth = 32;
        break;
    default:
        break;
    }
    return depth;
}

void VBitmap::Impl::fill(uint32_t pixel)
{
    auto dataPtr = data();
    if (!dataPtr) return;

    if (mFormat == VBitmap::Format::Alpha8) {
        std::memset(dataPtr, uint8_t(pixel & 0xff), size_t(mStride) * mHeight);
        return;
    }

    if (mFormat == VBitmap::Format::ARGB32 ||
        mFormat == VBitmap::Format::ARGB32_Premultiplied) {
        for (uint32_t y = 0; y < mHeight; ++y) {
            auto *row = reinterpret_cast<uint32_t *>(dataPtr + mStride * y);
            std::fill(row, row + mWidth, pixel);
        }
    }
}

void VBitmap::Impl::updateLuma()
{
    if (mFormat != VBitmap::Format::ARGB32_Premultiplied) return;
    auto dataPtr = data();
    for (uint32_t col = 0; col < mHeight; col++) {
        uint32_t *pixel = (uint32_t *)(dataPtr + mStride * col);
        for (uint32_t row = 0; row < mWidth; row++) {
            int alpha = vAlpha(*pixel);
            if (alpha == 0) {
                pixel++;
                continue;
            }

            int red = vRed(*pixel);
            int green = vGreen(*pixel);
            int blue = vBlue(*pixel);

            if (alpha != 255) {
                // un multiply
                red = (red * 255) / alpha;
                green = (green * 255) / alpha;
                blue = (blue * 255) / alpha;
            }
            int luminosity = int(0.299f * red + 0.587f * green + 0.114f * blue);
            *pixel = luminosity << 24;
            pixel++;
        }
    }
}

void VBitmap::Impl::updateLuma(const VRect &region)
{
    if (mFormat != VBitmap::Format::ARGB32_Premultiplied) return;

    auto clipped = region & rect();
    if (clipped.empty()) return;

    auto dataPtr = data();
    for (int y = clipped.top(); y < clipped.bottom(); ++y) {
        uint32_t *pixel =
            reinterpret_cast<uint32_t *>(dataPtr + mStride * y) + clipped.left();
        for (int x = clipped.left(); x < clipped.right(); ++x) {
            int alpha = vAlpha(*pixel);
            if (alpha == 0) {
                pixel++;
                continue;
            }

            int red = vRed(*pixel);
            int green = vGreen(*pixel);
            int blue = vBlue(*pixel);

            if (alpha != 255) {
                red = (red * 255) / alpha;
                green = (green * 255) / alpha;
                blue = (blue * 255) / alpha;
            }
            int luminosity =
                int(0.299f * red + 0.587f * green + 0.114f * blue);
            *pixel = luminosity << 24;
            pixel++;
        }
    }
}

void VBitmap::Impl::applyFill(const VRect &region, uint8_t red, uint8_t green,
                              uint8_t blue, float amount)
{
    if (mFormat != VBitmap::Format::ARGB32_Premultiplied) return;

    auto clipped = region & rect();
    if (clipped.empty()) return;

    amount = std::max(0.0f, std::min(1.0f, amount));
    if (vIsZero(amount)) return;

    auto dataPtr = data();
    for (int y = clipped.top(); y < clipped.bottom(); ++y) {
        uint32_t *pixel =
            reinterpret_cast<uint32_t *>(dataPtr + mStride * y) + clipped.left();
        for (int x = clipped.left(); x < clipped.right(); ++x) {
            auto alpha = int(vAlpha(*pixel));
            if (alpha == 0) {
                pixel++;
                continue;
            }

            auto srcRed = int(vRed(*pixel));
            auto srcGreen = int(vGreen(*pixel));
            auto srcBlue = int(vBlue(*pixel));

            if (alpha != 255) {
                srcRed = (srcRed * 255) / alpha;
                srcGreen = (srcGreen * 255) / alpha;
                srcBlue = (srcBlue * 255) / alpha;
            }

            auto outRed =
                int(std::lround(srcRed + (int(red) - srcRed) * amount));
            auto outGreen =
                int(std::lround(srcGreen + (int(green) - srcGreen) * amount));
            auto outBlue =
                int(std::lround(srcBlue + (int(blue) - srcBlue) * amount));

            outRed = (outRed * alpha + 127) / 255;
            outGreen = (outGreen * alpha + 127) / 255;
            outBlue = (outBlue * alpha + 127) / 255;

            *pixel = (uint32_t(alpha) << 24) | (uint32_t(outRed) << 16) |
                     (uint32_t(outGreen) << 8) | uint32_t(outBlue);
            pixel++;
        }
    }
}

void VBitmap::Impl::applyTint(const VRect &region, uint8_t blackRed,
                              uint8_t blackGreen, uint8_t blackBlue,
                              uint8_t whiteRed, uint8_t whiteGreen,
                              uint8_t whiteBlue, float amount)
{
    if (mFormat != VBitmap::Format::ARGB32_Premultiplied) return;

    auto clipped = region & rect();
    if (clipped.empty()) return;

    amount = std::max(0.0f, std::min(1.0f, amount));
    if (vIsZero(amount)) return;

    auto dataPtr = data();
    for (int y = clipped.top(); y < clipped.bottom(); ++y) {
        uint32_t *pixel =
            reinterpret_cast<uint32_t *>(dataPtr + mStride * y) + clipped.left();
        for (int x = clipped.left(); x < clipped.right(); ++x) {
            auto alpha = int(vAlpha(*pixel));
            if (alpha == 0) {
                pixel++;
                continue;
            }

            auto srcRed = int(vRed(*pixel));
            auto srcGreen = int(vGreen(*pixel));
            auto srcBlue = int(vBlue(*pixel));

            if (alpha != 255) {
                srcRed = (srcRed * 255) / alpha;
                srcGreen = (srcGreen * 255) / alpha;
                srcBlue = (srcBlue * 255) / alpha;
            }

            auto luminosity =
                int(0.299f * srcRed + 0.587f * srcGreen + 0.114f * srcBlue);
            auto tintRed = int(std::lround(
                blackRed + (int(whiteRed) - int(blackRed)) * (luminosity / 255.0f)));
            auto tintGreen =
                int(std::lround(blackGreen +
                                (int(whiteGreen) - int(blackGreen)) *
                                    (luminosity / 255.0f)));
            auto tintBlue = int(std::lround(
                blackBlue +
                (int(whiteBlue) - int(blackBlue)) * (luminosity / 255.0f)));

            auto outRed =
                int(std::lround(srcRed + (tintRed - srcRed) * amount));
            auto outGreen =
                int(std::lround(srcGreen + (tintGreen - srcGreen) * amount));
            auto outBlue =
                int(std::lround(srcBlue + (tintBlue - srcBlue) * amount));

            outRed = (outRed * alpha + 127) / 255;
            outGreen = (outGreen * alpha + 127) / 255;
            outBlue = (outBlue * alpha + 127) / 255;

            *pixel = (uint32_t(alpha) << 24) | (uint32_t(outRed) << 16) |
                     (uint32_t(outGreen) << 8) | uint32_t(outBlue);
            pixel++;
        }
    }
}

void VBitmap::Impl::applyStroke(const VRect &region, uint8_t red,
                                uint8_t green, uint8_t blue, float opacity,
                                float brushSize, float brushHardness,
                                int paintStyle)
{
    if (mFormat != VBitmap::Format::ARGB32_Premultiplied) return;

    auto clipped = region & rect();
    if (clipped.empty()) return;

    opacity = std::max(0.0f, std::min(1.0f, opacity));
    if (vIsZero(opacity)) return;

    const auto outerRadius = std::max(0.5f, brushSize * 0.5f);
    const auto hardness = std::max(0.0f, std::min(1.0f, brushHardness / 100.0f));
    const auto innerRadius = outerRadius * hardness;
    const auto outerSq = outerRadius * outerRadius;
    const auto innerSq = innerRadius * innerRadius;
    const auto radius = int(std::ceil(outerRadius));
    const auto width = clipped.width();
    const auto height = clipped.height();
    if (width <= 0 || height <= 0) return;

    std::vector<uint32_t> srcPixels(size_t(width) * size_t(height));
    auto dataPtr = data();
    for (int y = 0; y < height; ++y) {
        const auto *srcRow =
            reinterpret_cast<const uint32_t *>(dataPtr + mStride * (clipped.top() + y)) +
            clipped.left();
        std::copy(srcRow, srcRow + width,
                  srcPixels.begin() + size_t(y) * size_t(width));
    }

    for (int y = 0; y < height; ++y) {
        auto *dstRow =
            reinterpret_cast<uint32_t *>(dataPtr + mStride * (clipped.top() + y)) +
            clipped.left();
        for (int x = 0; x < width; ++x) {
            const auto srcPixel = srcPixels[size_t(y) * size_t(width) + size_t(x)];
            const auto srcAlpha = float(vAlpha(srcPixel)) / 255.0f;

            float strokeCoverage = 0.0f;
            const int minY = std::max(0, y - radius);
            const int maxY = std::min(height - 1, y + radius);
            const int minX = std::max(0, x - radius);
            const int maxX = std::min(width - 1, x + radius);
            for (int sy = minY; sy <= maxY; ++sy) {
                for (int sx = minX; sx <= maxX; ++sx) {
                    const auto dx = float(sx - x);
                    const auto dy = float(sy - y);
                    const auto distSq = dx * dx + dy * dy;
                    if (distSq > outerSq) continue;

                    const auto sampleAlpha =
                        float(vAlpha(srcPixels[size_t(sy) * size_t(width) +
                                              size_t(sx)])) /
                        255.0f;
                    if (vIsZero(sampleAlpha)) continue;

                    float falloff = 1.0f;
                    if (distSq > innerSq && outerSq > innerSq) {
                        const auto dist = std::sqrt(distSq);
                        falloff =
                            1.0f - ((dist - innerRadius) / (outerRadius - innerRadius));
                    }
                    strokeCoverage =
                        std::max(strokeCoverage, sampleAlpha * falloff);
                }
            }

            strokeCoverage = std::max(0.0f, strokeCoverage - srcAlpha);
            if (vIsZero(strokeCoverage)) {
                if (paintStyle == 2) dstRow[x] = 0;
                continue;
            }

            const auto strokeAlpha =
                uint8_t(std::lround(std::min(1.0f, strokeCoverage * opacity) * 255.0f));
            const auto strokePixel = premulPixel(red, green, blue, strokeAlpha);

            switch (paintStyle) {
            case 1:
                dstRow[x] = srcOverPixel(strokePixel, srcPixel);
                break;
            case 2:
                dstRow[x] = strokePixel;
                break;
            case 3: {
                const auto revealAlpha =
                    uint8_t(std::lround(std::min(1.0f, strokeCoverage) *
                                        float(vAlpha(srcPixel))));
                if (revealAlpha == 0) {
                    dstRow[x] = 0;
                    break;
                }
                auto srcRed = int(vRed(srcPixel));
                auto srcGreen = int(vGreen(srcPixel));
                auto srcBlue = int(vBlue(srcPixel));
                const auto srcBaseAlpha = int(vAlpha(srcPixel));
                if (srcBaseAlpha != 0 && srcBaseAlpha != 255) {
                    srcRed = (srcRed * 255) / srcBaseAlpha;
                    srcGreen = (srcGreen * 255) / srcBaseAlpha;
                    srcBlue = (srcBlue * 255) / srcBaseAlpha;
                }
                dstRow[x] = premulPixel(uint8_t(srcRed), uint8_t(srcGreen),
                                        uint8_t(srcBlue), revealAlpha);
                break;
            }
            default:
                dstRow[x] = strokePixel;
                break;
            }
        }
    }
}

void VBitmap::Impl::applyFourColorGradient(const VRect &region,
                                           const VBitmap &gradientMap,
                                           float amount)
{
    if (mFormat != VBitmap::Format::ARGB32_Premultiplied ||
        !gradientMap.valid()) {
        return;
    }

    auto clipped = region & rect() & gradientMap.rect();
    if (clipped.empty()) return;

    amount = std::max(0.0f, std::min(1.0f, amount));
    if (vIsZero(amount)) return;

    auto dataPtr = data();
    auto gradientPtr = gradientMap.data();
    const auto gradientStride = gradientMap.stride();

    for (int y = clipped.top(); y < clipped.bottom(); ++y) {
        uint32_t *pixel =
            reinterpret_cast<uint32_t *>(dataPtr + mStride * y) + clipped.left();
        const uint32_t *gradientPixel =
            reinterpret_cast<const uint32_t *>(gradientPtr + gradientStride * y) +
            clipped.left();
        for (int x = clipped.left(); x < clipped.right(); ++x) {
            const auto alpha = int(vAlpha(*pixel));
            if (alpha == 0) {
                ++pixel;
                ++gradientPixel;
                continue;
            }

            const auto effectRed = int(vRed(*gradientPixel));
            const auto effectGreen = int(vGreen(*gradientPixel));
            const auto effectBlue = int(vBlue(*gradientPixel));

            int outRed;
            int outGreen;
            int outBlue;
            if (vCompare(amount, 1.0f)) {
                outRed = (effectRed * alpha + 127) / 255;
                outGreen = (effectGreen * alpha + 127) / 255;
                outBlue = (effectBlue * alpha + 127) / 255;
            } else {
                auto srcRed = int(vRed(*pixel));
                auto srcGreen = int(vGreen(*pixel));
                auto srcBlue = int(vBlue(*pixel));
                if (alpha != 255) {
                    srcRed = (srcRed * 255) / alpha;
                    srcGreen = (srcGreen * 255) / alpha;
                    srcBlue = (srcBlue * 255) / alpha;
                }

                outRed = int(std::lround(srcRed + (effectRed - srcRed) * amount));
                outGreen = int(
                    std::lround(srcGreen + (effectGreen - srcGreen) * amount));
                outBlue =
                    int(std::lround(srcBlue + (effectBlue - srcBlue) * amount));

                outRed = (outRed * alpha + 127) / 255;
                outGreen = (outGreen * alpha + 127) / 255;
                outBlue = (outBlue * alpha + 127) / 255;
            }

            *pixel = (uint32_t(alpha) << 24) | (uint32_t(outRed) << 16) |
                     (uint32_t(outGreen) << 8) | uint32_t(outBlue);
            ++pixel;
            ++gradientPixel;
        }
    }
}

void VBitmap::Impl::generateFourColorGradientMap(const VRect &region,
                                                 const VPointF points[4],
                                                 const VColor colors[4])
{
    if (mFormat != VBitmap::Format::ARGB32_Premultiplied) return;

    auto clipped = region & rect();
    if (clipped.empty()) return;

    const FourColorGradientQuad quad(points);
    auto dataPtr = data();
    for (int y = clipped.top(); y < clipped.bottom(); ++y) {
        uint32_t *pixel =
            reinterpret_cast<uint32_t *>(dataPtr + mStride * y) + clipped.left();
        for (int x = clipped.left(); x < clipped.right(); ++x) {
            const float fx = x + 0.5f;
            const float fy = y + 0.5f;
            float u = 0.0f;
            float v = 0.0f;
            if (!quad.bilinearCoords({fx, fy}, u, v)) {
                *pixel++ = 0xff000000;
                continue;
            }

            const auto effectRed = bilinearChannel(colors, u, v, &VColor::red);
            const auto effectGreen =
                bilinearChannel(colors, u, v, &VColor::green);
            const auto effectBlue =
                bilinearChannel(colors, u, v, &VColor::blue);
            *pixel++ = 0xff000000 | (uint32_t(effectRed) << 16) |
                       (uint32_t(effectGreen) << 8) | uint32_t(effectBlue);
        }
    }
}

void VBitmap::Impl::applyFourColorGradient(const VRect &region,
                                           const VPointF points[4],
                                           const VColor colors[4],
                                           float amount)
{
    if (mFormat != VBitmap::Format::ARGB32_Premultiplied) return;

    auto clipped = region & rect();
    if (clipped.empty()) return;

    amount = std::max(0.0f, std::min(1.0f, amount));
    if (vIsZero(amount)) return;

    const FourColorGradientQuad quad(points);
    auto dataPtr = data();
    for (int y = clipped.top(); y < clipped.bottom(); ++y) {
        uint32_t *pixel =
            reinterpret_cast<uint32_t *>(dataPtr + mStride * y) + clipped.left();
        for (int x = clipped.left(); x < clipped.right(); ++x) {
            const auto alpha = int(vAlpha(*pixel));
            if (alpha == 0) {
                pixel++;
                continue;
            }

            auto srcRed = int(vRed(*pixel));
            auto srcGreen = int(vGreen(*pixel));
            auto srcBlue = int(vBlue(*pixel));
            if (alpha != 255) {
                srcRed = (srcRed * 255) / alpha;
                srcGreen = (srcGreen * 255) / alpha;
                srcBlue = (srcBlue * 255) / alpha;
            }

            const float fx = x + 0.5f;
            const float fy = y + 0.5f;
            float u = 0.0f;
            float v = 0.0f;
            if (!quad.bilinearCoords({fx, fy}, u, v)) {
                pixel++;
                continue;
            }

            const auto effectRed = bilinearChannel(colors, u, v, &VColor::red);
            const auto effectGreen =
                bilinearChannel(colors, u, v, &VColor::green);
            const auto effectBlue =
                bilinearChannel(colors, u, v, &VColor::blue);

            auto outRed =
                int(std::lround(srcRed + (effectRed - srcRed) * amount));
            auto outGreen = int(
                std::lround(srcGreen + (effectGreen - srcGreen) * amount));
            auto outBlue =
                int(std::lround(srcBlue + (effectBlue - srcBlue) * amount));

            outRed = (outRed * alpha + 127) / 255;
            outGreen = (outGreen * alpha + 127) / 255;
            outBlue = (outBlue * alpha + 127) / 255;

            *pixel = (uint32_t(alpha) << 24) | (uint32_t(outRed) << 16) |
                     (uint32_t(outGreen) << 8) | uint32_t(outBlue);
            pixel++;
        }
    }
}

VBitmap::VBitmap(size_t width, size_t height, VBitmap::Format format)
{
    if (width <= 0 || height <= 0 || format == Format::Invalid) return;

    mImpl = rc_ptr<Impl>(width, height, format);
}

VBitmap::VBitmap(uint8_t *data, size_t width, size_t height,
                 size_t bytesPerLine, VBitmap::Format format)
{
    if (!data || width <= 0 || height <= 0 || bytesPerLine <= 0 ||
        format == Format::Invalid)
        return;

    mImpl = rc_ptr<Impl>(data, width, height, bytesPerLine, format);
}

void VBitmap::reset(uint8_t *data, size_t w, size_t h, size_t bytesPerLine,
                    VBitmap::Format format)
{
    if (mImpl) {
        mImpl->reset(data, w, h, bytesPerLine, format);
    } else {
        mImpl = rc_ptr<Impl>(data, w, h, bytesPerLine, format);
    }
}

void VBitmap::reset(size_t w, size_t h, VBitmap::Format format)
{
    if (mImpl) {
        if (w == mImpl->width() && h == mImpl->height() &&
            format == mImpl->format()) {
            return;
        }
        mImpl->reset(w, h, format);
    } else {
        mImpl = rc_ptr<Impl>(w, h, format);
    }
}

size_t VBitmap::stride() const
{
    return mImpl ? mImpl->stride() : 0;
}

size_t VBitmap::width() const
{
    return mImpl ? mImpl->width() : 0;
}

size_t VBitmap::height() const
{
    return mImpl ? mImpl->height() : 0;
}

size_t VBitmap::depth() const
{
    return mImpl ? mImpl->mDepth : 0;
}

uint8_t *VBitmap::data()
{
    return mImpl ? mImpl->data() : nullptr;
}

uint8_t *VBitmap::data() const
{
    return mImpl ? mImpl->data() : nullptr;
}

VRect VBitmap::rect() const
{
    return mImpl ? mImpl->rect() : VRect();
}

VSize VBitmap::size() const
{
    return mImpl ? mImpl->size() : VSize();
}

bool VBitmap::valid() const
{
    return mImpl;
}

VBitmap::Format VBitmap::format() const
{
    return mImpl ? mImpl->format() : VBitmap::Format::Invalid;
}

void VBitmap::fill(uint32_t pixel)
{
    if (mImpl) mImpl->fill(pixel);
}

/*
 * This is special function which converts
 * RGB value to Luminosity and stores it in
 * the Alpha component of the pixel.
 * After this conversion the bitmap data is no more
 * in RGB space. but the Alpha component contains the
 *  Luminosity value of the pixel in HSL color space.
 * NOTE: this api has its own special usecase
 * make sure you know what you are doing before using
 * this api.
 */
void VBitmap::updateLuma()
{
    if (mImpl) mImpl->updateLuma();
}

void VBitmap::updateLuma(const VRect &region)
{
    if (mImpl) mImpl->updateLuma(region);
}

void VBitmap::applyFill(const VRect &region, uint8_t red, uint8_t green,
                        uint8_t blue, float amount)
{
    if (mImpl) mImpl->applyFill(region, red, green, blue, amount);
}

void VBitmap::applyTint(const VRect &region, uint8_t blackRed,
                        uint8_t blackGreen, uint8_t blackBlue,
                        uint8_t whiteRed, uint8_t whiteGreen,
                        uint8_t whiteBlue, float amount)
{
    if (mImpl) {
        mImpl->applyTint(region, blackRed, blackGreen, blackBlue, whiteRed,
                         whiteGreen, whiteBlue, amount);
    }
}

void VBitmap::applyStroke(const VRect &region, uint8_t red, uint8_t green,
                          uint8_t blue, float opacity, float brushSize,
                          float brushHardness, int paintStyle)
{
    if (mImpl) {
        mImpl->applyStroke(region, red, green, blue, opacity, brushSize,
                           brushHardness, paintStyle);
    }
}

void VBitmap::applyFourColorGradient(const VRect &region,
                                     const VBitmap &gradientMap, float amount)
{
    if (mImpl) mImpl->applyFourColorGradient(region, gradientMap, amount);
}

void VBitmap::generateFourColorGradientMap(const VRect &region,
                                           const VPointF points[4],
                                           const VColor colors[4])
{
    if (mImpl) mImpl->generateFourColorGradientMap(region, points, colors);
}

void VBitmap::applyFourColorGradient(const VRect &region,
                                     const VPointF points[4],
                                     const VColor colors[4], float amount)
{
    if (mImpl) mImpl->applyFourColorGradient(region, points, colors, amount);
}

V_END_NAMESPACE
