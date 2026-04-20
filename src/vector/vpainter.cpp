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

#include "vpainter.h"
#include <algorithm>
#include "vpath.h"
#include "vraster.h"


V_BEGIN_NAMESPACE


void VPainter::drawRle(const VPoint &, const VRle &rle)
{
    if (rle.empty()) return;
    // mSpanData.updateSpanFunc();

    if (!mSpanData.mUnclippedBlendFunc) return;

    // do draw after applying clip.
    rle.intersect(mSpanData.clipRect(), mSpanData.mUnclippedBlendFunc,
                  &mSpanData);
}

void VPainter::drawRle(const VRle &rle, const VRle &clip)
{
    if (rle.empty() || clip.empty()) return;

    if (!mSpanData.mUnclippedBlendFunc) return;

    rle.intersect(clip, mSpanData.mUnclippedBlendFunc, &mSpanData);
}

static void fillRect(const VRect &r, VSpanData *data)
{
    auto clip = data->clipRect();
    auto x1 = std::max(r.x(), clip.left());
    auto x2 = std::min(r.x() + r.width(), clip.right());
    auto y1 = std::max(r.y(), clip.top());
    auto y2 = std::min(r.y() + r.height(), clip.bottom());

    if (x2 <= x1 || y2 <= y1) return;

    const int  nspans = 256;
    VRle::Span spans[nspans];

    int y = y1;
    while (y < y2) {
        int n = std::min(nspans, y2 - y);
        int i = 0;
        while (i < n) {
            spans[i].x = short(x1);
            spans[i].len = uint16_t(x2 - x1);
            spans[i].y = short(y + i);
            spans[i].coverage = 255;
            ++i;
        }

        data->mUnclippedBlendFunc(n, spans, data);
        y += n;
    }
}

void VPainter::drawBitmapUntransform(const VRect &  target,
                                         const VBitmap &bitmap,
                                         const VRect &  source,
                                         uint8_t        const_alpha)
{
    mSpanData.initTexture(&bitmap, const_alpha, source);
    if (!mSpanData.mUnclippedBlendFunc) return;

    // update translation matrix for source texture.
    mSpanData.dx = float(source.x() - target.x());
    mSpanData.dy = float(source.y() - target.y());

    fillRect(target, &mSpanData);
}

void VPainter::drawBitmapTransform(const VMatrix &sourceToTarget,
                                   const VBitmap &bitmap,
                                   const VRect &source,
                                   uint8_t const_alpha)
{
    if (!bitmap.valid() || source.empty()) return;

    mSpanData.initTexture(&bitmap, const_alpha, source);
    mSpanData.setupMatrix(sourceToTarget);
    mSpanData.updateSpanFunc();
    if (!mSpanData.mUnclippedBlendFunc) return;

    VPath path;
    path.addRect(VRectF(float(source.x()), float(source.y()),
                        float(source.width()), float(source.height())));
    path.transform(sourceToTarget);

    VRasterizer rasterizer;
    rasterizer.rasterize(std::move(path), FillRule::Winding,
                         mSpanData.clipRect());
    drawRle(VPoint(), rasterizer.rle());
}

VPainter::VPainter(VBitmap *buffer)
{
    begin(buffer);
}
bool VPainter::begin(VBitmap *buffer)
{
    mBuffer.prepare(buffer);
    mSpanData.init(&mBuffer);
    // TODO find a better api to clear the surface
    mBuffer.clear();
    return true;
}
void VPainter::end() {}

void VPainter::setDrawRegion(const VRect &region)
{
    mSpanData.setDrawRegion(region);
}

void VPainter::setDrawRegion(const VRect &region, const VPoint &bufferOffset)
{
    mSpanData.setDrawRegion(region, bufferOffset);
}

void VPainter::setBrush(const VBrush &brush)
{
    mSpanData.setup(brush);
}

void VPainter::setBlendMode(BlendMode mode)
{
    mSpanData.mBlendMode = mode;
}

VRect VPainter::clipBoundingRect() const
{
    return mSpanData.clipRect();
}

void VPainter::drawBitmap(const VPoint &point, const VBitmap &bitmap,
                          const VRect &source, uint8_t const_alpha)
{
    if (!bitmap.valid()) return;

    drawBitmap(VRect(point, bitmap.size()),
               bitmap, source, const_alpha);
}

void VPainter::drawBitmap(const VRect &target, const VBitmap &bitmap,
                          const VRect &source, uint8_t const_alpha)
{
    if (!bitmap.valid()) return;

    // clear any existing brush data.
    setBrush(VBrush());

    if (target.size() == source.size()) {
        drawBitmapUntransform(target, bitmap, source, const_alpha);
    } else {
        VMatrix sourceToTarget;
        sourceToTarget.translate(float(target.x()), float(target.y()));
        sourceToTarget.scale(float(target.width()) / source.width(),
                             float(target.height()) / source.height());
        sourceToTarget.translate(float(-source.x()), float(-source.y()));
        drawBitmapTransform(sourceToTarget, bitmap, source, const_alpha);
    }
}

void VPainter::drawBitmap(const VPoint &point, const VBitmap &bitmap,
                          uint8_t const_alpha)
{
    if (!bitmap.valid()) return;

    drawBitmap(VRect(point, bitmap.size()),
               bitmap, bitmap.rect(),
               const_alpha);
}

void VPainter::drawBitmap(const VRect &rect, const VBitmap &bitmap,
                          uint8_t const_alpha)
{
    if (!bitmap.valid()) return;

    drawBitmap(rect, bitmap, bitmap.rect(),
               const_alpha);
}

void VPainter::drawBitmap(const VBitmap &bitmap,
                          const VMatrix &sourceToTarget,
                          const VRect &source, uint8_t const_alpha)
{
    if (!bitmap.valid()) return;

    drawBitmapTransform(sourceToTarget, bitmap, source, const_alpha);
}

void VPainter::drawBitmap(const VBitmap &bitmap,
                          const VMatrix &sourceToTarget,
                          uint8_t const_alpha)
{
    if (!bitmap.valid()) return;

    drawBitmapTransform(sourceToTarget, bitmap, bitmap.rect(), const_alpha);
}

V_END_NAMESPACE
