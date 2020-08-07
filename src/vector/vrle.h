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

#ifndef VRLE_H
#define VRLE_H

#include <vector>
#include "vcowptr.h"
#include "vglobal.h"
#include "vpoint.h"
#include "vrect.h"

V_BEGIN_NAMESPACE

class VRle {
public:
    struct Span {
        short  x{0};
        short  y{0};
        ushort len{0};
        uchar  coverage{0};
    };
    using VRleSpanCb =  void (*)(size_t count, const VRle::Span *spans,
                                 void *userData);
    bool  empty() const;
    VRect boundingRect() const;
    void setBoundingRect(const VRect &bbox);
    void  addSpan(const VRle::Span *span, size_t count);

    void reset();
    void translate(const VPoint &p);
    void invert();

    void operator*=(uchar alpha);

    void intersect(const VRect &r, VRleSpanCb cb, void *userData) const;
    void intersect(const VRle &rle, VRleSpanCb cb, void *userData) const;

    void operator&=(const VRle &o);
    VRle operator&(const VRle &o) const;
    VRle operator-(const VRle &o) const;
    VRle operator+(const VRle &o) const;
    VRle operator^(const VRle &o) const;

    static VRle toRle(const VRect &rect);

    bool unique() const {return d.unique();}
    size_t refCount() const { return d.refCount();}
    void clone(const VRle &o);

public:
    struct VRleData {
        enum class OpCode {
            Add,
            Xor
        };
        bool  empty() const { return mSpans.empty(); }
        void  addSpan(const VRle::Span *span, size_t count);
        void  updateBbox() const;
        VRect bbox() const;
        void setBbox(const VRect &bbox) const;
        void  reset();
        void  translate(const VPoint &p);
        void  operator*=(uchar alpha);
        void  invert();
        void  opIntersect(const VRect &, VRle::VRleSpanCb, void *) const;
        void  opGeneric(const VRle::VRleData &, const VRle::VRleData &, OpCode code);
        void  opSubstract(const VRle::VRleData &, const VRle::VRleData &);
        void  opIntersect(const VRle::VRleData &, const VRle::VRleData &);
        void  addRect(const VRect &rect);
        void  clone(const VRle::VRleData &);
        std::vector<VRle::Span> mSpans;
        VPoint                  mOffset;
        mutable VRect           mBbox;
        mutable bool            mBboxDirty = true;
    };
private:
    friend void opIntersectHelper(const VRle::VRleData &obj1,
                                  const VRle::VRleData &obj2,
                                  VRle::VRleSpanCb cb, void *userData);
    vcow_ptr<VRleData> d;
};

inline bool VRle::empty() const
{
    return d->empty();
}

inline void VRle::addSpan(const VRle::Span *span, size_t count)
{
    d.write().addSpan(span, count);
}

inline VRect VRle::boundingRect() const
{
    return d->bbox();
}

inline void VRle::setBoundingRect(const VRect & bbox)
{
    d->setBbox(bbox);
}

inline void VRle::invert()
{
    d.write().invert();
}

inline void VRle::operator*=(uchar alpha)
{
    d.write() *= alpha;
}

inline VRle VRle::operator&(const VRle &o) const
{
    if (empty() || o.empty()) return VRle();

    VRle result;
    result.d.write().opIntersect(d.read(), o.d.read());

    return result;
}

inline VRle VRle::operator+(const VRle &o) const
{
    if (empty()) return o;
    if (o.empty()) return *this;

    VRle result;
    result.d.write().opGeneric(d.read(), o.d.read(), VRleData::OpCode::Add);

    return result;
}

inline VRle VRle::operator^(const VRle &o) const
{
    if (empty()) return o;
    if (o.empty()) return *this;

    VRle result;
    result.d.write().opGeneric(d.read(), o.d.read(), VRleData::OpCode::Xor);

    return result;
}

inline VRle VRle::operator-(const VRle &o) const
{
    if (empty()) return VRle();
    if (o.empty()) return *this;

    VRle result;
    result.d.write().opSubstract(d.read(), o.d.read());

    return result;
}

inline void VRle::reset()
{
    d.write().reset();
}

inline void VRle::clone(const VRle &o)
{
    d.write().clone(o.d.read());
}

inline void VRle::translate(const VPoint &p)
{
    d.write().translate(p);
}

inline void VRle::intersect(const VRect &r, VRleSpanCb cb, void *userData) const
{
    d->opIntersect(r, cb, userData);
}

inline void VRle::intersect(const VRle &r, VRleSpanCb cb, void *userData) const
{
    if (empty() || r.empty()) return;
    opIntersectHelper(d.read(), r.d.read(), cb, userData);
}

V_END_NAMESPACE

#endif  // VRLE_H
