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

#include "vdrawable.h"
#include "../lottie/perfprofile.h"
#include "vdasher.h"
#include "vraster.h"
#include <cmath>

static VRect pathBounds(const VPath &path)
{
    const auto &points = path.points();
    if (points.empty()) return {};

    float left = points.front().x();
    float top = points.front().y();
    float right = left;
    float bottom = top;

    for (const auto &point : points) {
        left = std::min(left, point.x());
        top = std::min(top, point.y());
        right = std::max(right, point.x());
        bottom = std::max(bottom, point.y());
    }

    const int x = int(std::floor(left));
    const int y = int(std::floor(top));
    return {x, y, int(std::ceil(right)) - x, int(std::ceil(bottom)) - y};
}

struct TightClipResult {
    VRect clip;
    bool  skip{false};
};

static TightClipResult tightClipFromBounds(const VRect &clip,
                                           const VRect &pathBounds,
                                           VDrawable::Type type,
                                           const VDrawable::StrokeInfo *strokeInfo)
{
    if (pathBounds.empty()) return {clip, false};

    VRect bounds = pathBounds;
    if (type != VDrawable::Type::Fill && strokeInfo) {
        const int pad = int(std::ceil(strokeInfo->width * 0.5f)) + 2;
        bounds.setLeft(bounds.left() - pad);
        bounds.setTop(bounds.top() - pad);
        bounds.setRight(bounds.right() + pad);
        bounds.setBottom(bounds.bottom() + pad);
    }

    if (clip.empty()) return {bounds, false};

    auto result = clip & bounds;
    if (result.empty()) return {{}, true};
    return {result, false};
}

VDrawable::VDrawable(VDrawable::Type type)
{
    setType(type);
}

VDrawable::~VDrawable() noexcept
{
    if (mStrokeInfo) {
        if (mType == Type::StrokeWithDash) {
            delete static_cast<StrokeWithDashInfo *>(mStrokeInfo);
        } else {
            delete mStrokeInfo;
        }
    }
}

void VDrawable::setType(VDrawable::Type type)
{
    mType = type;
    if (mType == VDrawable::Type::Stroke) {
        mStrokeInfo = new StrokeInfo();
    } else if (mType == VDrawable::Type::StrokeWithDash) {
        mStrokeInfo = new StrokeWithDashInfo();
    }
}

void VDrawable::applyDashOp()
{
    if (mStrokeInfo && (mType == Type::StrokeWithDash)) {
        rlottie::internal::ScopedProfileEvent profile(
            rlottie::internal::ProfileEvent::DashApply);
        auto obj = static_cast<StrokeWithDashInfo *>(mStrokeInfo);
        if (!obj->mDash.empty()) {
            VDasher dasher(obj->mDash.data(), obj->mDash.size());
            mPath.clone(dasher.dashed(mPath));
        }
    }
}

void VDrawable::preprocess(const VRect &clip)
{
    if (mUseCustomRle) return;

    if (mFlag & (DirtyState::Path)) {
        auto rasterClip = tightClipFromBounds(clip, mPathBounds, mType,
                                              mStrokeInfo);
        if (rasterClip.skip) {
            if (mType == Type::Fill) {
                mRasterizer.rasterize(VPath(), mFillRule);
            } else {
                mRasterizer.rasterize(VPath(), mStrokeInfo->cap,
                                      mStrokeInfo->join, mStrokeInfo->width,
                                      mStrokeInfo->miterLimit);
            }
            return;
        }
        if (mType == Type::Fill) {
            mRasterizer.rasterize(std::move(mPath), mFillRule, rasterClip.clip);
        } else {
            applyDashOp();
            mRasterizer.rasterize(std::move(mPath), mStrokeInfo->cap, mStrokeInfo->join,
                                  mStrokeInfo->width, mStrokeInfo->miterLimit,
                                  rasterClip.clip);
        }
        mPath = {};
        mFlag &= ~DirtyFlag(DirtyState::Path);
    }
}

VRle VDrawable::rle()
{
    return mUseCustomRle ? mRle : mRasterizer.rle();
}

void VDrawable::setStrokeInfo(CapStyle cap, JoinStyle join, float miterLimit,
                              float strokeWidth)
{
    assert(mStrokeInfo);
    if ((mStrokeInfo->cap == cap) && (mStrokeInfo->join == join) &&
        vCompare(mStrokeInfo->miterLimit, miterLimit) &&
        vCompare(mStrokeInfo->width, strokeWidth))
        return;

    mStrokeInfo->cap = cap;
    mStrokeInfo->join = join;
    mStrokeInfo->miterLimit = miterLimit;
    mStrokeInfo->width = strokeWidth;
    mFlag |= DirtyState::Path;
}

void VDrawable::setDashInfo(std::vector<float> &dashInfo)
{
    assert(mStrokeInfo);
    assert(mType == VDrawable::Type::StrokeWithDash);

    auto obj = static_cast<StrokeWithDashInfo *>(mStrokeInfo);
    bool hasChanged = false;

    if (obj->mDash.size() == dashInfo.size()) {
        for (uint32_t i = 0; i < dashInfo.size(); ++i) {
            if (!vCompare(obj->mDash[i], dashInfo[i])) {
                hasChanged = true;
                break;
            }
        }
    } else {
        hasChanged = true;
    }

    if (!hasChanged) return;

    obj->mDash = dashInfo;

    mFlag |= DirtyState::Path;
}

void VDrawable::setPath(const VPath &path)
{
    mUseCustomRle = false;
    mPath = path;
    mPathBounds = pathBounds(mPath);
    mFlag |= DirtyState::Path;
}

void VDrawable::setRle(const VRle &rle)
{
    mUseCustomRle = true;
    if (!rle.empty() && !rle.unique()) {
        mRle.clone(rle);
    } else {
        mRle = rle;
    }
    mPathBounds = rle.boundingRect();
    mPath = {};
    mFlag &= ~DirtyFlag(DirtyState::Path);
}
