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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef VDRAWABLE_H
#define VDRAWABLE_H
#include <future>
#include <cstring>
#include "vbrush.h"
#include "vpath.h"
#include "vrle.h"
#include "vraster.h"

class VDrawable {
public:
    enum class DirtyState : unsigned char {
        None = 1<<0,
        Path = 1<<1,
        Stroke = 1<<2,
        Brush = 1<<3,
        All = (Path | Stroke | Brush)
    };

    enum class Type : unsigned char{
        Fill,
        Stroke,
        StrokeWithDash
    };

    explicit VDrawable(VDrawable::Type type = Type::Fill);
    void setType(VDrawable::Type type);
    ~VDrawable();

    typedef vFlag<DirtyState> DirtyFlag;
    void setPath(const VPath &path);
    void setFillRule(FillRule rule) { mFillRule = rule; }
    void setBrush(const VBrush &brush) { mBrush = brush; }
    void setStrokeInfo(CapStyle cap, JoinStyle join, float miterLimit,
                       float strokeWidth);
    void setDashInfo(std::vector<float> &dashInfo);
    void preprocess(const VRect &clip);
    void applyDashOp();
    VRle rle();
    void setName(const char *name)
    {
        mName = name;
    }
    const char* name() const { return mName; }

public:
    struct StrokeInfo {
        float              width{0.0};
        float              miterLimit{10};
        CapStyle           cap{CapStyle::Flat};
        JoinStyle          join{JoinStyle::Bevel};
    };

    struct StrokeWithDashInfo : public StrokeInfo{
        std::vector<float> mDash;
    };

public:
    VPath                    mPath;
    VBrush                   mBrush;
    VRasterizer              mRasterizer;
    StrokeInfo              *mStrokeInfo{nullptr};

    DirtyFlag                mFlag{DirtyState::All};
    FillRule                 mFillRule{FillRule::Winding};
    VDrawable::Type          mType{Type::Fill};

    const char              *mName{nullptr};
};

#endif  // VDRAWABLE_H
