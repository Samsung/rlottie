/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "vpainter.h"
#include <algorithm>


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
    auto x1 = std::max(r.x(), 0);
    auto x2 = std::min(r.x() + r.width(), data->mDrawableSize.width());
    auto y1 = std::max(r.y(), 0);
    auto y2 = std::min(r.y() + r.height(), data->mDrawableSize.height());

    if (x2 <= x1 || y2 <= y1) return;

    const int  nspans = 256;
    VRle::Span spans[nspans];

    int y = y1;
    while (y < y2) {
        int n = std::min(nspans, y2 - y);
        int i = 0;
        while (i < n) {
            spans[i].x = short(x1);
            spans[i].len = ushort(x2 - x1);
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
    mSpanData.dx = float(-target.x());
    mSpanData.dy = float(-target.y());

    VRect rr = source.translated(target.x(), target.y());

    fillRect(rr, &mSpanData);
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
        // @TODO scaling
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

V_END_NAMESPACE
