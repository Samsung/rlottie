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

#include "lottieparser.h"

//#define DEBUG_PARSER

// This parser implements JSON token-by-token parsing with an API that is
// more direct; we don't have to create  handler object and
// callbacks. Instead, we retrieve values from the JSON stream by calling
// GetInt(), GetDouble(), GetString() and GetBool(), traverse into structures
// by calling EnterObject() and EnterArray(), and skip over unwanted data by
// calling SkipValue(). As we know the lottie file structure this way will be
// the efficient way of parsing the file.
//
// If you aren't sure of what's next in the JSON data, you can use PeekType()
// and PeekValue() to look ahead to the next object before reading it.
//
// If you call the wrong retrieval method--e.g. GetInt when the next JSON token
// is not an int, EnterObject or EnterArray when there isn't actually an object
// or array to read--the stream parsing will end immediately and no more data
// will be delivered.
//
// After calling EnterObject, you retrieve keys via NextObjectKey() and values
// via the normal getters. When NextObjectKey() returns null, you have exited
// the object, or you can call SkipObject() to skip to the end of the object
// immediately. If you fetch the entire object (i.e. NextObjectKey() returned
// null), you should not call SkipObject().
//
// After calling EnterArray(), you must alternate between calling
// NextArrayValue() to see if the array has more data, and then retrieving
// values via the normal getters. You can call SkipArray() to skip to the end of
// the array immediately. If you fetch the entire array (i.e. NextArrayValue()
// returned null), you should not call SkipArray().
//
// This parser uses in-situ strings, so the JSON buffer will be altered during
// the parse.

#include <array>

#include "lottiemodel.h"
#include "rapidjson/document.h"

RAPIDJSON_DIAG_PUSH
#ifdef __GNUC__
RAPIDJSON_DIAG_OFF(effc++)
#endif

using namespace rapidjson;

class LookaheadParserHandler {
public:
    bool Null()
    {
        st_ = kHasNull;
        v_.SetNull();
        return true;
    }
    bool Bool(bool b)
    {
        st_ = kHasBool;
        v_.SetBool(b);
        return true;
    }
    bool Int(int i)
    {
        st_ = kHasNumber;
        v_.SetInt(i);
        return true;
    }
    bool Uint(unsigned u)
    {
        st_ = kHasNumber;
        v_.SetUint(u);
        return true;
    }
    bool Int64(int64_t i)
    {
        st_ = kHasNumber;
        v_.SetInt64(i);
        return true;
    }
    bool Uint64(int64_t u)
    {
        st_ = kHasNumber;
        v_.SetUint64(u);
        return true;
    }
    bool Double(double d)
    {
        st_ = kHasNumber;
        v_.SetDouble(d);
        return true;
    }
    bool RawNumber(const char *, SizeType, bool) { return false; }
    bool String(const char *str, SizeType length, bool)
    {
        st_ = kHasString;
        v_.SetString(str, length);
        return true;
    }
    bool StartObject()
    {
        st_ = kEnteringObject;
        return true;
    }
    bool Key(const char *str, SizeType length, bool)
    {
        st_ = kHasKey;
        v_.SetString(str, length);
        return true;
    }
    bool EndObject(SizeType)
    {
        st_ = kExitingObject;
        return true;
    }
    bool StartArray()
    {
        st_ = kEnteringArray;
        return true;
    }
    bool EndArray(SizeType)
    {
        st_ = kExitingArray;
        return true;
    }

protected:
    explicit LookaheadParserHandler(char *str);

protected:
    enum LookaheadParsingState {
        kInit,
        kError,
        kHasNull,
        kHasBool,
        kHasNumber,
        kHasString,
        kHasKey,
        kEnteringObject,
        kExitingObject,
        kEnteringArray,
        kExitingArray
    };

    Value                 v_;
    LookaheadParsingState st_;
    Reader                r_;
    InsituStringStream    ss_;

    static const int parseFlags = kParseDefaultFlags | kParseInsituFlag;
};

class LottieParserImpl : public LookaheadParserHandler {
public:
    LottieParserImpl(char *str, std::string dir_path, ColorFilter filter)
        : LookaheadParserHandler(str),
          mColorFilter(std::move(filter)),
          mDirPath(std::move(dir_path)) {}
    bool VerifyType();
    bool ParseNext();
public:
    VArenaAlloc& allocator() {return compRef->mArenaAlloc;}
    bool        EnterObject();
    bool        EnterArray();
    const char *NextObjectKey();
    bool        NextArrayValue();
    int         GetInt();
    double      GetDouble();
    const char *GetString();
    bool        GetBool();
    void        GetNull();

    void   SkipObject();
    void   SkipArray();
    void   SkipValue();
    Value *PeekValue();
    int PeekType() const;
    bool IsValid() { return st_ != kError; }

    void                  Skip(const char *key);
    LottieBlendMode       getBlendMode();
    CapStyle              getLineCap();
    JoinStyle             getLineJoin();
    FillRule              getFillRule();
    LOTTrimData::TrimType getTrimType();
    MatteType             getMatteType();
    LayerType             getLayerType();

    std::shared_ptr<LOTCompositionData> composition() const
    {
        return mComposition;
    }
    void                         parseComposition();
    void                         parseMarkers();
    void                         parseMarker();
    void                         parseAssets(LOTCompositionData *comp);
    LOTAsset*                    parseAsset();
    void                         parseLayers(LOTCompositionData *comp);
    LOTLayerData*                parseLayer();
    void                         parseFonts();
    void                         parseFont();
    void                         parseChars();
    void                         parseChar();
    void                         parseCharData(LOTChars &obj);
    void                         parseCharDataShape(VPath &obj);
    void                         parseMaskProperty(LOTLayerData *layer);
    void                         parseShapesAttr(LOTLayerData *layer);
    void                         parseShapesAttr(LOTShapeGroupData *obj);
    void                         parseObject(LOTGroupData *parent);
    LOTMaskData*                 parseMaskObject();
    LOTData*                     parseObjectTypeAttr();
    LOTData*                     parseGroupObject();
    LOTRectData*                 parseRectObject();
    LOTEllipseData*              parseEllipseObject();
    LOTShapeData*                parseShapeObject();
    LOTPolystarData*             parsePolystarObject();

    LOTTransformData*            parseTransformObject(bool ddd = false);
    LOTFillData*                 parseFillObject();
    LOTGFillData*                parseGFillObject();
    LOTStrokeData*               parseStrokeObject();
    LOTGStrokeData*              parseGStrokeObject();
    LOTTrimData*                 parseTrimObject();
    LOTRepeaterData*             parseReapeaterObject();

    void parseGradientProperty(LOTGradient *gradient, const char *key);

    VPointF parseInperpolatorPoint();

    void getValue(VPointF &pt);
    void getValue(float &fval);
    void getValue(LottieColor &color);
    void getValue(int &ival);
    void getValue(LottieShapeData &shape);
    void getValue(LottieGradient &gradient);
    void getValue(std::vector<VPointF> &v);
    void getValue(LOTRepeaterTransform &);

    template <typename T>
    bool parseKeyFrameValue(const char *key, LOTKeyFrameValue<T> &value);
    template <typename T>
    void parseKeyFrame(LOTAnimInfo<T> &obj);
    template <typename T>
    void parseProperty(LOTAnimatable<T> &obj);
    template <typename T>
    void parsePropertyHelper(LOTAnimatable<T> &obj);

    void parseShapeProperty(LOTAnimatable<LottieShapeData> &obj);
    void parseDashProperty(LOTDashProperty &dash);

    void parseTextProperties(LOTTextDocument &obj);
    void parseTextAnimatedProperties(LOTTextAnimator &obj);
    void parseTextRangeSelection(LOTTextAnimator &obj);
    void parseTextDocument(LOTTextLayerData *obj);
    void parseTextAnimators(LOTTextLayerData *obj);
    void parseText(LOTTextLayerData *obj);

    VInterpolator* interpolator(VPointF, VPointF, std::string);

    LottieColor toColor(const char *str);

    void resolveLayerRefs();
    void parsePathInfo();
private:
    ColorFilter mColorFilter;
    struct {
        std::vector<VPointF>                       mInPoint;  /* "i" */
        std::vector<VPointF>                       mOutPoint; /* "o" */
        std::vector<VPointF>                       mVertices; /* "v" */
        std::vector<VPointF>                       mResult;
        bool                                       mClosed{false};

        void convert() {
            // shape data could be empty.
            if (mInPoint.empty() || mOutPoint.empty() || mVertices.empty()) {
                mResult.clear();
                return;
            }

            /*
             * Convert the AE shape format to
             * list of bazier curves
             * The final structure will be Move +size*Cubic + Cubic (if the path is
             * closed one)
             */
            if (mInPoint.size() != mOutPoint.size() ||
                mInPoint.size() != mVertices.size()) {
                mResult.clear();
            } else {
                auto size = mVertices.size();
                mResult.push_back(mVertices[0]);
                for (size_t i = 1; i < size; i++) {
                    mResult.push_back(mVertices[i - 1] +
                                     mOutPoint[i - 1]);  // CP1 = start + outTangent
                    mResult.push_back(mVertices[i] +
                                     mInPoint[i]);   // CP2 = end + inTangent
                    mResult.push_back(mVertices[i]);  // end point
                }

                if (mClosed) {
                    mResult.push_back(mVertices[size - 1] +
                                     mOutPoint[size - 1]);  // CP1 = start + outTangent
                    mResult.push_back(mVertices[0] +
                                     mInPoint[0]);   // CP2 = end + inTangent
                    mResult.push_back(mVertices[0]);  // end point
                }
            }
        }
        void reset() {
            mInPoint.clear();
            mOutPoint.clear();
            mVertices.clear();
            mResult.clear();
            mClosed = false;
        }
       void updatePath(VPath &out) {

            if (mResult.empty()) return;

            auto size = mResult.size();
            auto points = mResult.data();
            /* reserve exact memory requirement at once
             * ptSize = size + 1(size + close)
             * elmSize = size/3 cubic + 1 move + 1 close
             */
            out.reserve(size + 1 , size/3 + 2);
            out.moveTo(points[0]);
            for (size_t i = 1 ; i < size; i+=3) {
               out.cubicTo(points[i], points[i+1], points[i+2]);
            }
            if (mClosed)
              out.close();
       }
    }mPathInfo;
protected:
    std::unordered_map<std::string, VInterpolator*>
                                               mInterpolatorCache;
    std::shared_ptr<LOTCompositionData>        mComposition;
    LOTCompositionData *                       compRef{nullptr};
    LOTLayerData *                             curLayerRef{nullptr};
    std::vector<LOTLayerData *>                mLayersToUpdate;
    std::string                                mDirPath;
    void                                       SkipOut(int depth);
};

LookaheadParserHandler::LookaheadParserHandler(char *str)
    : v_(), st_(kInit), ss_(str)
{
    r_.IterativeParseInit();
}

bool LottieParserImpl::VerifyType()
{
    /* Verify the media type is lottie json.
       Could add more strict check. */
    return ParseNext();
}

bool LottieParserImpl::ParseNext()
{
    if (r_.HasParseError()) {
        st_ = kError;
        return false;
    }

    if (!r_.IterativeParseNext<parseFlags>(ss_, *this)) {
        vCritical << "Lottie file parsing error";
        st_ = kError;
        return false;
    }
    return true;
}

bool LottieParserImpl::EnterObject()
{
    if (st_ != kEnteringObject) {
        st_ = kError;
        RAPIDJSON_ASSERT(false);
        return false;
    }

    ParseNext();
    return true;
}

bool LottieParserImpl::EnterArray()
{
    if (st_ != kEnteringArray) {
        st_ = kError;
        RAPIDJSON_ASSERT(false);
        return false;
    }

    ParseNext();
    return true;
}

const char *LottieParserImpl::NextObjectKey()
{
    if (st_ == kHasKey) {
        const char *result = v_.GetString();
        ParseNext();
        return result;
    }

    /* SPECIAL CASE
     * The parser works with a prdefined rule that it will be only
     * while (NextObjectKey()) for each object but in case of our nested group
     * object we can call multiple time NextObjectKey() while exiting the object
     * so ignore those and don't put parser in the error state.
     * */
    if (st_ == kExitingArray || st_ == kEnteringObject) {
        // #ifdef DEBUG_PARSER
        //         vDebug<<"Object: Exiting nested loop";
        // #endif
        return nullptr;
    }

    if (st_ != kExitingObject) {
        RAPIDJSON_ASSERT(false);
        st_ = kError;
        return nullptr;
    }

    ParseNext();
    return nullptr;
}

bool LottieParserImpl::NextArrayValue()
{
    if (st_ == kExitingArray) {
        ParseNext();
        return false;
    }

    /* SPECIAL CASE
     * same as  NextObjectKey()
     */
    if (st_ == kExitingObject) {
        return false;
    }

    if (st_ == kError || st_ == kHasKey) {
        RAPIDJSON_ASSERT(false);
        st_ = kError;
        return false;
    }

    return true;
}

int LottieParserImpl::GetInt()
{
    if (st_ != kHasNumber || !v_.IsInt()) {
        st_ = kError;
        RAPIDJSON_ASSERT(false);
        return 0;
    }

    int result = v_.GetInt();
    ParseNext();
    return result;
}

double LottieParserImpl::GetDouble()
{
    if (st_ != kHasNumber) {
        st_ = kError;
        RAPIDJSON_ASSERT(false);
        return 0.;
    }

    double result = v_.GetDouble();
    ParseNext();
    return result;
}

bool LottieParserImpl::GetBool()
{
    if (st_ != kHasBool) {
        st_ = kError;
        RAPIDJSON_ASSERT(false);
        return false;
    }

    bool result = v_.GetBool();
    ParseNext();
    return result;
}

void LottieParserImpl::GetNull()
{
    if (st_ != kHasNull) {
        st_ = kError;
        return;
    }

    ParseNext();
}

const char *LottieParserImpl::GetString()
{
    if (st_ != kHasString) {
        st_ = kError;
        RAPIDJSON_ASSERT(false);
        return nullptr;
    }

    const char *result = v_.GetString();
    ParseNext();
    return result;
}

void LottieParserImpl::SkipOut(int depth)
{
    do {
        if (st_ == kEnteringArray || st_ == kEnteringObject) {
            ++depth;
        } else if (st_ == kExitingArray || st_ == kExitingObject) {
            --depth;
        } else if (st_ == kError) {
            RAPIDJSON_ASSERT(false);
            return;
        }

        ParseNext();
    } while (depth > 0);
}

void LottieParserImpl::SkipValue()
{
    SkipOut(0);
}

void LottieParserImpl::SkipArray()
{
    SkipOut(1);
}

void LottieParserImpl::SkipObject()
{
    SkipOut(1);
}

Value *LottieParserImpl::PeekValue()
{
    if (st_ >= kHasNull && st_ <= kHasKey) {
        return &v_;
    }

    return nullptr;
}

// returns a rapidjson::Type, or -1 for no value (at end of
// object/array)
int LottieParserImpl::PeekType() const
{
    if (st_ >= kHasNull && st_ <= kHasKey) {
        return v_.GetType();
    }

    if (st_ == kEnteringArray) {
        return kArrayType;
    }

    if (st_ == kEnteringObject) {
        return kObjectType;
    }

    return -1;
}

void LottieParserImpl::Skip(const char * /*key*/)
{
    if (PeekType() == kArrayType) {
        EnterArray();
        SkipArray();
    } else if (PeekType() == kObjectType) {
        EnterObject();
        SkipObject();
    } else {
        SkipValue();
    }
}

LottieBlendMode LottieParserImpl::getBlendMode()
{
    RAPIDJSON_ASSERT(PeekType() == kNumberType);
    LottieBlendMode mode = LottieBlendMode::Normal;

    switch (GetInt()) {
    case 1:
        mode = LottieBlendMode::Multiply;
        break;
    case 2:
        mode = LottieBlendMode::Screen;
        break;
    case 3:
        mode = LottieBlendMode::OverLay;
        break;
    default:
        break;
    }
    return mode;
}

void LottieParserImpl::resolveLayerRefs()
{
    for (const auto &layer : mLayersToUpdate) {
        auto          search = compRef->mAssets.find(layer->extra()->mPreCompRefId);
        if (search != compRef->mAssets.end()) {
            if (layer->mLayerType == LayerType::Image) {
                layer->extra()->mAsset = search->second;
            } else if (layer->mLayerType == LayerType::Precomp) {
                layer->mChildren = search->second->mLayers;
                layer->setStatic(layer->isStatic() &&
                                 search->second->isStatic());
            }
        }
    }
}

void LottieParserImpl::parseComposition()
{
    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    EnterObject();
    std::shared_ptr<LOTCompositionData> sharedComposition =
        std::make_shared<LOTCompositionData>();
    LOTCompositionData *comp = sharedComposition.get();
    compRef = comp;
    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "v")) {
            RAPIDJSON_ASSERT(PeekType() == kStringType);
            comp->mVersion = std::string(GetString());
        } else if (0 == strcmp(key, "w")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            comp->mSize.setWidth(GetInt());
        } else if (0 == strcmp(key, "h")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            comp->mSize.setHeight(GetInt());
        } else if (0 == strcmp(key, "ip")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            comp->mStartFrame = GetDouble();
        } else if (0 == strcmp(key, "op")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            comp->mEndFrame = GetDouble();
        } else if (0 == strcmp(key, "fr")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            comp->mFrameRate = GetDouble();
        } else if (0 == strcmp(key, "assets")) {
            parseAssets(comp);
        } else if (0 == strcmp(key, "layers")) {
            parseLayers(comp);
        } else if (0 == strcmp(key, "markers")) {
            parseMarkers();
        } else if (0 == strcmp(key, "fonts")) {
             parseFonts();
        } else if (0 == strcmp(key, "chars")) {
             parseChars();
        } else {
#ifdef DEBUG_PARSER
            vWarning << "Composition Attribute Skipped : " << key;
#endif
            Skip(key);
        }
    }

    if (comp->mVersion.empty() || !comp->mRootLayer) {
        // don't have a valid bodymovin header
        return;
    }
    if (!IsValid()) {
        return;
    }

    resolveLayerRefs();
    comp->setStatic(comp->mRootLayer->isStatic());
    comp->mRootLayer->mInFrame = comp->mStartFrame;
    comp->mRootLayer->mOutFrame = comp->mEndFrame;

    mComposition = sharedComposition;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/layers/text.json
 */
void LottieParserImpl::parseTextProperties(LOTTextDocument &obj)
{
   RAPIDJSON_ASSERT(PeekType() == kObjectType);
   EnterObject();

   while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "s")) {
             RAPIDJSON_ASSERT(PeekType() == kNumberType);
             obj.mTextProperties.mSize = GetInt();
        } else if (0 == strcmp(key, "f")) {
             RAPIDJSON_ASSERT(PeekType() == kStringType);
             obj.mTextProperties.mFont = std::string(GetString());
        } else if (0 == strcmp(key, "t")) {
             RAPIDJSON_ASSERT(PeekType() == kStringType);
             obj.mTextProperties.mText = std::string(GetString());
        } else if (0 == strcmp(key, "j")) {
             RAPIDJSON_ASSERT(PeekType() == kNumberType);
             int j = GetInt();
             switch (j) {
                case 0:
                   obj.mTextProperties.mJustification = LEFT_ALIGN;
                   break;
                case 1:
                   obj.mTextProperties.mJustification = RIGHT_ALIGN;
                   break;
                case 2:
                default:
                   obj.mTextProperties.mJustification = CENTER_ALIGN;
                   break;
             }
        } else if (0 == strcmp(key, "tr")) {
             RAPIDJSON_ASSERT(PeekType() == kNumberType);
             obj.mTextProperties.mTracking = GetDouble();
        } else if (0 == strcmp(key, "lh")) {
             RAPIDJSON_ASSERT(PeekType() == kNumberType);
             obj.mTextProperties.mLineHeight = GetDouble();
        } else if (0 == strcmp(key, "ls")) {
             RAPIDJSON_ASSERT(PeekType() == kNumberType);
             obj.mTextProperties.mBaselineShift = GetDouble();
        } else if (0 == strcmp(key, "fc")) {
             getValue(obj.mTextProperties.mFillColor);
        } else if (0 == strcmp(key, "sc")) {
             getValue(obj.mTextProperties.mStrokeColor);
        } else if (0 == strcmp(key, "sw")) {
             RAPIDJSON_ASSERT(PeekType() == kNumberType);
             obj.mTextProperties.mStrokeWidth = GetDouble();
        } else if (0 == strcmp(key, "of")) {
             obj.mTextProperties.mStrokeOverFill = GetBool();
        } else {
             printf(".............. parseTextProperties key[%s] SKIPPED\n", key);
             Skip(key);
        }
   }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/layers/text.json
 */
void LottieParserImpl::parseTextDocument(LOTTextLayerData *obj)
{
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();

    obj->mTextDocument.emplace_back();
    LOTTextDocument &documentObj = obj->mTextDocument.back();

    while (NextArrayValue()) {
        RAPIDJSON_ASSERT(PeekType() == kObjectType);
        EnterObject();

        while (const char *key = NextObjectKey()) {
            if (0 == strcmp(key, "s")) {
                parseTextProperties(documentObj);
            } else if (0 == strcmp(key, "t")) {
                documentObj.mTime = GetInt();
            } else {
                Skip(key);
            }
        }
    }
}

void LottieParserImpl::parseTextAnimatedProperties(LOTTextAnimator &obj)
{
    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    EnterObject();

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "r")) {
            parseProperty(obj.mRotation);
        } else if (0 == strcmp(key, "o")) {
            parseProperty(obj.mOpacity);
        } else {
            Skip(key);
        }
    }
}

void LottieParserImpl::parseTextRangeSelection(LOTTextAnimator &obj)
{
    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    EnterObject();

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "a")){
            //parseProperty(??);
            Skip(key);
        } else if (0 == strcmp(key, "b")) {
            Skip(key);
        } else if (0 == strcmp(key, "t")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            obj.mTime = GetInt();
        } else {
            Skip(key);
        }
    }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/layers/text.json
 */
void LottieParserImpl::parseTextAnimators(LOTTextLayerData *obj)
{
   RAPIDJSON_ASSERT(PeekType() == kArrayType);
   EnterArray();

   while (NextArrayValue()) {
        RAPIDJSON_ASSERT(PeekType() == kObjectType);
        EnterObject();

        obj->mTextAnimator.emplace_back();
        LOTTextAnimator &animatorObj = obj->mTextAnimator.back();

        while (const char *key = NextObjectKey()) {
             if (0 == strcmp(key, "nm")) {
                  RAPIDJSON_ASSERT(PeekType() == kStringType);
                  animatorObj.mName = std::string(GetString());
             } else if (0 == strcmp(key, "a")) {
                  parseTextAnimatedProperties(animatorObj);
             } else if (0 == strcmp(key, "s")) {
                  parseTextRangeSelection(animatorObj);
             } else {
                  Skip(key);
             }
        }
   }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/layers/text.json
 */
void LottieParserImpl::parseText(LOTTextLayerData *obj)
{
   RAPIDJSON_ASSERT(PeekType() == kObjectType);
   EnterObject();

   while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "d")) {
             RAPIDJSON_ASSERT(PeekType() == kObjectType);
             EnterObject();

             while (const char *dKey = NextObjectKey()) {
                  if (0 == strcmp(dKey, "k")) {
                       parseTextDocument(obj);
                  } else {
                       Skip(dKey);
                  }
             }
        } else if (0 == strcmp(key, "a")) {
             parseTextAnimators(obj);
        } else if (0 == strcmp(key, "p")) {
             //printf("TEXT(p) ... SKIP! \n");
             Skip(key);
        } else if (0 == strcmp(key, "m")) {
             //printf("TEXT(m) ... SKIP! \n");
             Skip(key);
        } else {
             Skip(key);
        }
   }
}

void LottieParserImpl::parseFont()
{
   RAPIDJSON_ASSERT(PeekType() == kObjectType);
   EnterObject();

   compRef->mFonts.emplace_back();
   LOTFonts &fonts = compRef->mFonts.back();

   while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "fName")) {
             RAPIDJSON_ASSERT(PeekType() == kStringType);
             fonts.mFontName = std::string(GetString());
        } else if (0 == strcmp(key, "fFamily")) {
             RAPIDJSON_ASSERT(PeekType() == kStringType);
             fonts.mFontFamily = std::string(GetString());
        } else if (0 == strcmp(key, "fStyle")) {
             RAPIDJSON_ASSERT(PeekType() == kStringType);
             fonts.mFontStyle = std::string(GetString());
        } else if (0 == strcmp(key, "ascent")) {
             RAPIDJSON_ASSERT(PeekType() == kNumberType);
             fonts.mFontAscent = GetDouble();
        } else {
             printf("ERR! UNKNOWN KEYWORD for Font! key[%s] type[%d]\n", key, PeekType());
             Skip(key);
        }
   }
}

void LottieParserImpl::parseFonts()
{
   RAPIDJSON_ASSERT(PeekType() == kObjectType);
   EnterObject();

   while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "list")) {
             RAPIDJSON_ASSERT(PeekType() == kArrayType);
             EnterArray();
             while (NextArrayValue()) {
                  parseFont();
             }
        } else {
             printf("ERR! FONTS has NO LIST!\n");
             Skip(key);
        }
   }
}

void LottieParserImpl::parseCharDataShape(VPath &obj)
{
   RAPIDJSON_ASSERT(PeekType() == kObjectType);
   EnterObject();

   while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "it")) {
             RAPIDJSON_ASSERT(PeekType() == kArrayType);
             EnterArray();

             while (NextArrayValue()) {
                  RAPIDJSON_ASSERT(PeekType() == kObjectType);
                  EnterObject();

                  VPath pathItem;
                  while (const char *itKey = NextObjectKey()) {
                       if (0 == strcmp(itKey, "ks")) {
                            RAPIDJSON_ASSERT(PeekType() == kObjectType);
                            EnterObject();

                            while (const char *ksKey = NextObjectKey()) {
                                 if (0 == strcmp(ksKey, "k")) {
                                      LottieShapeData shapeData;

                                      getValue(shapeData);
                                      shapeData.toPath(pathItem);
                                      obj.addPath(pathItem);
                                 } else {
                                      Skip(ksKey);
                                 }
                            }
                       } else {
                            printf("parseCharDataShape - itKey[%s] = SKIP\n", itKey);
                            Skip(itKey);
                       }
                  }
             }
        } else {
             printf("parseCharDataShape - key[%s] = SKIP\n", key);
             Skip(key);
        }
   }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/sources/chars.json
 */
void LottieParserImpl::parseCharData(LOTChars &obj)
{
   RAPIDJSON_ASSERT(PeekType() == kObjectType);
   EnterObject();

   while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "shapes")) {
             // Instead of parsing a shape group,
             // parse a shape data and save as a VPath.
             //parseShapesAttr(obj.getShapesData());

             RAPIDJSON_ASSERT(PeekType() == kArrayType);
             EnterArray();
             while (NextArrayValue()) {
                  //obj.mShapesData.emplace_back();
                  //LottieShapeData &shapeDataObj = obj.mShapesData.back();
                  //
                  //parseCharDataShape(shapeDataObj);

                  obj.mShapePathData.emplace_back();
                  VPath &pathObj = obj.mShapePathData.back();

                  parseCharDataShape(pathObj);
             }
        } else {
             printf("parseCharData - UNKNOWN KEY!\n");
             Skip(key);
        }
   }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/sources/chars.json
 */
void LottieParserImpl::parseChar()
{
   RAPIDJSON_ASSERT(PeekType() == kObjectType);
   EnterObject();

   compRef->mChars.emplace_back();
   LOTChars &chars = compRef->mChars.back();

   while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "ch")) {
             RAPIDJSON_ASSERT(PeekType() == kStringType);
             chars.mCh = std::string(GetString());
        } else if (0 == strcmp(key, "size")) {
             RAPIDJSON_ASSERT(PeekType() == kNumberType);
             chars.mSize = GetDouble();
        } else if (0 == strcmp(key, "style")) {
             RAPIDJSON_ASSERT(PeekType() == kStringType);
             chars.mStyle = std::string(GetString());
        } else if (0 == strcmp(key, "w")) {
             RAPIDJSON_ASSERT(PeekType() == kNumberType);
             chars.mWidth = GetDouble();
        } else if (0 == strcmp(key, "data")) {
             parseCharData(chars);
        } else if (0 == strcmp(key, "fFamily")) {
             RAPIDJSON_ASSERT(PeekType() == kStringType);
             chars.mFontFamily = std::string(GetString());
        } else {
             Skip(key);
        }
   }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/sources/chars.json
 */
void LottieParserImpl::parseChars()
{
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();

    while (NextArrayValue()) {
		parseChar();
	}
}

void LottieParserImpl::parseMarker()
{
    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    EnterObject();
    std::string comment;
    int         timeframe{0};
    int          duration{0};
    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "cm")) {
            RAPIDJSON_ASSERT(PeekType() == kStringType);
            comment = std::string(GetString());
        } else if (0 == strcmp(key, "tm")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            timeframe = GetDouble();
        } else if (0 == strcmp(key, "dr")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            duration = GetDouble();

        } else {
#ifdef DEBUG_PARSER
            vWarning << "Marker Attribute Skipped : " << key;
#endif
            Skip(key);
        }
    }
    compRef->mMarkers.emplace_back(std::move(comment), timeframe, timeframe + duration);
}

void LottieParserImpl::parseMarkers()
{
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        parseMarker();
    }
    // update the precomp layers with the actual layer object
}

void LottieParserImpl::parseAssets(LOTCompositionData *composition)
{
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        auto asset = parseAsset();
        composition->mAssets[asset->mRefId] = asset;
    }
    // update the precomp layers with the actual layer object
}

static constexpr const unsigned char B64index[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  62, 63, 62, 62, 63, 52, 53, 54, 55, 56, 57,
    58, 59, 60, 61, 0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,
    7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 0,  0,  0,  0,  63, 0,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
    37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};

std::string b64decode(const char *data, const size_t len)
{
    auto p = reinterpret_cast<const unsigned char *>(data);
    int            pad = len > 0 && (len % 4 || p[len - 1] == '=');
    const size_t   L = ((len + 3) / 4 - pad) * 4;
    std::string    str(L / 4 * 3 + pad, '\0');

    for (size_t i = 0, j = 0; i < L; i += 4) {
        int n = B64index[p[i]] << 18 | B64index[p[i + 1]] << 12 |
                B64index[p[i + 2]] << 6 | B64index[p[i + 3]];
        str[j++] = n >> 16;
        str[j++] = n >> 8 & 0xFF;
        str[j++] = n & 0xFF;
    }
    if (pad) {
        int n = B64index[p[L]] << 18 | B64index[p[L + 1]] << 12;
        str[str.size() - 1] = n >> 16;

        if (len > L + 2 && p[L + 2] != '=') {
            n |= B64index[p[L + 2]] << 6;
            str.push_back(n >> 8 & 0xFF);
        }
    }
    return str;
}

static std::string convertFromBase64(const std::string &str)
{
    // usual header look like "data:image/png;base64,"
    // so need to skip till ','.
    size_t startIndex = str.find(",", 0);
    startIndex += 1;  // skip ","
    size_t length = str.length() - startIndex;

    const char *b64Data = str.c_str() + startIndex;

    return b64decode(b64Data, length);
}

/*
 *  std::to_string() function is missing in VS2017
 *  so this is workaround for windows build
 */
#include <sstream>
template<class T>
static std::string toString(const T &value) {
    std::ostringstream os;
    os << value;
    return os.str();
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/layers/shape.json
 *
 */
LOTAsset* LottieParserImpl::parseAsset()
{
    RAPIDJSON_ASSERT(PeekType() == kObjectType);

    auto                      asset = allocator().make<LOTAsset>();
    std::string               filename;
    std::string               relativePath;
    bool                      embededResource = false;
    EnterObject();
    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "w")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            asset->mWidth = GetInt();
        } else if (0 == strcmp(key, "h")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            asset->mHeight = GetInt();
        } else if (0 == strcmp(key, "p")) { /* image name */
            asset->mAssetType = LOTAsset::Type::Image;
            RAPIDJSON_ASSERT(PeekType() == kStringType);
            filename = std::string(GetString());
        } else if (0 == strcmp(key, "u")) { /* relative image path */
            RAPIDJSON_ASSERT(PeekType() == kStringType);
            relativePath = std::string(GetString());
        } else if (0 == strcmp(key, "e")) { /* relative image path */
            embededResource = GetInt();
        } else if (0 == strcmp(key, "id")) { /* reference id*/
            if (PeekType() == kStringType) {
                asset->mRefId = std::string(GetString());
            } else {
                RAPIDJSON_ASSERT(PeekType() == kNumberType);
                asset->mRefId = toString(GetInt());
            }
        } else if (0 == strcmp(key, "layers")) {
            asset->mAssetType = LOTAsset::Type::Precomp;
            RAPIDJSON_ASSERT(PeekType() == kArrayType);
            EnterArray();
            bool staticFlag = true;
            while (NextArrayValue()) {
                auto layer = parseLayer();
                if (layer) {
                    staticFlag = staticFlag && layer->isStatic();
                    asset->mLayers.push_back(layer);
                }
            }
            asset->setStatic(staticFlag);
        } else {
#ifdef DEBUG_PARSER
            vWarning << "Asset Attribute Skipped : " << key;
#endif
            Skip(key);
        }
    }

    if (asset->mAssetType == LOTAsset::Type::Image) {
        if (embededResource) {
            // embeder resource should start with "data:"
            if (filename.compare(0, 5, "data:") == 0) {
                asset->loadImageData(convertFromBase64(filename));
            }
        } else {
            asset->loadImagePath(mDirPath + relativePath + filename);
        }
    }

    return asset;
}

void LottieParserImpl::parseLayers(LOTCompositionData *comp)
{
    comp->mRootLayer = allocator().make<LOTLayerData>();
    comp->mRootLayer->mLayerType = LayerType::Precomp;
    comp->mRootLayer->setName("__");
    bool staticFlag = true;
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        auto layer = parseLayer();
        if (layer) {
            staticFlag = staticFlag && layer->isStatic();
            comp->mRootLayer->mChildren.push_back(layer);
        }
    }
    comp->mRootLayer->setStatic(staticFlag);
}

LottieColor LottieParserImpl::toColor(const char *str)
{
    LottieColor color;
    auto len = strlen(str);

    // some resource has empty color string
    // return a default color for those cases.
    if (len != 7 || str[0] != '#') return color;

    char tmp[3] = {'\0', '\0', '\0'};
    tmp[0] = str[1];
    tmp[1] = str[2];
    color.r = std::strtol(tmp, nullptr, 16) / 255.0f;

    tmp[0] = str[3];
    tmp[1] = str[4];
    color.g = std::strtol(tmp, nullptr, 16) / 255.0f;

    tmp[0] = str[5];
    tmp[1] = str[6];
    color.b = std::strtol(tmp, nullptr, 16) / 255.0f;

    return color;
}

MatteType LottieParserImpl::getMatteType()
{
    RAPIDJSON_ASSERT(PeekType() == kNumberType);
    switch (GetInt()) {
    case 1:
        return MatteType::Alpha;
        break;
    case 2:
        return MatteType::AlphaInv;
        break;
    case 3:
        return MatteType::Luma;
        break;
    case 4:
        return MatteType::LumaInv;
        break;
    default:
        return MatteType::None;
        break;
    }
}

LayerType LottieParserImpl::getLayerType()
{
    RAPIDJSON_ASSERT(PeekType() == kNumberType);
    switch (GetInt()) {
    case 0:
        return LayerType::Precomp;
        break;
    case 1:
        return LayerType::Solid;
        break;
    case 2:
        return LayerType::Image;
        break;
    case 3:
        return LayerType::Null;
        break;
    case 4:
        return LayerType::Shape;
        break;
    case 5:
        return LayerType::Text;
        break;
    default:
        return LayerType::Null;
        break;
    }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/layers/shape.json
 *
 */
LOTLayerData* LottieParserImpl::parseLayer()
{
    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    LOTLayerData *layer = allocator().make<LOTLayerData>();
    curLayerRef = layer;
    bool ddd = true;
    EnterObject();
    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "ty")) { /* Type of layer*/
            layer->mLayerType = getLayerType();
        } else if (0 == strcmp(key, "nm")) { /*Layer name*/
            RAPIDJSON_ASSERT(PeekType() == kStringType);
            layer->setName(GetString());
        } else if (0 == strcmp(key, "ind")) { /*Layer index in AE. Used for
                                                 parenting and expressions.*/
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            layer->mId = GetInt();
        } else if (0 == strcmp(key, "ddd")) { /*3d layer */
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            ddd = GetInt();
        } else if (0 ==
                   strcmp(key,
                          "parent")) { /*Layer Parent. Uses "ind" of parent.*/
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            layer->mParentId = GetInt();
        } else if (0 == strcmp(key, "refId")) { /*preComp Layer reference id*/
            RAPIDJSON_ASSERT(PeekType() == kStringType);
            layer->extra()->mPreCompRefId = std::string(GetString());
            layer->mHasGradient = true;
            mLayersToUpdate.push_back(layer);
        } else if (0 == strcmp(key, "sr")) {  // "Layer Time Stretching"
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            layer->mTimeStreatch = GetDouble();
        } else if (0 == strcmp(key, "tm")) {  // time remapping
            parseProperty(layer->extra()->mTimeRemap);
        } else if (0 == strcmp(key, "ip")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            layer->mInFrame = std::lround(GetDouble());
        } else if (0 == strcmp(key, "op")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            layer->mOutFrame = std::lround(GetDouble());
        } else if (0 == strcmp(key, "st")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            layer->mStartFrame = GetDouble();
        } else if (0 == strcmp(key, "bm")) {
            layer->mBlendMode = getBlendMode();
        } else if (0 == strcmp(key, "ks")) {
            RAPIDJSON_ASSERT(PeekType() == kObjectType);
            EnterObject();
            layer->mTransform = parseTransformObject(ddd);
        } else if (0 == strcmp(key, "shapes")) {
            parseShapesAttr(layer);
        } else if (0 == strcmp(key, "w")) {
            layer->mLayerSize.setWidth(GetInt());
        } else if (0 == strcmp(key, "h")) {
            layer->mLayerSize.setHeight(GetInt());
        } else if (0 == strcmp(key, "sw")) {
            layer->mLayerSize.setWidth(GetInt());
        } else if (0 == strcmp(key, "sh")) {
            layer->mLayerSize.setHeight(GetInt());
        } else if (0 == strcmp(key, "sc")) {
            layer->extra()->mSolidColor = toColor(GetString());
        } else if (0 == strcmp(key, "tt")) {
            layer->mMatteType = getMatteType();
        } else if (0 == strcmp(key, "hasMask")) {
            layer->mHasMask = GetBool();
        } else if (0 == strcmp(key, "masksProperties")) {
            parseMaskProperty(layer);
        } else if (0 == strcmp(key, "ao")) {
            layer->mAutoOrient = GetInt();
        } else if (0 == strcmp(key, "hd")) {
            layer->setHidden(GetBool());
        } else if (0 == strcmp(key, "t")) {
            parseText(layer->extra()->textLayer());
        } else {
#ifdef DEBUG_PARSER
            vWarning << "Layer Attribute Skipped : " << key;
#endif
            Skip(key);
        }
    }

    if (!layer->mTransform) {
        // not a valid layer
        return nullptr;
    }

    // make sure layer data is not corrupted.
    if (layer->hasParent() && (layer->id() == layer->parentId())) return nullptr;

    if (layer->mExtra) layer->mExtra->mCompRef = compRef;

    if (layer->hidden()) {
        // if layer is hidden, only data that is usefull is its
        // transform matrix(when it is a parent of some other layer)
        // so force it to be a Null Layer and release all resource.
        layer->setStatic(layer->mTransform->isStatic());
        layer->mLayerType = LayerType::Null;
        layer->mChildren = {};
        return layer;
    }

    // update the static property of layer
    bool staticFlag = true;
    for (const auto &child : layer->mChildren) {
        staticFlag &= child->isStatic();
    }

    if (layer->hasMask()) {
        for (const auto &mask : layer->mExtra->mMasks) {
            staticFlag &= mask->isStatic();
        }
    }

    layer->setStatic(staticFlag && layer->mTransform->isStatic());

    return layer;
}

void LottieParserImpl::parseMaskProperty(LOTLayerData *layer)
{
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        layer->extra()->mMasks.push_back(parseMaskObject());
    }
}

LOTMaskData* LottieParserImpl::parseMaskObject()
{
    auto obj = allocator().make<LOTMaskData>();

    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    EnterObject();
    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "inv")) {
            obj->mInv = GetBool();
        } else if (0 == strcmp(key, "mode")) {
            const char *str = GetString();
            if (!str) {
                obj->mMode = LOTMaskData::Mode::None;
                continue;
            }
            switch (str[0]) {
            case 'n':
                obj->mMode = LOTMaskData::Mode::None;
                break;
            case 'a':
                obj->mMode = LOTMaskData::Mode::Add;
                break;
            case 's':
                obj->mMode = LOTMaskData::Mode::Substarct;
                break;
            case 'i':
                obj->mMode = LOTMaskData::Mode::Intersect;
                break;
            case 'f':
                obj->mMode = LOTMaskData::Mode::Difference;
                break;
            default:
                obj->mMode = LOTMaskData::Mode::None;
                break;
            }
        } else if (0 == strcmp(key, "pt")) {
            parseShapeProperty(obj->mShape);
        } else if (0 == strcmp(key, "o")) {
            parseProperty(obj->mOpacity);
        } else {
            Skip(key);
        }
    }
    obj->mIsStatic = obj->mShape.isStatic() && obj->mOpacity.isStatic();
    return obj;
}

void LottieParserImpl::parseShapesAttr(LOTLayerData *layer)
{
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        parseObject(layer);
    }
}

LOTData* LottieParserImpl::parseObjectTypeAttr()
{
    RAPIDJSON_ASSERT(PeekType() == kStringType);
    const char *type = GetString();
    if (0 == strcmp(type, "gr")) {
        return parseGroupObject();
    } else if (0 == strcmp(type, "rc")) {
        return parseRectObject();
    } else if (0 == strcmp(type, "el")) {
        return parseEllipseObject();
    } else if (0 == strcmp(type, "tr")) {
        return parseTransformObject();
    } else if (0 == strcmp(type, "fl")) {
        return parseFillObject();
    } else if (0 == strcmp(type, "st")) {
        return parseStrokeObject();
    } else if (0 == strcmp(type, "gf")) {
        curLayerRef->mHasGradient = true;
        return parseGFillObject();
    } else if (0 == strcmp(type, "gs")) {
        curLayerRef->mHasGradient = true;
        return parseGStrokeObject();
    } else if (0 == strcmp(type, "sh")) {
        return parseShapeObject();
    } else if (0 == strcmp(type, "sr")) {
        return parsePolystarObject();
    } else if (0 == strcmp(type, "tm")) {
        curLayerRef->mHasPathOperator = true;
        return parseTrimObject();
    } else if (0 == strcmp(type, "rp")) {
        curLayerRef->mHasRepeater = true;
        return parseReapeaterObject();
    } else if (0 == strcmp(type, "mm")) {
        vWarning << "Merge Path is not supported yet";
        return nullptr;
    } else {
#ifdef DEBUG_PARSER
        vDebug << "The Object Type not yet handled = " << type;
#endif
        return nullptr;
    }
}

void LottieParserImpl::parseObject(LOTGroupData *parent)
{
    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    EnterObject();
    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "ty")) {
            auto child = parseObjectTypeAttr();
            if (child && !child->hidden()) parent->mChildren.push_back(child);
        } else {
            Skip(key);
        }
    }
}

LOTData* LottieParserImpl::parseGroupObject()
{
    auto group = allocator().make<LOTShapeGroupData>();

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "nm")) {
            group->setName(GetString());
        } else if (0 == strcmp(key, "it")) {
            RAPIDJSON_ASSERT(PeekType() == kArrayType);
            EnterArray();
            while (NextArrayValue()) {
                RAPIDJSON_ASSERT(PeekType() == kObjectType);
                parseObject(group);
            }
            if (group->mChildren.back()->type() == LOTData::Type::Transform) {
                group->mTransform = static_cast<LOTTransformData *>(group->mChildren.back());
                group->mChildren.pop_back();
            }
        } else {
            Skip(key);
        }
    }
    bool staticFlag = true;
    for (const auto &child : group->mChildren) {
        staticFlag &= child->isStatic();
    }

    if (group->mTransform) {
        group->setStatic(staticFlag && group->mTransform->isStatic());
    }

    return group;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/rect.json
 */
LOTRectData* LottieParserImpl::parseRectObject()
{
    auto obj = allocator().make<LOTRectData>();

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "nm")) {
            obj->setName(GetString());
        } else if (0 == strcmp(key, "p")) {
            parseProperty(obj->mPos);
        } else if (0 == strcmp(key, "s")) {
            parseProperty(obj->mSize);
        } else if (0 == strcmp(key, "r")) {
            parseProperty(obj->mRound);
        } else if (0 == strcmp(key, "d")) {
            obj->mDirection = GetInt();
        } else if (0 == strcmp(key, "hd")) {
            obj->setHidden(GetBool());
        } else {
            Skip(key);
        }
    }
    obj->setStatic(obj->mPos.isStatic() && obj->mSize.isStatic() &&
                   obj->mRound.isStatic());
    return obj;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/ellipse.json
 */
LOTEllipseData* LottieParserImpl::parseEllipseObject()
{
    auto obj = allocator().make<LOTEllipseData>();

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "nm")) {
            obj->setName(GetString());
        } else if (0 == strcmp(key, "p")) {
            parseProperty(obj->mPos);
        } else if (0 == strcmp(key, "s")) {
            parseProperty(obj->mSize);
        } else if (0 == strcmp(key, "d")) {
            obj->mDirection = GetInt();
        } else if (0 == strcmp(key, "hd")) {
            obj->setHidden(GetBool());
        } else {
            Skip(key);
        }
    }
    obj->setStatic(obj->mPos.isStatic() && obj->mSize.isStatic());
    return obj;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/shape.json
 */
LOTShapeData* LottieParserImpl::parseShapeObject()
{
    auto obj = allocator().make<LOTShapeData>();

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "nm")) {
            obj->setName(GetString());
        } else if (0 == strcmp(key, "ks")) {
            parseShapeProperty(obj->mShape);
        } else if (0 == strcmp(key, "d")) {
            obj->mDirection = GetInt();
        } else if (0 == strcmp(key, "hd")) {
            obj->setHidden(GetBool());
        } else {
#ifdef DEBUG_PARSER
            vDebug << "Shape property ignored :" << key;
#endif
            Skip(key);
        }
    }
    obj->setStatic(obj->mShape.isStatic());

    return obj;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/star.json
 */
LOTPolystarData* LottieParserImpl::parsePolystarObject()
{
    auto obj = allocator().make<LOTPolystarData>();

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "nm")) {
            obj->setName(GetString());
        } else if (0 == strcmp(key, "p")) {
            parseProperty(obj->mPos);
        } else if (0 == strcmp(key, "pt")) {
            parseProperty(obj->mPointCount);
        } else if (0 == strcmp(key, "ir")) {
            parseProperty(obj->mInnerRadius);
        } else if (0 == strcmp(key, "is")) {
            parseProperty(obj->mInnerRoundness);
        } else if (0 == strcmp(key, "or")) {
            parseProperty(obj->mOuterRadius);
        } else if (0 == strcmp(key, "os")) {
            parseProperty(obj->mOuterRoundness);
        } else if (0 == strcmp(key, "r")) {
            parseProperty(obj->mRotation);
        } else if (0 == strcmp(key, "sy")) {
            int starType = GetInt();
            if (starType == 1) obj->mPolyType = LOTPolystarData::PolyType::Star;
            if (starType == 2) obj->mPolyType = LOTPolystarData::PolyType::Polygon;
        } else if (0 == strcmp(key, "d")) {
            obj->mDirection = GetInt();
        } else if (0 == strcmp(key, "hd")) {
            obj->setHidden(GetBool());
        } else {
#ifdef DEBUG_PARSER
            vDebug << "Polystar property ignored :" << key;
#endif
            Skip(key);
        }
    }
    obj->setStatic(
        obj->mPos.isStatic() && obj->mPointCount.isStatic() &&
        obj->mInnerRadius.isStatic() && obj->mInnerRoundness.isStatic() &&
        obj->mOuterRadius.isStatic() && obj->mOuterRoundness.isStatic() &&
        obj->mRotation.isStatic());

    return obj;
}

LOTTrimData::TrimType LottieParserImpl::getTrimType()
{
    RAPIDJSON_ASSERT(PeekType() == kNumberType);
    switch (GetInt()) {
    case 1:
        return LOTTrimData::TrimType::Simultaneously;
        break;
    case 2:
        return LOTTrimData::TrimType::Individually;
        break;
    default:
        RAPIDJSON_ASSERT(0);
        return LOTTrimData::TrimType::Simultaneously;
        break;
    }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/trim.json
 */
LOTTrimData* LottieParserImpl::parseTrimObject()
{
    auto obj = allocator().make<LOTTrimData>();

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "nm")) {
            obj->setName(GetString());
        } else if (0 == strcmp(key, "s")) {
            parseProperty(obj->mStart);
        } else if (0 == strcmp(key, "e")) {
            parseProperty(obj->mEnd);
        } else if (0 == strcmp(key, "o")) {
            parseProperty(obj->mOffset);
        } else if (0 == strcmp(key, "m")) {
            obj->mTrimType = getTrimType();
        } else if (0 == strcmp(key, "hd")) {
            obj->setHidden(GetBool());
        } else {
#ifdef DEBUG_PARSER
            vDebug << "Trim property ignored :" << key;
#endif
            Skip(key);
        }
    }
    obj->setStatic(obj->mStart.isStatic() && obj->mEnd.isStatic() &&
                   obj->mOffset.isStatic());
    return obj;
}

void LottieParserImpl::getValue(LOTRepeaterTransform &obj)
{
    EnterObject();

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "a")) {
            parseProperty(obj.mAnchor);
        } else if (0 == strcmp(key, "p")) {
            parseProperty(obj.mPosition);
        } else if (0 == strcmp(key, "r")) {
            parseProperty(obj.mRotation);
        } else if (0 == strcmp(key, "s")) {
            parseProperty(obj.mScale);
        } else if (0 == strcmp(key, "so")) {
            parseProperty(obj.mStartOpacity);
        } else if (0 == strcmp(key, "eo")) {
            parseProperty(obj.mEndOpacity);
        } else {
            Skip(key);
        }
    }
}

LOTRepeaterData* LottieParserImpl::parseReapeaterObject()
{
    auto obj = allocator().make<LOTRepeaterData>();

    obj->setContent(allocator().make<LOTShapeGroupData>());

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "nm")) {
            obj->setName(GetString());
        } else if (0 == strcmp(key, "c")) {
            parseProperty(obj->mCopies);
            float maxCopy = 0.0;
            if (!obj->mCopies.isStatic()) {
                for (auto &keyFrame : obj->mCopies.animation().mKeyFrames) {
                    if (maxCopy < keyFrame.mValue.mStartValue)
                        maxCopy = keyFrame.mValue.mStartValue;
                    if (maxCopy < keyFrame.mValue.mEndValue)
                        maxCopy = keyFrame.mValue.mEndValue;
                }
            } else {
                maxCopy = obj->mCopies.value();
            }
            obj->mMaxCopies = maxCopy;
        } else if (0 == strcmp(key, "o")) {
            parseProperty(obj->mOffset);
        } else if (0 == strcmp(key, "tr")) {
            getValue(obj->mTransform);
        } else if (0 == strcmp(key, "hd")) {
            obj->setHidden(GetBool());
        } else {
#ifdef DEBUG_PARSER
            vDebug << "Repeater property ignored :" << key;
#endif
            Skip(key);
        }
    }
    obj->setStatic(obj->mCopies.isStatic() && obj->mOffset.isStatic() &&
                   obj->mTransform.isStatic());

    return obj;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/transform.json
 */
LOTTransformData* LottieParserImpl::parseTransformObject(
    bool ddd)
{
    auto objT = allocator().make<LOTTransformData>();

    std::shared_ptr<LOTTransformData> sharedTransform =
        std::make_shared<LOTTransformData>();

    auto obj = allocator().make<TransformData>();
    if (ddd) {
        obj->createExtraData();
        obj->mExtra->m3DData = true;
    }

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "nm")) {
            sharedTransform->setName(GetString());
        } else if (0 == strcmp(key, "a")) {
            parseProperty(obj->mAnchor);
        } else if (0 == strcmp(key, "p")) {
            EnterObject();
            bool separate = false;
            while (const char *key = NextObjectKey()) {
                if (0 == strcmp(key, "k")) {
                    parsePropertyHelper(obj->mPosition);
                } else if (0 == strcmp(key, "s")) {
                    obj->createExtraData();
                    obj->mExtra->mSeparate = GetBool();
                    separate = true;
                } else if (separate && (0 == strcmp(key, "x"))) {
                    parseProperty(obj->mExtra->mSeparateX);
                } else if (separate && (0 == strcmp(key, "y"))) {
                    parseProperty(obj->mExtra->mSeparateY);
                } else {
                    Skip(key);
                }
            }
        } else if (0 == strcmp(key, "r")) {
            parseProperty(obj->mRotation);
        } else if (0 == strcmp(key, "s")) {
            parseProperty(obj->mScale);
        } else if (0 == strcmp(key, "o")) {
            parseProperty(obj->mOpacity);
        } else if (0 == strcmp(key, "hd")) {
            sharedTransform->setHidden(GetBool());
        } else if (0 == strcmp(key, "rx")) {
            parseProperty(obj->mExtra->m3DRx);
        } else if (0 == strcmp(key, "ry")) {
            parseProperty(obj->mExtra->m3DRy);
        } else if (0 == strcmp(key, "rz")) {
            parseProperty(obj->mExtra->m3DRz);
        } else {
            Skip(key);
        }
    }
    bool isStatic = obj->mAnchor.isStatic() && obj->mPosition.isStatic() &&
                    obj->mRotation.isStatic() && obj->mScale.isStatic() &&
                    obj->mOpacity.isStatic();
    if (obj->mExtra) {
        isStatic = isStatic &&
                   obj->mExtra->m3DRx.isStatic() &&
                   obj->mExtra->m3DRy.isStatic() &&
                   obj->mExtra->m3DRz.isStatic() &&
                   obj->mExtra->mSeparateX.isStatic() &&
                   obj->mExtra->mSeparateY.isStatic();
    }

    objT->set(obj, isStatic);

    return objT;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/fill.json
 */
LOTFillData* LottieParserImpl::parseFillObject()
{
    auto obj = allocator().make<LOTFillData>();

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "nm")) {
            obj->setName(GetString());
        } else if (0 == strcmp(key, "c")) {
            parseProperty(obj->mColor);
        } else if (0 == strcmp(key, "o")) {
            parseProperty(obj->mOpacity);
        } else if (0 == strcmp(key, "fillEnabled")) {
            obj->mEnabled = GetBool();
        } else if (0 == strcmp(key, "r")) {
            obj->mFillRule = getFillRule();
        } else if (0 == strcmp(key, "hd")) {
            obj->setHidden(GetBool());
        } else {
#ifdef DEBUG_PARSER
            vWarning << "Fill property skipped = " << key;
#endif
            Skip(key);
        }
    }
    obj->setStatic(obj->mColor.isStatic() && obj->mOpacity.isStatic());

    return obj;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/helpers/lineCap.json
 */
CapStyle LottieParserImpl::getLineCap()
{
    RAPIDJSON_ASSERT(PeekType() == kNumberType);
    switch (GetInt()) {
    case 1:
        return CapStyle::Flat;
        break;
    case 2:
        return CapStyle::Round;
        break;
    default:
        return CapStyle::Square;
        break;
    }
}

FillRule LottieParserImpl::getFillRule()
{
    RAPIDJSON_ASSERT(PeekType() == kNumberType);
    switch (GetInt()) {
    case 1:
        return FillRule::Winding;
        break;
    case 2:
        return FillRule::EvenOdd;
        break;
    default:
        return FillRule::Winding;
        break;
    }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/helpers/lineJoin.json
 */
JoinStyle LottieParserImpl::getLineJoin()
{
    RAPIDJSON_ASSERT(PeekType() == kNumberType);
    switch (GetInt()) {
    case 1:
        return JoinStyle::Miter;
        break;
    case 2:
        return JoinStyle::Round;
        break;
    default:
        return JoinStyle::Bevel;
        break;
    }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/stroke.json
 */
LOTStrokeData* LottieParserImpl::parseStrokeObject()
{
    auto obj = allocator().make<LOTStrokeData>();

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "nm")) {
            obj->setName(GetString());
        } else if (0 == strcmp(key, "c")) {
            parseProperty(obj->mColor);
        } else if (0 == strcmp(key, "o")) {
            parseProperty(obj->mOpacity);
        } else if (0 == strcmp(key, "w")) {
            parseProperty(obj->mWidth);
        } else if (0 == strcmp(key, "fillEnabled")) {
            obj->mEnabled = GetBool();
        } else if (0 == strcmp(key, "lc")) {
            obj->mCapStyle = getLineCap();
        } else if (0 == strcmp(key, "lj")) {
            obj->mJoinStyle = getLineJoin();
        } else if (0 == strcmp(key, "ml")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            obj->mMiterLimit = GetDouble();
        } else if (0 == strcmp(key, "d")) {
            parseDashProperty(obj->mDash);
        } else if (0 == strcmp(key, "hd")) {
            obj->setHidden(GetBool());
        } else {
#ifdef DEBUG_PARSER
            vWarning << "Stroke property skipped = " << key;
#endif
            Skip(key);
        }
    }
    obj->setStatic(obj->mColor.isStatic() && obj->mOpacity.isStatic() &&
                   obj->mWidth.isStatic() && obj->mDash.isStatic());
    return obj;
}

void LottieParserImpl::parseGradientProperty(LOTGradient *obj, const char *key)
{
    if (0 == strcmp(key, "t")) {
        RAPIDJSON_ASSERT(PeekType() == kNumberType);
        obj->mGradientType = GetInt();
    } else if (0 == strcmp(key, "o")) {
        parseProperty(obj->mOpacity);
    } else if (0 == strcmp(key, "s")) {
        parseProperty(obj->mStartPoint);
    } else if (0 == strcmp(key, "e")) {
        parseProperty(obj->mEndPoint);
    } else if (0 == strcmp(key, "h")) {
        parseProperty(obj->mHighlightLength);
    } else if (0 == strcmp(key, "a")) {
        parseProperty(obj->mHighlightAngle);
    } else if (0 == strcmp(key, "g")) {
        EnterObject();
        while (const char *key = NextObjectKey()) {
            if (0 == strcmp(key, "k")) {
                parseProperty(obj->mGradient);
            } else if (0 == strcmp(key, "p")) {
                obj->mColorPoints = GetInt();
            } else {
                Skip(nullptr);
            }
        }
    } else if (0 == strcmp(key, "hd")) {
        obj->setHidden(GetBool());
    } else {
#ifdef DEBUG_PARSER
        vWarning << "Gradient property skipped = " << key;
#endif
        Skip(key);
    }
    obj->setStatic(
        obj->mOpacity.isStatic() && obj->mStartPoint.isStatic() &&
        obj->mEndPoint.isStatic() && obj->mHighlightAngle.isStatic() &&
        obj->mHighlightLength.isStatic() && obj->mGradient.isStatic());
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/gfill.json
 */
LOTGFillData* LottieParserImpl::parseGFillObject()
{
    auto obj = allocator().make<LOTGFillData>();

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "nm")) {
            obj->setName(GetString());
        } else if (0 == strcmp(key, "r")) {
            obj->mFillRule = getFillRule();
        } else {
            parseGradientProperty(obj, key);
        }
    }
    return obj;
}

void LottieParserImpl::parseDashProperty(LOTDashProperty &dash)
{
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        RAPIDJSON_ASSERT(PeekType() == kObjectType);
        EnterObject();
        while (const char *key = NextObjectKey()) {
            if (0 == strcmp(key, "v")) {
                dash.mData.emplace_back();
                parseProperty(dash.mData.back());
            } else {
                Skip(key);
            }
        }
    }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/gstroke.json
 */
LOTGStrokeData* LottieParserImpl::parseGStrokeObject()
{
    auto obj = allocator().make<LOTGStrokeData>();

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "nm")) {
            obj->setName(GetString());
        } else if (0 == strcmp(key, "w")) {
            parseProperty(obj->mWidth);
        } else if (0 == strcmp(key, "lc")) {
            obj->mCapStyle = getLineCap();
        } else if (0 == strcmp(key, "lj")) {
            obj->mJoinStyle = getLineJoin();
        } else if (0 == strcmp(key, "ml")) {
            RAPIDJSON_ASSERT(PeekType() == kNumberType);
            obj->mMiterLimit = GetDouble();
        } else if (0 == strcmp(key, "d")) {
            parseDashProperty(obj->mDash);
        } else {
            parseGradientProperty(obj, key);
        }
    }

    obj->setStatic(obj->isStatic() && obj->mWidth.isStatic() &&
                   obj->mDash.isStatic());
    return obj;
}

void LottieParserImpl::getValue(std::vector<VPointF> &v)
{
    RAPIDJSON_ASSERT(PeekType() == kArrayType);
    EnterArray();
    while (NextArrayValue()) {
        RAPIDJSON_ASSERT(PeekType() == kArrayType);
        EnterArray();
        VPointF pt;
        getValue(pt);
        v.push_back(pt);
    }
}

void LottieParserImpl::getValue(VPointF &pt)
{
    float val[4] = {0.f};
    int   i = 0;

    if (PeekType() == kArrayType) EnterArray();

    while (NextArrayValue()) {
        const auto value = GetDouble();
        if (i < 4) {
            val[i++] = value;
        }
    }
    pt.setX(val[0]);
    pt.setY(val[1]);
}

void LottieParserImpl::getValue(float &val)
{
    if (PeekType() == kArrayType) {
        EnterArray();
        if (NextArrayValue()) val = GetDouble();
        // discard rest
        while (NextArrayValue()) {
            GetDouble();
        }
    } else if (PeekType() == kNumberType) {
        val = GetDouble();
    } else {
        RAPIDJSON_ASSERT(0);
    }
}

void LottieParserImpl::getValue(LottieColor &color)
{
    float val[4] = {0.f};
    int   i = 0;
    if (PeekType() == kArrayType) EnterArray();

    while (NextArrayValue()) {
        const auto value = GetDouble();
        if (i < 4) {
            val[i++] = value;
        }
    }

    if (mColorFilter) mColorFilter( val[0] , val[1], val[2]) ;

    color.r = val[0];
    color.g = val[1];
    color.b = val[2];
}

void LottieParserImpl::getValue(LottieGradient &grad)
{
    if (PeekType() == kArrayType) EnterArray();

    while (NextArrayValue()) {
        grad.mGradient.push_back(GetDouble());
    }
}

void LottieParserImpl::getValue(int &val)
{
    if (PeekType() == kArrayType) {
        EnterArray();
        while (NextArrayValue()) {
            val = GetInt();
        }
    } else if (PeekType() == kNumberType) {
        val = GetInt();
    } else {
        RAPIDJSON_ASSERT(0);
    }
}

void LottieParserImpl::parsePathInfo()
{
    mPathInfo.reset();

    /*
     * The shape object could be wrapped by a array
     * if its part of the keyframe object
     */
    bool arrayWrapper = (PeekType() == kArrayType);
    if (arrayWrapper) EnterArray();

    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    EnterObject();
    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "i")) {
            getValue(mPathInfo.mInPoint);
        } else if (0 == strcmp(key, "o")) {
            getValue(mPathInfo.mOutPoint);
        } else if (0 == strcmp(key, "v")) {
            getValue(mPathInfo.mVertices);
        } else if (0 == strcmp(key, "c")) {
            mPathInfo.mClosed = GetBool();
        } else {
            RAPIDJSON_ASSERT(0);
            Skip(nullptr);
        }
    }
    // exit properly from the array
    if (arrayWrapper) NextArrayValue();

    mPathInfo.convert();
}

void LottieParserImpl::getValue(LottieShapeData &obj)
{
    parsePathInfo();
    obj.mPoints = mPathInfo.mResult;
    obj.mClosed = mPathInfo.mClosed;
}

VPointF LottieParserImpl::parseInperpolatorPoint()
{
    VPointF cp;
    RAPIDJSON_ASSERT(PeekType() == kObjectType);
    EnterObject();
    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "x")) {
            getValue(cp.rx());
        }
        if (0 == strcmp(key, "y")) {
            getValue(cp.ry());
        }
    }
    return cp;
}

template <typename T>
bool LottieParserImpl::parseKeyFrameValue(const char *, LOTKeyFrameValue<T> &)
{
    return false;
}

template <>
bool LottieParserImpl::parseKeyFrameValue(const char *               key,
                                          LOTKeyFrameValue<VPointF> &value)
{
    if (0 == strcmp(key, "ti")) {
        value.mPathKeyFrame = true;
        getValue(value.mInTangent);
    } else if (0 == strcmp(key, "to")) {
        value.mPathKeyFrame = true;
        getValue(value.mOutTangent);
    } else {
        return false;
    }
    return true;
}

VInterpolator* LottieParserImpl::interpolator(
    VPointF inTangent, VPointF outTangent, std::string key)
{
    if (key.empty()) {
        std::array<char, 20> temp;
        snprintf(temp.data(), temp.size(), "%.2f_%.2f_%.2f_%.2f", inTangent.x(),
                 inTangent.y(), outTangent.x(), outTangent.y());
        key = temp.data();
    }

    auto search = mInterpolatorCache.find(key);

    if (search != mInterpolatorCache.end()) {
        return search->second;
    }

    auto obj = allocator().make<VInterpolator>(outTangent, inTangent);
    mInterpolatorCache[std::move(key)] = obj;
    return obj;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/properties/multiDimensionalKeyframed.json
 */
template <typename T>
void LottieParserImpl::parseKeyFrame(LOTAnimInfo<T> &obj)
{
    struct ParsedField {
        std::string interpolatorKey;
        bool        interpolator{false};
        bool        value{false};
        bool        hold{false};
        bool        noEndValue{true};
    };

    EnterObject();
    ParsedField    parsed;
    LOTKeyFrame<T> keyframe;
    VPointF        inTangent;
    VPointF        outTangent;

    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "i")) {
            parsed.interpolator = true;
            inTangent = parseInperpolatorPoint();
        } else if (0 == strcmp(key, "o")) {
            outTangent = parseInperpolatorPoint();
        } else if (0 == strcmp(key, "t")) {
            keyframe.mStartFrame = GetDouble();
        } else if (0 == strcmp(key, "s")) {
            parsed.value = true;
            getValue(keyframe.mValue.mStartValue);
            continue;
        } else if (0 == strcmp(key, "e")) {
            parsed.noEndValue = false;
            getValue(keyframe.mValue.mEndValue);
            continue;
        } else if (0 == strcmp(key, "n")) {
            if (PeekType() == kStringType) {
                parsed.interpolatorKey = GetString();
            } else {
                RAPIDJSON_ASSERT(PeekType() == kArrayType);
                EnterArray();
                while (NextArrayValue()) {
                    RAPIDJSON_ASSERT(PeekType() == kStringType);
                    if (parsed.interpolatorKey.empty()) {
                        parsed.interpolatorKey = GetString();
                    } else {
                        // skip rest of the string
                        GetString();
                    }
                }
            }
            continue;
        } else if (parseKeyFrameValue(key, keyframe.mValue)) {
            continue;
        } else if (0 == strcmp(key, "h")) {
            parsed.hold = GetInt();
            continue;
        } else {
#ifdef DEBUG_PARSER
            vDebug << "key frame property skipped = " << key;
#endif
            Skip(key);
        }
    }

    if (!obj.mKeyFrames.empty()) {
        // update the endFrame value of current keyframe
        obj.mKeyFrames.back().mEndFrame = keyframe.mStartFrame;
        // if no end value provided, copy start value to previous frame
        if (parsed.value && parsed.noEndValue) {
            obj.mKeyFrames.back().mValue.mEndValue =
                keyframe.mValue.mStartValue;
        }
    }

    if (parsed.hold) {
        keyframe.mValue.mEndValue = keyframe.mValue.mStartValue;
        keyframe.mEndFrame = keyframe.mStartFrame;
        obj.mKeyFrames.push_back(std::move(keyframe));
    } else if (parsed.interpolator) {
        keyframe.mInterpolator = interpolator(
            inTangent, outTangent, std::move(parsed.interpolatorKey));
        obj.mKeyFrames.push_back(std::move(keyframe));
    } else {
        // its the last frame discard.
    }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/properties/shapeKeyframed.json
 */

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/properties/shape.json
 */
void LottieParserImpl::parseShapeProperty(LOTAnimatable<LottieShapeData> &obj)
{
    EnterObject();
    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "k")) {
            if (PeekType() == kArrayType) {
                EnterArray();
                while (NextArrayValue()) {
                    RAPIDJSON_ASSERT(PeekType() == kObjectType);
                    parseKeyFrame(obj.animation());
                }
            } else {
                if (!obj.isStatic()) {
                    RAPIDJSON_ASSERT(false);
                    st_ = kError;
                    return;
                }
                getValue(obj.value());
            }
        } else {
#ifdef DEBUG_PARSER
            vDebug << "shape property ignored = " << key;
#endif
            Skip(nullptr);
        }
    }
}

template <typename T>
void LottieParserImpl::parsePropertyHelper(LOTAnimatable<T> &obj)
{
    if (PeekType() == kNumberType) {
        if (!obj.isStatic()) {
            RAPIDJSON_ASSERT(false);
            st_ = kError;
            return;
        }
        /*single value property with no animation*/
        getValue(obj.value());
    } else {
        RAPIDJSON_ASSERT(PeekType() == kArrayType);
        EnterArray();
        while (NextArrayValue()) {
            /* property with keyframe info*/
            if (PeekType() == kObjectType) {
                parseKeyFrame(obj.animation());
            } else {
                /* Read before modifying.
                 * as there is no way of knowing if the
                 * value of the array is either array of numbers
                 * or array of object without entering the array
                 * thats why this hack is there
                 */
                RAPIDJSON_ASSERT(PeekType() == kNumberType);
                if (!obj.isStatic()) {
                    RAPIDJSON_ASSERT(false);
                    st_ = kError;
                    return;
                }
                /*multi value property with no animation*/
                getValue(obj.value());
                /*break here as we already reached end of array*/
                break;
            }
        }
    }
}

/*
 * https://github.com/airbnb/lottie-web/tree/master/docs/json/properties
 */
template <typename T>
void LottieParserImpl::parseProperty(LOTAnimatable<T> &obj)
{
    EnterObject();
    while (const char *key = NextObjectKey()) {
        if (0 == strcmp(key, "k")) {
            parsePropertyHelper(obj);
        } else {
            Skip(key);
        }
    }
}

#ifdef LOTTIE_DUMP_TREE_SUPPORT

class LOTDataInspector {
public:
    void visit(LOTCompositionData *obj, std::string level)
    {
        vDebug << " { " << level << "Composition:: a: " << !obj->isStatic()
               << ", v: " << obj->mVersion << ", stFm: " << obj->startFrame()
               << ", endFm: " << obj->endFrame()
               << ", W: " << obj->size().width()
               << ", H: " << obj->size().height() << "\n";
        level.append("\t");
        visit(obj->mRootLayer, level);
        level.erase(level.end() - 1, level.end());
        vDebug << " } " << level << "Composition End\n";
    }
    void visit(LOTLayerData *obj, std::string level)
    {
        vDebug << level << "{ " << layerType(obj->mLayerType)
               << ", name: " << obj->name() << ", id:" << obj->mId
               << " Pid:" << obj->mParentId << ", a:" << !obj->isStatic()
               << ", " << matteType(obj->mMatteType)
               << ", mask:" << obj->hasMask() << ", inFm:" << obj->mInFrame
               << ", outFm:" << obj->mOutFrame << ", stFm:" << obj->mStartFrame
               << ", ts:" << obj->mTimeStreatch << ", ao:" << obj->autoOrient()
               << ", W:" << obj->layerSize().width()
               << ", H:" << obj->layerSize().height();

        if (obj->mLayerType == LayerType::Image)
            vDebug << level << "\t{ "
                   << "ImageInfo:"
                   << " W :" << obj->extra()->mAsset->mWidth
                   << ", H :" << obj->extra()->mAsset->mHeight << " }"
                   << "\n";
        else {
            vDebug << level;
        }
        visitChildren(static_cast<LOTGroupData *>(obj), level);
        vDebug << level << "} " << layerType(obj->mLayerType).c_str()
               << ", id: " << obj->mId << "\n";
    }
    void visitChildren(LOTGroupData *obj, std::string level)
    {
        level.append("\t");
        for (const auto &child : obj->mChildren) visit(child, level);
        if (obj->mTransform) visit(obj->mTransform, level);
    }

    void visit(LOTData *obj, std::string level)
    {
        switch (obj->type()) {
        case LOTData::Type::Repeater: {
            auto r = static_cast<LOTRepeaterData *>(obj);
            vDebug << level << "{ Repeater: name: " << obj->name()
                   << " , a:" << !obj->isStatic()
                   << ", copies:" << r->maxCopies()
                   << ", offset:" << r->offset(0);
            visitChildren(r->mContent, level);
            vDebug << level << "} Repeater";
            break;
        }
        case LOTData::Type::ShapeGroup: {
            vDebug << level << "{ ShapeGroup: name: " << obj->name()
                   << " , a:" << !obj->isStatic();
            visitChildren(static_cast<LOTGroupData *>(obj), level);
            vDebug << level << "} ShapeGroup";
            break;
        }
        case LOTData::Type::Layer: {
            visit(static_cast<LOTLayerData *>(obj), level);
            break;
        }
        case LOTData::Type::Trim: {
            vDebug << level << "{ Trim: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case LOTData::Type::Rect: {
            vDebug << level << "{ Rect: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case LOTData::Type::Ellipse: {
            vDebug << level << "{ Ellipse: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case LOTData::Type::Shape: {
            vDebug << level << "{ Shape: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case LOTData::Type::Polystar: {
            vDebug << level << "{ Polystar: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case LOTData::Type::Transform: {
            vDebug << level << "{ Transform: name: " << obj->name()
                   << " , a: " << !obj->isStatic() << " }";
            break;
        }
        case LOTData::Type::Stroke: {
            vDebug << level << "{ Stroke: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case LOTData::Type::GStroke: {
            vDebug << level << "{ GStroke: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case LOTData::Type::Fill: {
            vDebug << level << "{ Fill: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case LOTData::Type::GFill: {
            auto f = static_cast<LOTGFillData *>(obj);
            vDebug << level << "{ GFill: name: " << obj->name()
                   << " , a:" << !f->isStatic() << ", ty:" << f->mGradientType
                   << ", s:" << f->mStartPoint.value(0)
                   << ", e:" << f->mEndPoint.value(0) << " }";
            break;
        }
        default:
            break;
        }
    }

    std::string matteType(MatteType type)
    {
        switch (type) {
        case MatteType::None:
            return "Matte::None";
            break;
        case MatteType::Alpha:
            return "Matte::Alpha";
            break;
        case MatteType::AlphaInv:
            return "Matte::AlphaInv";
            break;
        case MatteType::Luma:
            return "Matte::Luma";
            break;
        case MatteType::LumaInv:
            return "Matte::LumaInv";
            break;
        default:
            return "Matte::Unknown";
            break;
        }
    }
    std::string layerType(LayerType type)
    {
        switch (type) {
        case LayerType::Precomp:
            return "Layer::Precomp";
            break;
        case LayerType::Null:
            return "Layer::Null";
            break;
        case LayerType::Shape:
            return "Layer::Shape";
            break;
        case LayerType::Solid:
            return "Layer::Solid";
            break;
        case LayerType::Image:
            return "Layer::Image";
            break;
        case LayerType::Text:
            return "Layer::Text";
            break;
        default:
            return "Layer::Unknown";
            break;
        }
    }
};

#endif

LottieParser::~LottieParser() = default;
LottieParser::LottieParser(char *str, std::string dir_path, ColorFilter filter)
    : d(std::make_unique<LottieParserImpl>(str, std::move(dir_path), std::move(filter)))
{
    if (d->VerifyType())
        d->parseComposition();
    else
        vWarning << "Input data is not Lottie format!";
}

std::shared_ptr<LOTModel> LottieParser::model()
{
    if (!d->composition()) return nullptr;

    std::shared_ptr<LOTModel> model = std::make_shared<LOTModel>();
    model->mRoot = d->composition();
    model->mRoot->processRepeaterObjects();
    model->mRoot->updateStats();


#ifdef LOTTIE_DUMP_TREE_SUPPORT
    LOTDataInspector inspector;
    inspector.visit(model->mRoot.get(), "");
#endif

    return model;
}

RAPIDJSON_DIAG_POP
