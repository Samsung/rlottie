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

#ifndef LOTModel_H
#define LOTModel_H

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include "varenaalloc.h"
#include "vbezier.h"
#include "vbrush.h"
#include "vinterpolator.h"
#include "vmatrix.h"
#include "vpath.h"
#include "vpoint.h"
#include "vrect.h"

V_USE_NAMESPACE

namespace rlottie {

namespace internal {

using Marker = std::tuple<std::string, int, int>;

using LayerInfo = Marker;

template <typename T>
inline T lerp(const T &start, const T &end, float t)
{
    return start + t * (end - start);
}

namespace model {

enum class MatteType : uchar { None = 0, Alpha = 1, AlphaInv, Luma, LumaInv };

enum class BlendMode : uchar {
    Normal = 0,
    Multiply = 1,
    Screen = 2,
    OverLay = 3
};

class Color {
public:
    Color() = default;
    Color(float red, float green, float blue) : r(red), g(green), b(blue) {}
    VColor toColor(float a = 1)
    {
        return VColor(uchar(255 * r), uchar(255 * g), uchar(255 * b),
                      uchar(255 * a));
    }
    friend inline Color operator+(const Color &c1, const Color &c2);
    friend inline Color operator-(const Color &c1, const Color &c2);
    friend inline bool operator==(const Color &c1, const Color &c2);
public:
    float r{1};
    float g{1};
    float b{1};
};

inline Color operator-(const Color &c1, const Color &c2)
{
    return Color(c1.r - c2.r, c1.g - c2.g, c1.b - c2.b);
}
inline Color operator+(const Color &c1, const Color &c2)
{
    return Color(c1.r + c2.r, c1.g + c2.g, c1.b + c2.b);
}

inline const Color operator*(const Color &c, float m)
{
    return Color(c.r * m, c.g * m, c.b * m);
}

inline const Color operator*(float m, const Color &c)
{
    return Color(c.r * m, c.g * m, c.b * m);
}

inline bool operator==(const Color &c1, const Color &c2)
{
    if (vCompare(c1.r, c2.r) &&
        vCompare(c1.g, c2.g) &&
        vCompare(c1.b, c2.b))
      return true;
    return false;
}



struct PathData {
    std::vector<VPointF> mPoints;
    bool                 mClosed = false; /* "c" */
    void        reserve(size_t size) { mPoints.reserve(mPoints.size() + size); }
    static void lerp(const PathData &start, const PathData &end, float t,
                     VPath &result)
    {
        result.reset();
        auto size = std::min(start.mPoints.size(), end.mPoints.size());
        /* reserve exact memory requirement at once
         * ptSize = size + 1(size + close)
         * elmSize = size/3 cubic + 1 move + 1 close
         */
        result.reserve(size + 1, size / 3 + 2);
        result.moveTo(start.mPoints[0] +
                      t * (end.mPoints[0] - start.mPoints[0]));
        for (size_t i = 1; i < size; i += 3) {
            result.cubicTo(
                start.mPoints[i] + t * (end.mPoints[i] - start.mPoints[i]),
                start.mPoints[i + 1] +
                    t * (end.mPoints[i + 1] - start.mPoints[i + 1]),
                start.mPoints[i + 2] +
                    t * (end.mPoints[i + 2] - start.mPoints[i + 2]));
        }
        if (start.mClosed) result.close();
    }
    void toPath(VPath &path) const
    {
        path.reset();

        if (mPoints.empty()) return;

        auto size = mPoints.size();
        auto points = mPoints.data();
        /* reserve exact memory requirement at once
         * ptSize = size + 1(size + close)
         * elmSize = size/3 cubic + 1 move + 1 close
         */
        path.reserve(size + 1, size / 3 + 2);
        path.moveTo(points[0]);
        for (size_t i = 1; i < size; i += 3) {
            path.cubicTo(points[i], points[i + 1], points[i + 2]);
        }
        if (mClosed) path.close();
    }
};

template <typename T>
struct Value {
    T     mStartValue;
    T     mEndValue;
    T     at(float t) const { return lerp(mStartValue, mEndValue, t); }
    float angle(float) const { return 0; }
};

template <>
struct Value<VPointF> {
    VPointF mStartValue;
    VPointF mEndValue;
    VPointF mInTangent;
    VPointF mOutTangent;
    bool    mPathKeyFrame = false;

    VPointF at(float t) const
    {
        if (mPathKeyFrame) {
            /*
             * position along the path calcualated
             * using bezier at progress length (t * bezlen)
             */
            VBezier b =
                VBezier::fromPoints(mStartValue, mStartValue + mOutTangent,
                                    mEndValue + mInTangent, mEndValue);
            return b.pointAt(b.tAtLength(t * b.length()));
        }
        return lerp(mStartValue, mEndValue, t);
    }

    float angle(float t) const
    {
        if (mPathKeyFrame) {
            VBezier b =
                VBezier::fromPoints(mStartValue, mStartValue + mOutTangent,
                                    mEndValue + mInTangent, mEndValue);
            return b.angleAt(b.tAtLength(t * b.length()));
        }
        return 0;
    }
};

template <typename T>
class KeyFrame {
public:
    float progress(int frameNo) const
    {
        return mInterpolator ? mInterpolator->value((frameNo - mStartFrame) /
                                                    (mEndFrame - mStartFrame))
                             : 0;
    }
    T     value(int frameNo) const { return mValue.at(progress(frameNo)); }
    float angle(int frameNo) const { return mValue.angle(progress(frameNo)); }

public:
    float          mStartFrame{0};
    float          mEndFrame{0};
    VInterpolator *mInterpolator{nullptr};
    Value<T>       mValue;
};

template <typename T>
class DynamicProperty {
public:
    T value(int frameNo) const
    {
        if (mKeyFrames.front().mStartFrame >= frameNo)
            return mKeyFrames.front().mValue.mStartValue;
        if (mKeyFrames.back().mEndFrame <= frameNo)
            return mKeyFrames.back().mValue.mEndValue;

        for (const auto &keyFrame : mKeyFrames) {
            if (frameNo >= keyFrame.mStartFrame && frameNo < keyFrame.mEndFrame)
                return keyFrame.value(frameNo);
        }
        return T();
    }

    float angle(int frameNo) const
    {
        if ((mKeyFrames.front().mStartFrame >= frameNo) ||
            (mKeyFrames.back().mEndFrame <= frameNo))
            return 0;

        for (const auto &keyFrame : mKeyFrames) {
            if (frameNo >= keyFrame.mStartFrame && frameNo < keyFrame.mEndFrame)
                return keyFrame.angle(frameNo);
        }
        return 0;
    }

    bool changed(int prevFrame, int curFrame) const
    {
        auto first = mKeyFrames.front().mStartFrame;
        auto last = mKeyFrames.back().mEndFrame;

        return !((first > prevFrame && first > curFrame) ||
                 (last < prevFrame && last < curFrame));
    }

public:
    std::vector<KeyFrame<T>> mKeyFrames;
};

template <typename T>
class Property {
public:
    Property() { construct(impl.mValue, {}); }
    explicit Property(T value) { construct(impl.mValue, std::move(value)); }

    const DynamicProperty<T> &animation() const
    {
        return *(impl.mAnimInfo.get());
    }
    const T &value() const { return impl.mValue; }

    DynamicProperty<T> &animation()
    {
        if (mStatic) {
            destroy();
            construct(impl.mAnimInfo, std::make_unique<DynamicProperty<T>>());
            mStatic = false;
        }
        return *(impl.mAnimInfo.get());
    }

    T &value()
    {
        assert(mStatic);
        return impl.mValue;
    }

    Property(Property &&other) noexcept
    {
        if (!other.mStatic) {
            construct(impl.mAnimInfo, std::move(other.impl.mAnimInfo));
            mStatic = false;
        } else {
            construct(impl.mValue, std::move(other.impl.mValue));
            mStatic = true;
        }
    }
    // delete special member functions
    Property(const Property &) = delete;
    Property &operator=(const Property &) = delete;
    Property &operator=(Property &&) = delete;

    ~Property() { destroy(); }

    bool isStatic() const { return mStatic; }

    T value(int frameNo) const
    {
        return isStatic() ? value() : animation().value(frameNo);
    }

    // special function only for type T=PathData
    template <typename SpecialT = PathData>
    auto value(int frameNo, VPath &path) const ->
        typename std::enable_if_t<std::is_same<T, SpecialT>::value, void>
    {
        if (isStatic()) {
            value().toPath(path);
        } else {
            const auto &vec = animation().mKeyFrames;
            if (vec.front().mStartFrame >= frameNo)
                return vec.front().mValue.mStartValue.toPath(path);
            if (vec.back().mEndFrame <= frameNo)
                return vec.back().mValue.mEndValue.toPath(path);

            for (const auto &keyFrame : vec) {
                if (frameNo >= keyFrame.mStartFrame &&
                    frameNo < keyFrame.mEndFrame) {
                    PathData::lerp(keyFrame.mValue.mStartValue,
                                   keyFrame.mValue.mEndValue,
                                   keyFrame.progress(frameNo), path);
                }
            }
        }
    }

    float angle(int frameNo) const
    {
        return isStatic() ? 0 : animation().angle(frameNo);
    }

    bool changed(int prevFrame, int curFrame) const
    {
        return isStatic() ? false : animation().changed(prevFrame, curFrame);
    }

private:
    template <typename Tp>
    void construct(Tp &member, Tp &&val)
    {
        new (&member) Tp(std::move(val));
    }

    void destroy()
    {
        if (mStatic) {
            impl.mValue.~T();
        } else {
            using std::unique_ptr;
            impl.mAnimInfo.~unique_ptr<DynamicProperty<T>>();
        }
    }
    union details {
        std::unique_ptr<DynamicProperty<T>> mAnimInfo;
        T                                   mValue;
        details(){};
        details(const details &) = delete;
        details(details &&) = delete;
        details &operator=(details &&) = delete;
        details &operator=(const details &) = delete;
        ~details(){};
    } impl;
    bool mStatic{true};
};

/* *
 * Hand written std::variant equivalent till c++17
 */
class PropertyText
{
public:
    enum class Type {
        Opacity = 0,
        Rotation,
        Tracking,
        StrokeWidth,
        Position,
        Scale,
        Anchor,
        StrokeColor,
        FillColor,
    };
    PropertyText(PropertyText::Type prop):mProperty(prop) {
        switch (mProperty) {
            case Type::Opacity:
            case Type::Rotation:
            case Type::Tracking:
            case Type::StrokeWidth:
                construct(impl.mFloat, {});
                break;
            case Type::Position:
            case Type::Scale:
            case Type::Anchor:
                construct(impl.mPoint, {});
                break;
            case Type::StrokeColor:
            case Type::FillColor:
                construct(impl.mColor, {});
                break;
        }
    }
    ~PropertyText() { destroy(); }

    PropertyText(PropertyText &&other) noexcept {
        switch (other.mProperty) {
            case Type::Opacity:
            case Type::Rotation:
            case Type::Tracking:
            case Type::StrokeWidth:
                construct(impl.mFloat, std::move(other.impl.mFloat));
                break;
            case Type::Position:
            case Type::Scale:
            case Type::Anchor:
                construct(impl.mPoint, std::move(other.impl.mPoint));
                break;
            case Type::StrokeColor:
            case Type::FillColor:
                construct(impl.mColor, std::move(other.impl.mColor));
                break;
        }
    }

    // delete special member functions
    PropertyText(const PropertyText &) = delete;
    PropertyText& operator=(const PropertyText&) = delete;
    PropertyText& operator=(PropertyText&&) = delete;

    PropertyText::Type type() const {return mProperty;}

    Property<float>& opacity() {assert(mProperty == Type::Opacity); return impl.mFloat;}
    Property<float>& roation() {assert(mProperty == Type::Rotation); return impl.mFloat;}
    Property<float>& tracking() {assert(mProperty == Type::Tracking); return impl.mFloat;}
    Property<float>& strokeWidth() {assert(mProperty == Type::StrokeWidth); return impl.mFloat;}

    Property<VPointF>& position() {assert(mProperty == Type::Position); return impl.mPoint;}
    Property<VPointF>& scale() {assert(mProperty == Type::Scale); return impl.mPoint;}
    Property<VPointF>& anchor() {assert(mProperty == Type::Anchor); return impl.mPoint;}

    Property<Color>& strokeColor() {assert(mProperty == Type::StrokeColor); return impl.mColor;}
    Property<Color>& fillColor() {assert(mProperty == Type::FillColor); return impl.mColor;}
private:
    template <typename Tp>
    void construct(Tp& member, Tp&& val)
    {
        new (&member) Tp(std::move(val));
    }
    void destroy() {
        switch (mProperty) {
            case Type::Opacity:
            case Type::Rotation:
            case Type::Tracking:
            case Type::StrokeWidth:
                impl.mFloat.~Property<float>();
                break;
            case Type::Position:
            case Type::Scale:
            case Type::Anchor:
                impl.mPoint.~Property<VPointF>();
                break;
            case Type::StrokeColor:
            case Type::FillColor:
                impl.mColor.~Property<Color>();
                break;
        }
    }
private:
    PropertyText::Type     mProperty;
    union details {
        Property<float>        mFloat;
        Property<VPointF>      mPoint;
        Property<Color>  mColor;
        details(){};
        details(const details&) = delete;
        details(details&&) = delete;
        details& operator=(details&&) = delete;
        details& operator=(const details&) = delete;
        ~details(){};
    }impl;
};

class Path;
struct PathData;
struct Dash {
    std::vector<Property<float>> mData;
    bool                         empty() const { return mData.empty(); }
    size_t                       size() const { return mData.size(); }
    bool                         isStatic() const
    {
        for (const auto &elm : mData)
            if (!elm.isStatic()) return false;
        return true;
    }
    void getDashInfo(int frameNo, std::vector<float> &result) const;
};

class Mask {
public:
    enum class Mode { None, Add, Substarct, Intersect, Difference };
    float opacity(int frameNo) const
    {
        return mOpacity.value(frameNo) / 100.0f;
    }
    bool isStatic() const { return mIsStatic; }

public:
    Property<PathData> mShape;
    Property<float>    mOpacity{100};
    bool               mInv{false};
    bool               mIsStatic{true};
    Mask::Mode         mMode;
};

class Object {
public:
    enum class Type : unsigned char {
        Composition = 1,
        Layer,
        Group,
        Transform,
        Fill,
        Stroke,
        GFill,
        GStroke,
        Rect,
        Ellipse,
        Path,
        Polystar,
        Trim,
        Repeater
    };

    explicit Object(Object::Type type) : mPtr(nullptr)
    {
        mData._type = type;
        mData._static = true;
        mData._shortString = true;
        mData._hidden = false;
    }
    ~Object()
    {
        if (!shortString() && mPtr) free(mPtr);
    }
    Object(const Object &) = delete;
    Object &operator=(const Object &) = delete;

    void         setStatic(bool value) { mData._static = value; }
    bool         isStatic() const { return mData._static; }
    bool         hidden() const { return mData._hidden; }
    void         setHidden(bool value) { mData._hidden = value; }
    void         setType(Object::Type type) { mData._type = type; }
    Object::Type type() const { return mData._type; }
    void         setName(const char *name)
    {
        if (name) {
            auto len = strlen(name);
            if (len < maxShortStringLength) {
                setShortString(true);
                strncpy(mData._buffer, name, len + 1);
            } else {
                setShortString(false);
                mPtr = strdup(name);
            }
        }
    }
    const char *name() const { return shortString() ? mData._buffer : mPtr; }

private:
    static constexpr unsigned char maxShortStringLength = 14;
    void setShortString(bool value) { mData._shortString = value; }
    bool shortString() const { return mData._shortString; }
    struct Data {
        char         _buffer[maxShortStringLength];
        Object::Type _type;
        bool         _static : 1;
        bool         _hidden : 1;
        bool         _shortString : 1;
    };
    union {
        Data  mData;
        char *mPtr{nullptr};
    };
};

struct Asset {
    enum class Type : unsigned char { Precomp, Image, Char };
    bool                  isStatic() const { return mStatic; }
    void                  setStatic(bool value) { mStatic = value; }
    VBitmap               bitmap() const { return mBitmap; }
    void                  loadImageData(std::string data);
    void                  loadImagePath(std::string Path);
    Type                  mAssetType{Type::Precomp};
    bool                  mStatic{true};
    std::string           mRefId;  // ref id
    std::vector<Object *> mLayers;
    // image asset data
    int     mWidth{0};
    int     mHeight{0};
    VBitmap mBitmap;
};

class Fonts {
public:
	std::string		mFontName;
	std::string		mFontFamily;
	std::string		mFontStyle;
	double			mFontAscent;
};

class Chars {
public:
    std::string              mCh;            /* ch */
    std::string              mStyle;         /* style */
    std::string              mFontFamily;    /* fFamily */
    double                   mSize;          /* size */
    double                   mWidth;         /* w */
    std::vector<VPath>       mShapePathData; /* data */
};

class Layer;

class Composition : public Object {
public:
    Composition() : Object(Object::Type::Composition) {}
    std::vector<LayerInfo>     layerInfoList() const;
    const std::vector<Marker> &markers() const { return mMarkers; }
    double                     duration() const
    {
        return frameDuration() / frameRate();  // in second
    }
    size_t frameAtPos(double pos) const
    {
        if (pos < 0) pos = 0;
        if (pos > 1) pos = 1;
        return size_t(round(pos * frameDuration()));
    }
    long frameAtTime(double timeInSec) const
    {
        return long(frameAtPos(timeInSec / duration()));
    }
    size_t totalFrame() const { return mEndFrame - mStartFrame; }
    long   frameDuration() const { return mEndFrame - mStartFrame - 1; }
    float  frameRate() const { return mFrameRate; }
    size_t startFrame() const { return mStartFrame; }
    size_t endFrame() const { return mEndFrame; }
    VSize  size() const { return mSize; }
    void   processRepeaterObjects();
    void   updateStats();

public:
    struct Stats {
        uint16_t precompLayerCount{0};
        uint16_t solidLayerCount{0};
        uint16_t shapeLayerCount{0};
        uint16_t imageLayerCount{0};
        uint16_t nullLayerCount{0};
    };

public:
    std::string                              mVersion;
    VSize                                    mSize;
    long                                     mStartFrame{0};
    long                                     mEndFrame{0};
    float                                    mFrameRate{60};
    BlendMode                                mBlendMode{BlendMode::Normal};
    Layer *                                  mRootLayer{nullptr};
    std::unordered_map<std::string, Asset *> mAssets;

    std::vector<Marker> mMarkers;
    std::vector<Fonts>   mFonts;
    std::vector<Chars>   mChars;
    VArenaAlloc         mArenaAlloc{2048};
    Stats               mStats;
};

class Transform : public Object {
public:
    struct Data {
        struct Extra {
            Property<float> m3DRx{0};
            Property<float> m3DRy{0};
            Property<float> m3DRz{0};
            Property<float> mSeparateX{0};
            Property<float> mSeparateY{0};
            bool            mSeparate{false};
            bool            m3DData{false};
        };
        VMatrix matrix(int frameNo, bool autoOrient = false) const;
        float   opacity(int frameNo) const
        {
            return mOpacity.value(frameNo) / 100.0f;
        }
        void createExtraData()
        {
            if (!mExtra) mExtra = std::make_unique<Extra>();
        }
        Property<float>        mRotation{0};       /* "r" */
        Property<VPointF>      mScale{{100, 100}}; /* "s" */
        Property<VPointF>      mPosition;          /* "p" */
        Property<VPointF>      mAnchor;            /* "a" */
        Property<float>        mOpacity{100};      /* "o" */
        std::unique_ptr<Extra> mExtra;
    };

    Transform() : Object(Object::Type::Transform) {}
    void set(Transform::Data *data, bool staticFlag)
    {
        setStatic(staticFlag);
        if (isStatic()) {
            new (&impl.mStaticData)
                StaticData(data->matrix(0), data->opacity(0));
        } else {
            impl.mData = data;
        }
    }
    VMatrix matrix(int frameNo, bool autoOrient = false) const
    {
        if (isStatic()) return impl.mStaticData.mMatrix;
        return impl.mData->matrix(frameNo, autoOrient);
    }
    float opacity(int frameNo) const
    {
        if (isStatic()) return impl.mStaticData.mOpacity;
        return impl.mData->opacity(frameNo);
    }
    Transform(const Transform &) = delete;
    Transform(Transform &&) = delete;
    Transform &operator=(Transform &) = delete;
    Transform &operator=(Transform &&) = delete;
    ~Transform() { destroy(); }

private:
    void destroy()
    {
        if (isStatic()) {
            impl.mStaticData.~StaticData();
        }
    }
    struct StaticData {
        StaticData(VMatrix &&m, float opacity)
            : mOpacity(opacity), mMatrix(std::move(m))
        {
        }
        float   mOpacity;
        VMatrix mMatrix;
    };
    union details {
        Data *     mData{nullptr};
        StaticData mStaticData;
        details(){};
        details(const details &) = delete;
        details(details &&) = delete;
        details &operator=(details &&) = delete;
        details &operator=(const details &) = delete;
        ~details(){};
    } impl;
};

class Group : public Object {
public:
    Group() : Object(Object::Type::Group) {}
    explicit Group(Object::Type type) : Object(type) {}

public:
    std::vector<Object *> mChildren;
    Transform *           mTransform{nullptr};
};


enum class Justification {
    Left,
    Right,
    Center
};

class TextProperties
{
public:
    int                 mSize{0};                   /* "s" */
    std::string         mFont;                      /* "f" */
    std::string         mText;                      /* "t" */
    Justification mJustification{Justification::Left}; /* "j" */
    float               mTracking{0.0};             /* "tr" */
    float               mLineHeight{0.0};           /* "lh" */
    float               mBaselineShift{0.0};        /* "ls" */
    Color         mFillColor;                 /* "fc" */
    Color         mStrokeColor;               /* "sc" */
    float               mStrokeWidth{0.0};          /* "sw" */
    bool                mStrokeOverFill{false};     /* "of" */

    inline bool operator==(const TextProperties &a) {
         if ((mSize == a.mSize) &&
	     (mFont.compare(a.mFont) == 0) &&
             (mText.compare(a.mText) == 0) &&
             (mJustification == a.mJustification) &&
             vCompare(mTracking, a.mTracking) &&
             vCompare(mLineHeight, a.mLineHeight) &&
             vCompare(mBaselineShift, a.mBaselineShift) &&
             (mFillColor == a.mFillColor) &&
             (mStrokeColor == a.mStrokeColor) &&
             vCompare(mStrokeWidth, a.mStrokeWidth) &&
             (mStrokeOverFill == a.mStrokeOverFill))
             return true;
	 return false;
    }
};

class TextDocument
{
public:
    TextProperties mTextProperties;
    int               mTime;
};

class TextAnimator
{
public:
    std::string            mName;

    /* Animated Properties */
    Property<float>   mOpacity{100};
    Property<float>   mRotation;
    Property<VPointF> mTracking;

    /* Range Selection */
    int                    mTime;
};

class TextLayerData
{

private:
    TextProperties textProperties(int frameNo) {
        for (auto &textDocument : mTextDocument) {
            if (textDocument.mTime >= frameNo)
                return textDocument.mTextProperties;
            }
        return mTextDocument.back().mTextProperties;
    }

public:
    std::vector<TextDocument>       mTextDocument;
    std::vector<TextAnimator>       mTextAnimator;

    TextProperties getTextProperties(int frameNo) {
        return textProperties(frameNo);
    }

    Color getTextFillColor(int frameNo) {
        if (mTextAnimator.empty()) {
            TextProperties textProp = getTextProperties(frameNo);
            return textProp.mFillColor;
        } else {
            // TODO: get Animatable fill color.
            return Color(0.0, 1.0, 0.0);
        }
    }

    float getTextStrokeWidth(int frameNo) {
        if (mTextAnimator.empty()) {
            TextProperties textProp = getTextProperties(frameNo);
            return textProp.mStrokeWidth;
        } else {
            // TODO: get Animatable stroke width.
            return 0.0;
        }
    }

    Color getTextStrokeColor(int frameNo) {
        if (mTextAnimator.empty()) {
            TextProperties textProp = getTextProperties(frameNo);
            return textProp.mStrokeColor;
        } else {
            // TODO: get Animatable stroke color.
            return Color(0.0, 1.0, 0.0);
        }
    }

    bool getTextStrokeOverFill(int frameNo) {
        return getTextProperties(frameNo).mStrokeOverFill;
    }

    int getTextOpacity(int frameNo) {
        if (mTextAnimator.empty()) {
            return 100;
        } else {
            // TODO: get Animatable opacity.
            return 50;
        }
    }
};



class Layer : public Group {
public:
    enum class Type : uchar {
        Precomp = 0,
        Solid = 1,
        Image = 2,
        Null = 3,
        Shape = 4,
        Text = 5
    };
    Layer() : Group(Object::Type::Layer) {}
    bool    hasPathOperator() const noexcept { return mHasPathOperator; }
    bool    hasGradient() const noexcept { return mHasGradient; }
    bool    hasMask() const noexcept { return mHasMask; }
    bool    hasRepeater() const noexcept { return mHasRepeater; }
    int     id() const noexcept { return mId; }
    int     parentId() const noexcept { return mParentId; }
    bool    hasParent() const noexcept { return mParentId != -1; }
    int     inFrame() const noexcept { return mInFrame; }
    int     outFrame() const noexcept { return mOutFrame; }
    int     startFrame() const noexcept { return mStartFrame; }
    Color   solidColor() const noexcept { return mExtra->mSolidColor; }
    bool    autoOrient() const noexcept { return mAutoOrient; }
    int     timeRemap(int frameNo) const;
    VSize   layerSize() const { return mLayerSize; }
    bool    precompLayer() const { return mLayerType == Type::Precomp; }
    VMatrix matrix(int frameNo) const
    {
        return mTransform ? mTransform->matrix(frameNo, autoOrient())
                          : VMatrix{};
    }
    float opacity(int frameNo) const
    {
        return mTransform ? mTransform->opacity(frameNo) : 1.0f;
    }
    Asset *asset() const
    {
        return (mExtra && mExtra->mAsset) ? mExtra->mAsset : nullptr;
    }
    struct Extra {
        Color               mSolidColor;
        std::string         mPreCompRefId;
        Property<float>     mTimeRemap; /* "tm" */
        Composition *       mCompRef{nullptr};
        Asset *             mAsset{nullptr};
        std::vector<Mask *> mMasks;
	std::unique_ptr<TextLayerData> mTextLayerData{nullptr};

	    TextLayerData* textLayer()
	    {
		if (!mTextLayerData) mTextLayerData = std::make_unique<TextLayerData>();
		return mTextLayerData.get();
	    }
    };

    Layer::Extra *extra()
    {
        if (!mExtra) mExtra = std::make_unique<Layer::Extra>();
        return mExtra.get();
    }

public:
    MatteType mMatteType{MatteType::None};
    Type      mLayerType{Layer::Type::Null};
    BlendMode mBlendMode{BlendMode::Normal};
    bool      mHasPathOperator{false};
    bool      mHasMask{false};
    bool      mHasRepeater{false};
    bool      mHasGradient{false};
    bool      mAutoOrient{false};
    VSize     mLayerSize;
    int       mParentId{-1};  // Lottie the id of the parent in the composition
    int       mId{-1};        // Lottie the group id  used for parenting.
    float     mTimeStreatch{1.0f};
    int       mInFrame{0};
    int       mOutFrame{0};
    int       mStartFrame{0};
    std::unique_ptr<Extra> mExtra{nullptr};
};

/**
 * TimeRemap has the value in time domain(in sec)
 * To get the proper mapping first we get the mapped time at the current frame
 * Number then we need to convert mapped time to frame number using the
 * composition time line Ex: at frame 10 the mappend time is 0.5(500 ms) which
 * will be convert to frame number 30 if the frame rate is 60. or will result to
 * frame number 15 if the frame rate is 30.
 */
inline int Layer::timeRemap(int frameNo) const
{
    /*
     * only consider startFrame() when there is no timeRemap.
     * when a layer has timeremap bodymovin updates the startFrame()
     * of all child layer so we don't have to take care of it.
     */
    if (!mExtra || mExtra->mTimeRemap.isStatic())
        frameNo = frameNo - startFrame();
    else
        frameNo =
            mExtra->mCompRef->frameAtTime(mExtra->mTimeRemap.value(frameNo));
    /* Apply time streatch if it has any.
     * Time streatch is just a factor by which the animation will speedup or
     * slow down with respect to the overal animation. Time streach factor is
     * already applied to the layers inFrame and outFrame.
     * @TODO need to find out if timestreatch also affects the in and out frame
     * of the child layers or not. */
    return int(frameNo / mTimeStreatch);
}

class Stroke : public Object {
public:
    Stroke() : Object(Object::Type::Stroke) {}
    Color color(int frameNo) const { return mColor.value(frameNo); }
    float opacity(int frameNo) const
    {
        return mOpacity.value(frameNo) / 100.0f;
    }
    float     strokeWidth(int frameNo) const { return mWidth.value(frameNo); }
    CapStyle  capStyle() const { return mCapStyle; }
    JoinStyle joinStyle() const { return mJoinStyle; }
    float     miterLimit() const { return mMiterLimit; }
    bool      hasDashInfo() const { return !mDash.empty(); }
    void      getDashInfo(int frameNo, std::vector<float> &result) const
    {
        return mDash.getDashInfo(frameNo, result);
    }

public:
    Property<Color> mColor;                       /* "c" */
    Property<float> mOpacity{100};                /* "o" */
    Property<float> mWidth{0};                    /* "w" */
    CapStyle        mCapStyle{CapStyle::Flat};    /* "lc" */
    JoinStyle       mJoinStyle{JoinStyle::Miter}; /* "lj" */
    float           mMiterLimit{0};               /* "ml" */
    Dash            mDash;
    bool            mEnabled{true}; /* "fillEnabled" */
};

class Gradient : public Object {
public:
    class Data {
    public:
        friend inline Gradient::Data operator+(const Gradient::Data &g1,
                                               const Gradient::Data &g2);
        friend inline Gradient::Data operator-(const Gradient::Data &g1,
                                               const Gradient::Data &g2);
        friend inline Gradient::Data operator*(float                 m,
                                               const Gradient::Data &g);

    public:
        std::vector<float> mGradient;
    };
    explicit Gradient(Object::Type type) : Object(type) {}
    inline float opacity(int frameNo) const
    {
        return mOpacity.value(frameNo) / 100.0f;
    }
    void update(std::unique_ptr<VGradient> &grad, int frameNo);

private:
    void populate(VGradientStops &stops, int frameNo);

public:
    int                      mGradientType{1};    /* "t" Linear=1 , Radial = 2*/
    Property<VPointF>        mStartPoint;         /* "s" */
    Property<VPointF>        mEndPoint;           /* "e" */
    Property<float>          mHighlightLength{0}; /* "h" */
    Property<float>          mHighlightAngle{0};  /* "a" */
    Property<float>          mOpacity{100};       /* "o" */
    Property<Gradient::Data> mGradient;           /* "g" */
    int                      mColorPoints{-1};
    bool                     mEnabled{true}; /* "fillEnabled" */
};

class GradientStroke : public Gradient {
public:
    GradientStroke() : Gradient(Object::Type::GStroke) {}
    float     width(int frameNo) const { return mWidth.value(frameNo); }
    CapStyle  capStyle() const { return mCapStyle; }
    JoinStyle joinStyle() const { return mJoinStyle; }
    float     miterLimit() const { return mMiterLimit; }
    bool      hasDashInfo() const { return !mDash.empty(); }
    void      getDashInfo(int frameNo, std::vector<float> &result) const
    {
        return mDash.getDashInfo(frameNo, result);
    }

public:
    Property<float> mWidth;                       /* "w" */
    CapStyle        mCapStyle{CapStyle::Flat};    /* "lc" */
    JoinStyle       mJoinStyle{JoinStyle::Miter}; /* "lj" */
    float           mMiterLimit{0};               /* "ml" */
    Dash            mDash;
};

class GradientFill : public Gradient {
public:
    GradientFill() : Gradient(Object::Type::GFill) {}
    FillRule fillRule() const { return mFillRule; }

public:
    FillRule mFillRule{FillRule::Winding}; /* "r" */
};

class Fill : public Object {
public:
    Fill() : Object(Object::Type::Fill) {}
    Color color(int frameNo) const { return mColor.value(frameNo); }
    float opacity(int frameNo) const
    {
        return mOpacity.value(frameNo) / 100.0f;
    }
    FillRule fillRule() const { return mFillRule; }

public:
    FillRule        mFillRule{FillRule::Winding}; /* "r" */
    bool            mEnabled{true};               /* "fillEnabled" */
    Property<Color> mColor;                       /* "c" */
    Property<float> mOpacity{100};                /* "o" */
};

class Shape : public Object {
public:
    explicit Shape(Object::Type type) : Object(type) {}
    VPath::Direction direction()
    {
        return (mDirection == 3) ? VPath::Direction::CCW : VPath::Direction::CW;
    }

public:
    int mDirection{1};
};

class Path : public Shape {
public:
    Path() : Shape(Object::Type::Path) {}

public:
    Property<PathData> mShape;
};

class Rect : public Shape {
public:
    Rect() : Shape(Object::Type::Rect) {}

public:
    Property<VPointF> mPos;
    Property<VPointF> mSize;
    Property<float>   mRound{0};
};

class Ellipse : public Shape {
public:
    Ellipse() : Shape(Object::Type::Ellipse) {}

public:
    Property<VPointF> mPos;
    Property<VPointF> mSize;
};

class Polystar : public Shape {
public:
    enum class PolyType { Star = 1, Polygon = 2 };
    Polystar() : Shape(Object::Type::Polystar) {}

public:
    Polystar::PolyType mPolyType{PolyType::Polygon};
    Property<VPointF>  mPos;
    Property<float>    mPointCount{0};
    Property<float>    mInnerRadius{0};
    Property<float>    mOuterRadius{0};
    Property<float>    mInnerRoundness{0};
    Property<float>    mOuterRoundness{0};
    Property<float>    mRotation{0};
};

class Repeater : public Object {
public:
    struct Transform {
        VMatrix matrix(int frameNo, float multiplier) const;
        float   startOpacity(int frameNo) const
        {
            return mStartOpacity.value(frameNo) / 100;
        }
        float endOpacity(int frameNo) const
        {
            return mEndOpacity.value(frameNo) / 100;
        }
        bool isStatic() const
        {
            return mRotation.isStatic() && mScale.isStatic() &&
                   mPosition.isStatic() && mAnchor.isStatic() &&
                   mStartOpacity.isStatic() && mEndOpacity.isStatic();
        }
        Property<float>   mRotation{0};       /* "r" */
        Property<VPointF> mScale{{100, 100}}; /* "s" */
        Property<VPointF> mPosition;          /* "p" */
        Property<VPointF> mAnchor;            /* "a" */
        Property<float>   mStartOpacity{100}; /* "so" */
        Property<float>   mEndOpacity{100};   /* "eo" */
    };
    Repeater() : Object(Object::Type::Repeater) {}
    Group *content() const { return mContent ? mContent : nullptr; }
    void   setContent(Group *content) { mContent = content; }
    int    maxCopies() const { return int(mMaxCopies); }
    float  copies(int frameNo) const { return mCopies.value(frameNo); }
    float  offset(int frameNo) const { return mOffset.value(frameNo); }
    bool   processed() const { return mProcessed; }
    void   markProcessed() { mProcessed = true; }

public:
    Group *         mContent{nullptr};
    Transform       mTransform;
    Property<float> mCopies{0};
    Property<float> mOffset{0};
    float           mMaxCopies{0.0};
    bool            mProcessed{false};
};

class Trim : public Object {
public:
    struct Segment {
        float start{0};
        float end{0};
        Segment() = default;
        explicit Segment(float s, float e) : start(s), end(e) {}
    };
    enum class TrimType { Simultaneously, Individually };
    Trim() : Object(Object::Type::Trim) {}
    /*
     * if start > end vector trims the path as a loop ( 2 segment)
     * if start < end vector trims the path without loop ( 1 segment).
     * if no offset then there is no loop.
     */
    Segment segment(int frameNo) const
    {
        float start = mStart.value(frameNo) / 100.0f;
        float end = mEnd.value(frameNo) / 100.0f;
        float offset = std::fmod(mOffset.value(frameNo), 360.0f) / 360.0f;

        float diff = std::abs(start - end);
        if (vCompare(diff, 0.0f)) return Segment(0, 0);
        if (vCompare(diff, 1.0f)) return Segment(0, 1);

        if (offset > 0) {
            start += offset;
            end += offset;
            if (start <= 1 && end <= 1) {
                return noloop(start, end);
            } else if (start > 1 && end > 1) {
                return noloop(start - 1, end - 1);
            } else {
                return (start > 1) ? loop(start - 1, end)
                                   : loop(start, end - 1);
            }
        } else {
            start += offset;
            end += offset;
            if (start >= 0 && end >= 0) {
                return noloop(start, end);
            } else if (start < 0 && end < 0) {
                return noloop(1 + start, 1 + end);
            } else {
                return (start < 0) ? loop(1 + start, end)
                                   : loop(start, 1 + end);
            }
        }
    }
    Trim::TrimType type() const { return mTrimType; }

private:
    Segment noloop(float start, float end) const
    {
        assert(start >= 0);
        assert(end >= 0);
        Segment s;
        s.start = std::min(start, end);
        s.end = std::max(start, end);
        return s;
    }
    Segment loop(float start, float end) const
    {
        assert(start >= 0);
        assert(end >= 0);
        Segment s;
        s.start = std::max(start, end);
        s.end = std::min(start, end);
        return s;
    }

public:
    Property<float> mStart{0};
    Property<float> mEnd{0};
    Property<float> mOffset{0};
    Trim::TrimType  mTrimType{TrimType::Simultaneously};
};

inline Gradient::Data operator+(const Gradient::Data &g1,
                                const Gradient::Data &g2)
{
    if (g1.mGradient.size() != g2.mGradient.size()) return g1;

    Gradient::Data newG;
    newG.mGradient = g1.mGradient;

    auto g2It = g2.mGradient.begin();
    for (auto &i : newG.mGradient) {
        i = i + *g2It;
        g2It++;
    }

    return newG;
}

inline Gradient::Data operator-(const Gradient::Data &g1,
                                const Gradient::Data &g2)
{
    if (g1.mGradient.size() != g2.mGradient.size()) return g1;
    Gradient::Data newG;
    newG.mGradient = g1.mGradient;

    auto g2It = g2.mGradient.begin();
    for (auto &i : newG.mGradient) {
        i = i - *g2It;
        g2It++;
    }

    return newG;
}

inline Gradient::Data operator*(float m, const Gradient::Data &g)
{
    Gradient::Data newG;
    newG.mGradient = g.mGradient;

    for (auto &i : newG.mGradient) {
        i = i * m;
    }
    return newG;
}

using ColorFilter = std::function<void(float &, float &, float &)>;

void configureModelCacheSize(size_t cacheSize);

std::shared_ptr<model::Composition> loadFromFile(const std::string &filePath,
                                                 bool cachePolicy);

std::shared_ptr<model::Composition> loadFromData(std::string        jsonData,
                                                 const std::string &key,
                                                 std::string resourcePath,
                                                 bool        cachePolicy);

std::shared_ptr<model::Composition> loadFromData(std::string jsonData,
                                                 std::string resourcePath,
                                                 ColorFilter filter);

std::shared_ptr<model::Composition> parse(char *str, std::string dir_path,
                                          ColorFilter filter = {});

}  // namespace model

}  // namespace internal

}  // namespace rlottie

#endif  // LOTModel_H
