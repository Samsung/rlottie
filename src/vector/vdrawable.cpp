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

#include "vdrawable.h"
#include "vdasher.h"
#include "vraster.h"

VDrawable::VDrawable(VDrawable::Type type)
{
    setType(type);
}

VDrawable::~VDrawable()
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
        auto obj = static_cast<StrokeWithDashInfo *>(mStrokeInfo);
        if (!obj->mDash.empty()) {
            VDasher dasher(obj->mDash.data(), obj->mDash.size());
            mPath.clone(dasher.dashed(mPath));
        }
    }
}

void VDrawable::preprocess(const VRect &clip)
{
    if (mFlag & (DirtyState::Path)) {
        if (mType == Type::Fill) {
            mRasterizer.rasterize(std::move(mPath), mFillRule, clip);
        } else {
            applyDashOp();
            mRasterizer.rasterize(std::move(mPath), mStrokeInfo->cap, mStrokeInfo->join,
                                  mStrokeInfo->width, mStrokeInfo->miterLimit, clip);
        }
        mPath = {};
        mFlag &= ~DirtyFlag(DirtyState::Path);
    }
}

VRle VDrawable::rle()
{
    return mRasterizer.rle();
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
        for (uint i = 0; i < dashInfo.size(); ++i) {
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
    mPath = path;
    mFlag |= DirtyState::Path;
}
