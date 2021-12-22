/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
#include <stack>
#include <string_view>

#include "lottiemodel.h"
#include "JSON/JSON.hpp"
#include "JSON/ROString.hpp"

typedef JSONT<int32_t> JSON;
using namespace rlottie::internal;

/** JSON does not have a value type so make one for generic usage */
struct Value {
    ROString strValue;
    JSON::Token token;

    Value() { }

    double GetDouble()   { if (token.type == JSON::Token::Number) return (double)strValue; return 0; }
       int GetInt()      { if (token.type == JSON::Token::Number) return (int)strValue; return 0; }
    ROString GetString() { if (token.type == JSON::Token::String) return strValue; return 0; }
    bool GetBool()       { return (token.type == JSON::Token::True); }
    void GetNull()       { return; } // WTF?
    //TODO: Do everything else 

    void set(const ROString & value, const JSON::Token & token) { strValue = value; this->token = token;}
};

struct LottieParserImpl {
    enum ValueType
    {
        Undefined = 0,
        Null      = 1,
        Bool      = 2,
        Number    = 3,
        String    = 5,
        Array     = 6,
        Object    = 7,
    };

public:
    LottieParserImpl(const char *str, std::string dir_path, model::ColorFilter filter)
        : mColorFilter(std::move(filter)),
          mDirPath(std::move(dir_path)),
          mLastSuper(JSON::InvalidPos),
          mInput(str),
          errorPos(JSON::InvalidPos)
    { }
    LottieParserImpl(const char *str, size_t len, std::string dir_path, model::ColorFilter filter)
        : mColorFilter(std::move(filter)),
          mDirPath(std::move(dir_path)),
          mLastSuper(JSON::InvalidPos),
          mInput(str, (int)len),
          errorPos(JSON::InvalidPos)
    { }
    bool VerifyType();
    bool ParseNext();
    void Error() { errorPos = parser.pos; }

public:
    VArenaAlloc &allocator() { return compRef->mArenaAlloc; }
    bool         EnterObject();
    bool         EnterArray();
    ROString     NextObjectKey();
    bool         NextArrayValue();
    int          GetInt();
    double       GetDouble();
    ROString     GetString();
    std::string  GetStringObject();
    bool         GetBool();
    void         GetNull();

    void   SkipObject();
    void   SkipArray();
    void   SkipValue();
    Value *     PeekValue();
    ValueType   PeekType() const;
    bool   IsValid() { return errorPos == JSON::InvalidPos; }

    void                  Skip(const ROString & key);
    model::BlendMode      getBlendMode();
    CapStyle              getLineCap();
    JoinStyle             getLineJoin();
    FillRule              getFillRule();
    model::Trim::TrimType getTrimType();
    model::MatteType      getMatteType();
    model::Layer::Type    getLayerType();

    std::shared_ptr<model::Composition> composition() const
    {
        return mComposition;
    }
    void             parseComposition();
    void             parseMarkers();
    void             parseMarker();
    void             parseAssets(model::Composition *comp);
    model::Asset *   parseAsset();
    void             parseLayers(model::Composition *comp);
    model::Layer *   parseLayer();
    void             parseMaskProperty(model::Layer *layer);
    void             parseShapesAttr(model::Layer *layer);
    void             parseObject(model::Group *parent);
    model::Mask *    parseMaskObject();
    model::Object *  parseObjectTypeAttr();
    model::Object *  parseGroupObject();
    model::Rect *    parseRectObject();
    model::RoundedCorner *    parseRoundedCorner();
    void updateRoundedCorner(model::Group *parent, model::RoundedCorner *rc);

    model::Ellipse * parseEllipseObject();
    model::Path *    parseShapeObject();
    model::Polystar *parsePolystarObject();

    model::Transform *     parseTransformObject(bool ddd = false);
    model::Fill *          parseFillObject();
    model::GradientFill *  parseGFillObject();
    model::Stroke *        parseStrokeObject();
    model::GradientStroke *parseGStrokeObject();
    model::Trim *          parseTrimObject();
    model::Repeater *      parseReapeaterObject();

    void parseGradientProperty(model::Gradient *gradient, const ROString & key);

    VPointF parseInperpolatorPoint();

    void getValue(VPointF &pt);
    void getValue(float &fval);
    void getValue(model::Color &color);
    void getValue(int &ival);
    void getValue(model::PathData &shape);
    void getValue(model::Gradient::Data &gradient);
    void getValue(std::vector<VPointF> &v);
    void getValue(model::Repeater::Transform &);

    template <typename T, typename Tag>
    bool parseKeyFrameValue(const ROString &, model::Value<T, Tag> &)
    {
        return false;
    }

    template <typename T>
    bool parseKeyFrameValue(const ROString & key,
                            model::Value<T, model::Position> &value);
    template <typename T, typename Tag>
    void parseKeyFrame(model::KeyFrames<T, Tag> &obj);
    template <typename T>
    void parseProperty(model::Property<T> &obj);
    template <typename T, typename Tag>
    void parsePropertyHelper(model::Property<T, Tag> &obj);

    void parseShapeProperty(model::Property<model::PathData> &obj);
    void parseDashProperty(model::Dash &dash);

    VInterpolator *interpolator(VPointF, VPointF, std::string);

    model::Color toColor(const ROString & str);

    void resolveLayerRefs();
    void parsePathInfo();

private:
    model::ColorFilter mColorFilter;
    struct {
        std::vector<VPointF> mInPoint;  /* "i" */
        std::vector<VPointF> mOutPoint; /* "o" */
        std::vector<VPointF> mVertices; /* "v" */
        std::vector<VPointF> mResult;
        bool                 mClosed{false};

        void convert()
        {
            // shape data could be empty.
            if (mInPoint.empty() || mOutPoint.empty() || mVertices.empty()) {
                mResult.clear();
                return;
            }

            /*
             * Convert the AE shape format to
             * list of bazier curves
             * The final structure will be Move +size*Cubic + Cubic (if the path
             * is closed one)
             */
            if (mInPoint.size() != mOutPoint.size() ||
                mInPoint.size() != mVertices.size()) {
                mResult.clear();
            } else {
                auto size = mVertices.size();
                mResult.push_back(mVertices[0]);
                for (size_t i = 1; i < size; i++) {
                    mResult.push_back(
                        mVertices[i - 1] +
                        mOutPoint[i - 1]);  // CP1 = start + outTangent
                    mResult.push_back(mVertices[i] +
                                      mInPoint[i]);   // CP2 = end + inTangent
                    mResult.push_back(mVertices[i]);  // end point
                }

                if (mClosed) {
                    mResult.push_back(
                        mVertices[size - 1] +
                        mOutPoint[size - 1]);  // CP1 = start + outTangent
                    mResult.push_back(mVertices[0] +
                                      mInPoint[0]);   // CP2 = end + inTangent
                    mResult.push_back(mVertices[0]);  // end point
                }
            }
        }
        void reset()
        {
            mInPoint.clear();
            mOutPoint.clear();
            mVertices.clear();
            mResult.clear();
            mClosed = false;
        }
        void updatePath(VPath &out)
        {
            if (mResult.empty()) return;

            auto size = mResult.size();
            auto points = mResult.data();
            /* reserve exact memory requirement at once
             * ptSize = size + 1(size + close)
             * elmSize = size/3 cubic + 1 move + 1 close
             */
            out.reserve(size + 1, size / 3 + 2);
            out.moveTo(points[0]);
            for (size_t i = 1; i < size; i += 3) {
                out.cubicTo(points[i], points[i + 1], points[i + 2]);
            }
            if (mClosed) out.close();
        }
    } mPathInfo;

    ROString current() const { return mInput.midString(token.start, token.end - token.start); }
    ValueType toType() const { 
        switch(token.type) {
        case JSON::Token::Null : return Null;
        case JSON::Token::True: case JSON::Token::False: return Bool;
        case JSON::Token::String: return String;
        case JSON::Token::Object: return Object;
        case JSON::Token::Array: return Array;
        case JSON::Token::Number: return Number;
        case JSON::Token::Undefined:
        default: return Undefined;        
        }
    }

protected:
    std::unordered_map<std::string, VInterpolator *> mInterpolatorCache;
    std::shared_ptr<model::Composition>              mComposition;
    model::Composition *                             compRef{nullptr};
    model::Layer *                                   curLayerRef{nullptr};
    std::vector<model::Layer *>                      mLayersToUpdate;
    std::string                                      mDirPath;
    void                                             SkipOut(int depth);

    JSON                                             parser;
    JSON::Token                                      token;
    int32_t                                          mLastSuper;
    std::stack<int32_t>                              mSuperLIFO;
    ROString                                         mInput;
    int32_t                                          errorPos;
    Value                                            v_;
};

bool LottieParserImpl::VerifyType()
{
    /* Verify the media type is lottie json.
       Could add more strict check. */
    return ParseNext();
}

bool LottieParserImpl::ParseNext()
{
    // Are we done?
    if (parser.state == JSON::Done) return false;

    JSON::IndexType res = parser.parseOne(mInput.getData(), mInput.getLength(), token, mLastSuper);
    if (res < 0) {
        Error();
        printf("parse error: %d/%u\n", res, parser.pos);
        return false;
    }

    if (res == JSON::SaveSuper)
        mSuperLIFO.push(mLastSuper);
    if (res == JSON::RestoreSuper) {
        if (mSuperLIFO.size()) mSuperLIFO.pop();
        mLastSuper = mSuperLIFO.size() ? mSuperLIFO.top() : JSON::InvalidPos; 
    }
    if (res == JSON::Finished) {
        // We are done.
        return false;
    }

    // Then decide on what to do
    /*
    switch(token.parent) {
        case JSON::EnteringObject: return EnterObject();
        case JSON::EnteringArray: return EnterArray();
        case JSON::HadKey:        return NextObjectKey();
        case JSON::HadValue:  
    }
        vCritical << "Lottie file parsing error";
        */
    return true;
}

bool LottieParserImpl::EnterObject()
{
    ParseNext();
    return true;
}

bool LottieParserImpl::EnterArray()
{
    ParseNext();
    return true;
}

ROString LottieParserImpl::NextObjectKey()
{
    if (token.parent == JSON::HadKey) {
        ROString key = current();
        ParseNext();
        return key;
    }

    /* SPECIAL CASE
     * The parser works with a predefined rule that it will be only
     * while (NextObjectKey()) for each object but in case of our nested group
     * object we can call multiple time NextObjectKey() while exiting the object
     * so ignore those and don't put parser in the error state.
     * */
    if (token.parent == JSON::LeavingArray || token.parent == JSON::EnteringObject) {
        // #ifdef DEBUG_PARSER
        //         vDebug<<"Object: Exiting nested loop";
        // #endif
        return ROString();
    }

    if (token.parent != JSON::LeavingObject) {
        Error();
        return ROString();
    }

    ParseNext();
    return ROString();
}

bool LottieParserImpl::NextArrayValue()
{
    if (token.parent == JSON::LeavingArray) {
        ParseNext();
        return false;
    }

    /* SPECIAL CASE
     * same as  NextObjectKey()
     */
    if (token.parent == JSON::LeavingObject) {
        return false;
    }

    if (token.parent == JSON::HadKey) {
        Error();
        return false;
    }

    return true;
}

int LottieParserImpl::GetInt()
{
    if (token.parent != JSON::HadValue || token.type != JSON::Token::Number) {
        Error();
        return 0;
    }

    int result = (int)current();
    ParseNext();
    return result;
}

double LottieParserImpl::GetDouble()
{
    if (token.parent != JSON::HadValue || token.type != JSON::Token::Number) {
        Error();
        return 0.;
    }

    double result = (double)current();
    ParseNext();
    return result;
}

bool LottieParserImpl::GetBool()
{
    if (token.parent != JSON::HadValue || (token.type != JSON::Token::True && token.type != JSON::Token::False)) {
        Error();
        return false;
    }

    bool result = current() == "true";
    ParseNext();
    return result;
}

void LottieParserImpl::GetNull()
{
    if (token.parent != JSON::HadValue || token.type != JSON::Token::Null) {
        Error();
        return;
    }

    ParseNext();
}

ROString LottieParserImpl::GetString()
{
    if (token.parent != JSON::HadValue || token.type != JSON::Token::String) {
        Error();
        return ROString();
    }

    ROString result = current();
    ParseNext();
    return result;
}

std::string LottieParserImpl::GetStringObject()
{
    ROString str = GetString();

    if (str) {
        return std::string(str, str.getLength());
    }

    return {};
}

void LottieParserImpl::SkipOut(int depth)
{
    do {
        if (token.parent == JSON::EnteringArray || token.parent == JSON::EnteringObject) {
            ++depth;
        } else if (token.parent == JSON::LeavingArray || token.parent == JSON::LeavingObject) {
            --depth;
        } else if (errorPos != JSON::InvalidPos) {
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
    if (token.parent == JSON::HadValue) {
        v_.set(current(), token);
        return &v_;
    }

    return nullptr;
}

// returns a rapidjson::Type, or -1 for no value (at end of
// object/array)
LottieParserImpl::ValueType LottieParserImpl::PeekType() const
{
    return toType();
}

void LottieParserImpl::Skip(const ROString & /*key*/)
{
    if (PeekType() == Array) {
        EnterArray();
        SkipArray();
    } else if (PeekType() == Object) {
        EnterObject();
        SkipObject();
    } else {
        SkipValue();
    }
}

model::BlendMode LottieParserImpl::getBlendMode()
{
    auto mode = model::BlendMode::Normal;

    switch (GetInt()) {
    case 1:
        mode = model::BlendMode::Multiply;
        break;
    case 2:
        mode = model::BlendMode::Screen;
        break;
    case 3:
        mode = model::BlendMode::OverLay;
        break;
    default:
        break;
    }
    return mode;
}

void LottieParserImpl::resolveLayerRefs()
{
    for (const auto &layer : mLayersToUpdate) {
        auto search = compRef->mAssets.find(layer->extra()->mPreCompRefId);
        if (search != compRef->mAssets.end()) {
            if (layer->mLayerType == model::Layer::Type::Image) {
                layer->extra()->mAsset = search->second;
            } else if (layer->mLayerType == model::Layer::Type::Precomp) {
                layer->mChildren = search->second->mLayers;
                layer->setStatic(layer->isStatic() &&
                                 search->second->isStatic());
            }
        }
    }
}

void LottieParserImpl::parseComposition()
{
    EnterObject();
    std::shared_ptr<model::Composition> sharedComposition =
        std::make_shared<model::Composition>();
    model::Composition *comp = sharedComposition.get();
    compRef = comp;
    while (ROString key = NextObjectKey()) {
        switch(key.hash())
        {
        case "v"_hash: comp->mVersion = GetStringObject(); break;
        case "w"_hash: comp->mSize.setWidth(GetInt()); break;
        case "h"_hash: comp->mSize.setHeight(GetInt()); break;
        case "ip"_hash:  comp->mStartFrame = GetDouble(); break;
        case "op"_hash:  comp->mEndFrame = GetDouble(); break;
        case "fr"_hash:  comp->mFrameRate = GetDouble(); break;
        case "assets"_hash: parseAssets(comp); break;
        case "layers"_hash: parseLayers(comp); break;
        case "markers"_hash: parseMarkers(); break;
        default: 
#ifdef DEBUG_PARSER
            vWarning << "Composition Attribute Skipped : " << key;
#endif
            Skip(key);
            break;
        }
    }

    if (comp->mVersion.empty() || !comp->mRootLayer) {
        // don't have a valid bodymovin header
        return;
    }
    if (comp->mStartFrame > comp->mEndFrame) {
        // reveresed animation? missing data?
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

void LottieParserImpl::parseMarker()
{
    EnterObject();
    std::string comment;
    int         timeframe{0};
    int         duration{0};
    while (ROString key = NextObjectKey()) {
        switch (key.hash()) {
        case "cm"_hash: comment = GetStringObject(); break;
        case "tm"_hash: timeframe = GetDouble(); break;
        case "dr"_hash: duration = GetDouble(); break;
        default:
#ifdef DEBUG_PARSER
            vWarning << "Marker Attribute Skipped : " << key;
#endif
            Skip(key);
            break;
        }
    }
    compRef->mMarkers.emplace_back(std::move(comment), timeframe,
                                   timeframe + duration);
}

void LottieParserImpl::parseMarkers()
{
    EnterArray();
    while (NextArrayValue()) {
        parseMarker();
    }
    // update the precomp layers with the actual layer object
}

void LottieParserImpl::parseAssets(model::Composition *composition)
{
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
    auto         p = reinterpret_cast<const unsigned char *>(data);
    int          pad = len > 0 && (len % 4 || p[len - 1] == '=');
    const size_t L = ((len + 3) / 4 - pad) * 4;
    std::string  str(L / 4 * 3 + pad, '\0');

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
template <class T>
static std::string toString(const T &value)
{
    std::ostringstream os;
    os << value;
    return os.str();
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/layers/shape.json
 *
 */
model::Asset *LottieParserImpl::parseAsset()
{
    auto        asset = allocator().make<model::Asset>();
    std::string filename;
    std::string relativePath;
    bool        embededResource = false;
    EnterObject();
    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "w"_hash: asset->mWidth = GetInt(); break; 
        case "h"_hash: asset->mHeight = GetInt(); break; 
        case "p"_hash:  /* image name */
            asset->mAssetType = model::Asset::Type::Image;
            filename = GetStringObject();
        break; 
        case "u"_hash:  /* relative image path */
            relativePath = GetStringObject();
        break; 
        case "e"_hash:  /* relative image path */
            embededResource = GetInt();
        break; 
        case "id"_hash:  /* reference id*/
            if (PeekType() == String)
                asset->mRefId = GetStringObject();
            else asset->mRefId = toString(GetInt());
        break; 
        case "layers"_hash: {
            asset->mAssetType = model::Asset::Type::Precomp;
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
            break;
        }
        default: 
#ifdef DEBUG_PARSER
            vWarning << "Asset Attribute Skipped : " << key;
#endif
            Skip(key);
            break;
        }
    }

    if (asset->mAssetType == model::Asset::Type::Image) {
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

void LottieParserImpl::parseLayers(model::Composition *comp)
{
    comp->mRootLayer = allocator().make<model::Layer>();
    comp->mRootLayer->mLayerType = model::Layer::Type::Precomp;
    comp->mRootLayer->setName("__", 2);
    bool staticFlag = true;
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

model::Color LottieParserImpl::toColor(const ROString & str)
{
    if (!str) return {};

    model::Color color;
    size_t       len = str.getLength();

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

model::MatteType LottieParserImpl::getMatteType()
{
    switch (GetInt()) {
    case 1:
        return model::MatteType::Alpha;
        break;
    case 2:
        return model::MatteType::AlphaInv;
        break;
    case 3:
        return model::MatteType::Luma;
        break;
    case 4:
        return model::MatteType::LumaInv;
        break;
    default:
        return model::MatteType::None;
        break;
    }
}

model::Layer::Type LottieParserImpl::getLayerType()
{
    switch (GetInt()) {
    case 0:
        return model::Layer::Type::Precomp;
        break;
    case 1:
        return model::Layer::Type::Solid;
        break;
    case 2:
        return model::Layer::Type::Image;
        break;
    case 3:
        return model::Layer::Type::Null;
        break;
    case 4:
        return model::Layer::Type::Shape;
        break;
    case 5:
        return model::Layer::Type::Text;
        break;
    default:
        return model::Layer::Type::Null;
        break;
    }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/layers/shape.json
 *
 */
model::Layer *LottieParserImpl::parseLayer()
{
    model::Layer *layer = allocator().make<model::Layer>();
    curLayerRef = layer;
    bool ddd = true;
    EnterObject();
    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "ty"_hash: layer->mLayerType = getLayerType(); break;
        /*Layer index in AE. Used for parenting and expressions.*/
        case "ind"_hash: layer->mId = GetInt(); break; 
        /*3d layer */
        case "ddd"_hash: ddd = GetInt(); break;
        // "Layer Time Stretching"
        case "sr"_hash: layer->mTimeStreatch = GetDouble(); break;
        case "ip"_hash: layer->mInFrame = std::lround(GetDouble()); break;
        case "op"_hash: layer->mOutFrame = std::lround(GetDouble()); break;
        case "st"_hash: layer->mStartFrame = GetDouble(); break;
        case "bm"_hash: layer->mBlendMode = getBlendMode(); break;
        case "w"_hash:  layer->mLayerSize.setWidth(GetInt()); break;
        case "h"_hash:  layer->mLayerSize.setHeight(GetInt()); break;
        case "sw"_hash: layer->mLayerSize.setWidth(GetInt()); break;
        case "sh"_hash: layer->mLayerSize.setHeight(GetInt()); break;
        case "sc"_hash: layer->extra()->mSolidColor = toColor(GetString()); break;
        case "tt"_hash: layer->mMatteType = getMatteType(); break;
        case "ao"_hash: layer->mAutoOrient = GetInt(); break;
        case "hd"_hash: layer->setHidden(GetBool()); break;
        /*Layer Parent. Uses "ind" of parent.*/
        case "parent"_hash: layer->mParentId = GetInt(); break;
        case "hasMask"_hash: layer->mHasMask = GetBool(); break;
        // time remapping
        case "tm"_hash: parseProperty(layer->extra()->mTimeRemap); break;
        case "shapes"_hash: parseShapesAttr(layer); break;
        case "masksProperties"_hash: parseMaskProperty(layer); break;
        case "ks"_hash: {
            EnterObject();
            layer->mTransform = parseTransformObject(ddd);
            break;
        } 
        case "nm"_hash: { 
            ROString name = GetString(); 
            layer->setName(name.getData(), name.getLength()); 
            break; 
        }
        /*preComp Layer reference id*/
        case "refId"_hash: { 
            layer->extra()->mPreCompRefId = GetStringObject();
            layer->mHasGradient = true;
            mLayersToUpdate.push_back(layer);
        } break;
        default:
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
    if (layer->hasParent() && (layer->id() == layer->parentId()))
        return nullptr;

    if (layer->mExtra) layer->mExtra->mCompRef = compRef;

    if (layer->hidden()) {
        // if layer is hidden, only data that is usefull is its
        // transform matrix(when it is a parent of some other layer)
        // so force it to be a Null Layer and release all resource.
        layer->setStatic(layer->mTransform->isStatic());
        layer->mLayerType = model::Layer::Type::Null;
        layer->mChildren = {};
        return layer;
    }

    // update the static property of layer
    bool staticFlag = true;
    for (const auto &child : layer->mChildren) {
        staticFlag &= child->isStatic();
    }

    if (layer->hasMask() && layer->mExtra) {
        for (const auto &mask : layer->mExtra->mMasks) {
            staticFlag &= mask->isStatic();
        }
    }

    layer->setStatic(staticFlag && layer->mTransform->isStatic());

    return layer;
}

void LottieParserImpl::parseMaskProperty(model::Layer *layer)
{
    EnterArray();
    while (NextArrayValue()) {
        layer->extra()->mMasks.push_back(parseMaskObject());
    }
}

model::Mask *LottieParserImpl::parseMaskObject()
{
    auto obj = allocator().make<model::Mask>();

    EnterObject();
    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "inv"_hash: obj->mInv = GetBool(); break;
        case "mode"_hash: {
            ROString str = GetString();
            if (!str) {
                obj->mMode = model::Mask::Mode::None;
                continue;
            }
            switch (str[0]) {
            case 'n':
                obj->mMode = model::Mask::Mode::None;
                break;
            case 'a':
                obj->mMode = model::Mask::Mode::Add;
                break;
            case 's':
                obj->mMode = model::Mask::Mode::Substarct;
                break;
            case 'i':
                obj->mMode = model::Mask::Mode::Intersect;
                break;
            case 'f':
                obj->mMode = model::Mask::Mode::Difference;
                break;
            default:
                obj->mMode = model::Mask::Mode::None;
                break;
            }
            break;
        } 
        case "pt"_hash: parseShapeProperty(obj->mShape); break;
        case "o"_hash: parseProperty(obj->mOpacity); break;
        default:
            Skip(key);
        }
    }
    obj->mIsStatic = obj->mShape.isStatic() && obj->mOpacity.isStatic();
    return obj;
}

void LottieParserImpl::parseShapesAttr(model::Layer *layer)
{
    EnterArray();
    while (NextArrayValue()) {
        parseObject(layer);
    }
}

model::Object *LottieParserImpl::parseObjectTypeAttr()
{
    ROString type = GetString();
    switch(type.hash()) {
    case "gr"_hash: return parseGroupObject();
    case "rc"_hash: return parseRectObject();
    case "rd"_hash: {
        curLayerRef->mHasRoundedCorner = true;
        return parseRoundedCorner();
    }
    case "el"_hash: return parseEllipseObject();
    case "tr"_hash: return parseTransformObject();
    case "fl"_hash: return parseFillObject();
    case "st"_hash: return parseStrokeObject();
    case "gf"_hash: {
        curLayerRef->mHasGradient = true;
        return parseGFillObject();
    }
    case "gs"_hash: {       
        curLayerRef->mHasGradient = true;
        return parseGStrokeObject();
    }
    case "sh"_hash: return parseShapeObject();
    case "sr"_hash: return parsePolystarObject();
    case "tm"_hash: {
        curLayerRef->mHasPathOperator = true;
        return parseTrimObject();
    }
    case "rp"_hash: {
        curLayerRef->mHasRepeater = true;
        return parseReapeaterObject();
    }
    case "mm"_hash: 
        vWarning << "Merge Path is not supported yet";
        return nullptr;
    default:
#ifdef DEBUG_PARSER
        vDebug << "The Object Type not yet handled = " << type;
#endif
        return nullptr;
    }
}

void LottieParserImpl::parseObject(model::Group *parent)
{
    EnterObject();
    while (ROString key = NextObjectKey()) {
        if (key == "ty") {
            auto child = parseObjectTypeAttr();
            if (child && !child->hidden()) {
                if (child->type() == model::Object::Type::RoundedCorner) {
                    updateRoundedCorner(parent, static_cast<model::RoundedCorner *>(child));
                }
                parent->mChildren.push_back(child);
            }
        } else {
            Skip(key);
        }
    }
}

void LottieParserImpl::updateRoundedCorner(model::Group *group, model::RoundedCorner *rc)
{
    for(auto &e : group->mChildren)
    {
        if (e->type() == model::Object::Type::Rect) {
            static_cast<model::Rect *>(e)->mRoundedCorner = rc;
            if (!rc->isStatic()) {
                e->setStatic(false);
                group->setStatic(false);
                //@TODO need to propagate.
            }
        } else if ( e->type() == model::Object::Type::Group) {
            updateRoundedCorner(static_cast<model::Group *>(e), rc);
        }
    }
}

model::Object *LottieParserImpl::parseGroupObject()
{
    auto group = allocator().make<model::Group>();

    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "nm"_hash: { ROString name = GetString();  group->setName(name.getData(), name.getLength()); break; }
        case "it"_hash: {
            EnterArray();
            while (NextArrayValue()) {
                parseObject(group);
            }
            if (!group->mChildren.empty()
                    && group->mChildren.back()->type()
                            == model::Object::Type::Transform) {
                group->mTransform =
                    static_cast<model::Transform *>(group->mChildren.back());
                group->mChildren.pop_back();
            }
            break;
        }
        default:
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
model::Rect *LottieParserImpl::parseRectObject()
{
    auto obj = allocator().make<model::Rect>();

    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "nm"_hash: { ROString name = GetString();  obj->setName(name.getData(), name.getLength()); break; } 
        case "p"_hash: parseProperty(obj->mPos); break;
        case "s"_hash: parseProperty(obj->mSize); break;
        case "r"_hash: parseProperty(obj->mRound); break;
        case "d"_hash: obj->mDirection = GetInt(); break;
        case "hd"_hash: obj->setHidden(GetBool()); break;
        default:           Skip(key);
        }
    }
    obj->setStatic(obj->mPos.isStatic() && obj->mSize.isStatic() &&
                   obj->mRound.isStatic());
    return obj;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/rect.json
 */
model::RoundedCorner *LottieParserImpl::parseRoundedCorner()
{
    auto obj = allocator().make<model::RoundedCorner>();

    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "nm"_hash: { ROString name = GetString();  obj->setName(name.getData(), name.getLength()); break; } 
        case "r"_hash: parseProperty(obj->mRadius); break;
        case "hd"_hash: obj->setHidden(GetBool()); break;
        default:
            Skip(key);
        }
    }
    obj->setStatic(obj->mRadius.isStatic());
    return obj;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/ellipse.json
 */
model::Ellipse *LottieParserImpl::parseEllipseObject()
{
    auto obj = allocator().make<model::Ellipse>();

    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "nm"_hash: { ROString name = GetString();  obj->setName(name.getData(), name.getLength()); break; } 
        case "p"_hash: parseProperty(obj->mPos); break;
        case "s"_hash: parseProperty(obj->mSize); break;
        case "d"_hash: obj->mDirection = GetInt(); break;
        case "hd"_hash: obj->setHidden(GetBool()); break;
        default:
            Skip(key);
        }
    }
    obj->setStatic(obj->mPos.isStatic() && obj->mSize.isStatic());
    return obj;
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/shape.json
 */
model::Path *LottieParserImpl::parseShapeObject()
{
    auto obj = allocator().make<model::Path>();

    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "nm"_hash: { ROString name = GetString();  obj->setName(name.getData(), name.getLength()); break; } 
        case "ks"_hash: parseShapeProperty(obj->mShape); break;
        case "d"_hash:  obj->mDirection = GetInt(); break;
        case "hd"_hash: obj->setHidden(GetBool()); break;
        default:
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
model::Polystar *LottieParserImpl::parsePolystarObject()
{
    auto obj = allocator().make<model::Polystar>();

    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "nm"_hash: { ROString name = GetString();  obj->setName(name.getData(), name.getLength()); break; } 
        case "p"_hash:  parseProperty(obj->mPos);               break;
        case "pt"_hash: parseProperty(obj->mPointCount);        break;
        case "ir"_hash: parseProperty(obj->mInnerRadius);       break;
        case "is"_hash: parseProperty(obj->mInnerRoundness);    break;
        case "or"_hash: parseProperty(obj->mOuterRadius);       break;
        case "os"_hash: parseProperty(obj->mOuterRoundness);    break;
        case "r"_hash:  parseProperty(obj->mRotation);          break;
        case "sy"_hash: {
            int starType = GetInt();
            if (starType == 1) obj->mPolyType = model::Polystar::PolyType::Star;
            if (starType == 2)
                obj->mPolyType = model::Polystar::PolyType::Polygon;
            break;
        }
        case "d"_hash:  obj->mDirection = GetInt(); break;
        case "hd"_hash: obj->setHidden(GetBool());  break;
        default:
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

model::Trim::TrimType LottieParserImpl::getTrimType()
{
    switch (GetInt()) {
    case 1:
        return model::Trim::TrimType::Simultaneously;
        break;
    case 2:
        return model::Trim::TrimType::Individually;
        break;
    default:
        Error();
        return model::Trim::TrimType::Simultaneously;
        break;
    }
}

/*
 * https://github.com/airbnb/lottie-web/blob/master/docs/json/shapes/trim.json
 */
model::Trim *LottieParserImpl::parseTrimObject()
{
    auto obj = allocator().make<model::Trim>();

    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "nm"_hash: { ROString name = GetString();  obj->setName(name.getData(), name.getLength()); break; } 
        case "s"_hash:  parseProperty(obj->mStart);     break;
        case "e"_hash:  parseProperty(obj->mEnd);       break;
        case "o"_hash:  parseProperty(obj->mOffset);    break;
        case "m"_hash:  obj->mTrimType = getTrimType(); break;
        case "hd"_hash: obj->setHidden(GetBool());      break;
        default:
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

void LottieParserImpl::getValue(model::Repeater::Transform &obj)
{
    EnterObject();

    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "a"_hash: parseProperty(obj.mAnchor); break;
        case "p"_hash: parseProperty(obj.mPosition); break;
        case "r"_hash: parseProperty(obj.mRotation); break;
        case "s"_hash: parseProperty(obj.mScale); break;
        case "so"_hash: parseProperty(obj.mStartOpacity); break;
        case "eo"_hash: parseProperty(obj.mEndOpacity); break;
        default:
            Skip(key);
        }
    }
}

model::Repeater *LottieParserImpl::parseReapeaterObject()
{
    auto obj = allocator().make<model::Repeater>();

    obj->setContent(allocator().make<model::Group>());

    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "nm"_hash: { ROString name = GetString();  obj->setName(name.getData(), name.getLength()); break; } 
        case "c"_hash: {
            parseProperty(obj->mCopies);
            float maxCopy = 0.0;
            if (!obj->mCopies.isStatic()) {
                for (auto &keyFrame : obj->mCopies.animation().frames_) {
                    if (maxCopy < keyFrame.value_.start_)
                        maxCopy = keyFrame.value_.start_;
                    if (maxCopy < keyFrame.value_.end_)
                        maxCopy = keyFrame.value_.end_;
                }
            }
            else maxCopy = obj->mCopies.value();
            obj->mMaxCopies = maxCopy;
            break;
        }
        case "o"_hash:  parseProperty(obj->mOffset); break;
        case "tr"_hash: getValue(obj->mTransform); break;
        case "hd"_hash: obj->setHidden(GetBool()); break;
        default:
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
model::Transform *LottieParserImpl::parseTransformObject(bool ddd)
{
    auto objT = allocator().make<model::Transform>();

    auto obj = allocator().make<model::Transform::Data>();
    if (ddd) {
        obj->createExtraData();
        obj->mExtra->m3DData = true;
    }

    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "nm"_hash: { ROString name = GetString();  objT->setName(name.getData(), name.getLength()); break; } 
        case "a"_hash: parseProperty(obj->mAnchor);     break;
        case "r"_hash: parseProperty(obj->mRotation);   break;
        case "s"_hash: parseProperty(obj->mScale);      break;
        case "o"_hash: parseProperty(obj->mOpacity);    break;
        case "hd"_hash: objT->setHidden(GetBool());     break;
        case "rx"_hash: 
            if (!obj->mExtra) return nullptr;
            parseProperty(obj->mExtra->m3DRx);
            break;
        case "ry"_hash: 
            if (!obj->mExtra) return nullptr;
            parseProperty(obj->mExtra->m3DRy);
            break;
        case "rz"_hash: 
            if (!obj->mExtra) return nullptr;
            parseProperty(obj->mExtra->m3DRz);
            break;
        case "p"_hash: {
            EnterObject();
            bool separate = false;
            while (ROString k = NextObjectKey()) {
                switch(k.hash()) {
                case "k"_hash: parsePropertyHelper(obj->mPosition); break;
                case "s"_hash: 
                    obj->createExtraData();
                    obj->mExtra->mSeparate = GetBool();
                    separate = true;
                    break;
                case "x"_hash:  if (separate) parseProperty(obj->mExtra->mSeparateX); break;
                case "y"_hash: if (separate) parseProperty(obj->mExtra->mSeparateY); break;
                default:
                    Skip(key);
                }
            }
            break;
        }
        default:
            Skip(key);
        }
    }
    bool isStatic = obj->mAnchor.isStatic() && obj->mPosition.isStatic() &&
                    obj->mRotation.isStatic() && obj->mScale.isStatic() &&
                    obj->mOpacity.isStatic();
    if (obj->mExtra) {
        isStatic = isStatic && obj->mExtra->m3DRx.isStatic() &&
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
model::Fill *LottieParserImpl::parseFillObject()
{
    auto obj = allocator().make<model::Fill>();

    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "nm"_hash: { ROString name = GetString();  obj->setName(name.getData(), name.getLength()); break; } 
        case "c"_hash:             parseProperty(obj->mColor);      break;
        case "o"_hash:             parseProperty(obj->mOpacity);    break;
        case "fillEnabled"_hash:   obj->mEnabled = GetBool();       break;
        case "r"_hash:             obj->mFillRule = getFillRule();  break;
        case "hd"_hash:            obj->setHidden(GetBool());       break;
        default:
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
model::Stroke *LottieParserImpl::parseStrokeObject()
{
    auto obj = allocator().make<model::Stroke>();

    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "nm"_hash: { ROString name = GetString();  obj->setName(name.getData(), name.getLength()); break; } 
        case "c"_hash:             parseProperty(obj->mColor);          break;
        case "o"_hash:             parseProperty(obj->mOpacity);        break;
        case "w"_hash:             parseProperty(obj->mWidth);          break;
        case "fillEnabled"_hash:   obj->mEnabled = GetBool();           break;
        case "lc"_hash:            obj->mCapStyle = getLineCap();       break;
        case "lj"_hash:            obj->mJoinStyle = getLineJoin();     break;
        case "ml"_hash:            obj->mMiterLimit = GetDouble();      break;
        case "d"_hash:             parseDashProperty(obj->mDash);       break;
        case "hd"_hash:            obj->setHidden(GetBool());           break;
        default:
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

void LottieParserImpl::parseGradientProperty(model::Gradient *obj,
                                             const ROString & key)
{
    switch(key.hash()) {    
    case "t"_hash:  obj->mGradientType = GetInt();        break;
    case "o"_hash:  parseProperty(obj->mOpacity);         break;
    case "s"_hash:  parseProperty(obj->mStartPoint);      break;
    case "e"_hash:  parseProperty(obj->mEndPoint);        break;
    case "h"_hash:  parseProperty(obj->mHighlightLength); break;
    case "a"_hash:  parseProperty(obj->mHighlightAngle);  break;
    case "hd"_hash: obj->setHidden(GetBool());            break;
    case "g"_hash: {
        EnterObject();
        while (ROString k = NextObjectKey()) {
            if (k == "k") parseProperty(obj->mGradient);
            else if (k == "p") obj->mColorPoints = GetInt();
            else
            {
                Skip(k);
            }
        }
        break;
    }
    default:
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
model::GradientFill *LottieParserImpl::parseGFillObject()
{
    auto obj = allocator().make<model::GradientFill>();

    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "nm"_hash: { ROString name = GetString();  obj->setName(name.getData(), name.getLength()); break; } 
        case "r"_hash: obj->mFillRule = getFillRule(); break;
        default:
            parseGradientProperty(obj, key);
        }
    }
    return obj;
}

void LottieParserImpl::parseDashProperty(model::Dash &dash)
{
    EnterArray();
    while (NextArrayValue()) {
        EnterObject();
        while (ROString key = NextObjectKey()) {
            if (key == "v") {
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
model::GradientStroke *LottieParserImpl::parseGStrokeObject()
{
    auto obj = allocator().make<model::GradientStroke>();

    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "nm"_hash: { ROString name = GetString();  obj->setName(name.getData(), name.getLength()); break; } 
        case "w"_hash:   parseProperty(obj->mWidth);        break;
        case "lc"_hash:  obj->mCapStyle = getLineCap();     break;
        case "lj"_hash:  obj->mJoinStyle = getLineJoin();   break;
        case "ml"_hash:  obj->mMiterLimit = GetDouble();    break;
        case "d"_hash:   parseDashProperty(obj->mDash);     break;
        default:
            parseGradientProperty(obj, key);
        }
    }

    obj->setStatic(obj->isStatic() && obj->mWidth.isStatic() &&
                   obj->mDash.isStatic());
    return obj;
}

void LottieParserImpl::getValue(std::vector<VPointF> &v)
{
    EnterArray();
    while (NextArrayValue()) {
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

    if (PeekType() == Array) EnterArray();

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
    if (PeekType() == Array) {
        EnterArray();
        if (NextArrayValue()) val = GetDouble();
        // discard rest
        while (NextArrayValue()) {
            GetDouble();
        }
    } else if (PeekType() == Number) {
        val = GetDouble();
    } else {
        Error();
    }
}

void LottieParserImpl::getValue(model::Color &color)
{
    float val[4] = {0.f};
    int   i = 0;
    if (PeekType() == Array) EnterArray();

    while (NextArrayValue()) {
        const auto value = GetDouble();
        if (i < 4) {
            val[i++] = value;
        }
    }

    if (mColorFilter) mColorFilter(val[0], val[1], val[2]);

    color.r = val[0];
    color.g = val[1];
    color.b = val[2];
}

void LottieParserImpl::getValue(model::Gradient::Data &grad)
{
    if (PeekType() == Array) EnterArray();

    while (NextArrayValue()) {
        grad.mGradient.push_back(GetDouble());
    }
}

void LottieParserImpl::getValue(int &val)
{
    if (PeekType() == Array) {
        EnterArray();
        while (NextArrayValue()) {
            val = GetInt();
        }
    } else if (PeekType() == Number) {
        val = GetInt();
    } else {
        Error();
    }
}

void LottieParserImpl::parsePathInfo()
{
    mPathInfo.reset();

    /*
     * The shape object could be wrapped by a array
     * if its part of the keyframe object
     */
    bool arrayWrapper = (PeekType() == Array);
    if (arrayWrapper) EnterArray();

    EnterObject();
    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "i"_hash: getValue(mPathInfo.mInPoint); break;
        case "o"_hash: getValue(mPathInfo.mOutPoint); break;
        case "v"_hash: getValue(mPathInfo.mVertices); break;
        case "c"_hash: mPathInfo.mClosed = GetBool(); break;
        default:
            Error();
            Skip(key);
        }
    }
    // exit properly from the array
    if (arrayWrapper) NextArrayValue();

    mPathInfo.convert();
}

void LottieParserImpl::getValue(model::PathData &obj)
{
    parsePathInfo();
    obj.mPoints = mPathInfo.mResult;
    obj.mClosed = mPathInfo.mClosed;
}

VPointF LottieParserImpl::parseInperpolatorPoint()
{
    VPointF cp;
    EnterObject();
    while (ROString key = NextObjectKey()) {
        if (key == "x") {
            getValue(cp.rx());
        }
        else if (key == "y") {
            getValue(cp.ry());
        }
    }
    return cp;
}

template <typename T>
bool LottieParserImpl::parseKeyFrameValue(
    const ROString & key, model::Value<T, model::Position> &value)
{
    if (key == "ti") {
        value.hasTangent_ = true;
        getValue(value.inTangent_);
    } else if (key == "to") {
        value.hasTangent_ = true;
        getValue(value.outTangent_);
    } else {
        return false;
    }
    return true;
}

VInterpolator *LottieParserImpl::interpolator(VPointF     inTangent,
                                              VPointF     outTangent,
                                              std::string key)
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
template <typename T, typename Tag>
void LottieParserImpl::parseKeyFrame(model::KeyFrames<T, Tag> &obj)
{
    struct ParsedField {
        std::string interpolatorKey;
        bool        interpolator{false};
        bool        value{false};
        bool        hold{false};
        bool        noEndValue{true};
    };

    EnterObject();
    ParsedField                              parsed;
    typename model::KeyFrames<T, Tag>::Frame keyframe;
    VPointF                                  inTangent;
    VPointF                                  outTangent;

    while (ROString key = NextObjectKey()) {
        switch(key.hash()) {
        case "i"_hash: {
            parsed.interpolator = true;
            inTangent = parseInperpolatorPoint();
            break;
        }
        case "o"_hash: outTangent = parseInperpolatorPoint();   break;
        case "t"_hash: keyframe.start_ = GetDouble();           break;
        case "s"_hash: {
            parsed.value = true;
            getValue(keyframe.value_.start_);
            continue;
        }
        case "e"_hash: {
            parsed.noEndValue = false;
            getValue(keyframe.value_.end_);
            continue;
        }
        case "n"_hash: {
            if (PeekType() == String) {
                parsed.interpolatorKey = GetStringObject();
            } else {
                EnterArray();
                while (NextArrayValue()) {
                    if (parsed.interpolatorKey.empty()) {
                        parsed.interpolatorKey = GetStringObject();
                    } else {
                        // skip rest of the string
                        Skip(key);
                    }
                }
            }
            continue;
        }
        case "h"_hash: {
            parsed.hold = GetInt();
            continue;
        }
        default:
            if (parseKeyFrameValue(key, keyframe.value_)) continue;
#ifdef DEBUG_PARSER
            vDebug << "key frame property skipped = " << key;
#endif
            Skip(key);
        }
    }

    auto &list = obj.frames_;
    if (!list.empty()) {
        // update the endFrame value of current keyframe
        list.back().end_ = keyframe.start_;
        // if no end value provided, copy start value to previous frame
        if (parsed.value && parsed.noEndValue) {
            list.back().value_.end_ = keyframe.value_.start_;
        }
    }

    if (parsed.hold) {
        keyframe.value_.end_ = keyframe.value_.start_;
        keyframe.end_ = keyframe.start_;
        list.push_back(std::move(keyframe));
    } else if (parsed.interpolator) {
        keyframe.interpolator_ = interpolator(
            inTangent, outTangent, std::move(parsed.interpolatorKey));
        list.push_back(std::move(keyframe));
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
void LottieParserImpl::parseShapeProperty(model::Property<model::PathData> &obj)
{
    EnterObject();
    while (ROString key = NextObjectKey()) {
        if (key == "k") {
            if (PeekType() == Array) {
                EnterArray();
                while (NextArrayValue()) {
                    parseKeyFrame(obj.animation());
                }
            } else {
                if (!obj.isStatic()) {
                    Error();
                    return;
                }
                getValue(obj.value());
            }
        } else {
#ifdef DEBUG_PARSER
            vDebug << "shape property ignored = " << key;
#endif
            Skip(key);
        }
    }
    obj.cache();
}

template <typename T, typename Tag>
void LottieParserImpl::parsePropertyHelper(model::Property<T, Tag> &obj)
{
    if (PeekType() == Number) {
        if (!obj.isStatic()) {
            Error();
            return;
        }
        /*single value property with no animation*/
        getValue(obj.value());
    } else {
        EnterArray();
        while (NextArrayValue()) {
            /* property with keyframe info*/
            if (PeekType() == Object) {
                parseKeyFrame(obj.animation());
            } else {
                /* Read before modifying.
                 * as there is no way of knowing if the
                 * value of the array is either array of numbers
                 * or array of object without entering the array
                 * thats why this hack is there
                 */
                if (!obj.isStatic()) {
                    Error();
                    return;
                }
                /*multi value property with no animation*/
                getValue(obj.value());
                /*break here as we already reached end of array*/
                break;
            }
        }
        obj.cache();
    }
}

/*
 * https://github.com/airbnb/lottie-web/tree/master/docs/json/properties
 */
template <typename T>
void LottieParserImpl::parseProperty(model::Property<T> &obj)
{
    EnterObject();
    while (ROString key = NextObjectKey()) {
        if (key == "k") {
            parsePropertyHelper(obj);
        } else {
            Skip(key);
        }
    }
}

#ifdef LOTTIE_DUMP_TREE_SUPPORT

class ObjectInspector {
public:
    void visit(model::Composition *obj, std::string level)
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
    void visit(model::Layer *obj, std::string level)
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

        if (obj->mLayerType == model::Layer::Type::Image)
            vDebug << level << "\t{ "
                   << "ImageInfo:"
                   << " W :" << obj->extra()->mAsset->mWidth
                   << ", H :" << obj->extra()->mAsset->mHeight << " }"
                   << "\n";
        else {
            vDebug << level;
        }
        visitChildren(static_cast<model::Group *>(obj), level);
        vDebug << level << "} " << layerType(obj->mLayerType).c_str()
               << ", id: " << obj->mId << "\n";
    }
    void visitChildren(model::Group *obj, std::string level)
    {
        level.append("\t");
        for (const auto &child : obj->mChildren) visit(child, level);
        if (obj->mTransform) visit(obj->mTransform, level);
    }

    void visit(model::Object *obj, std::string level)
    {
        switch (obj->type()) {
        case model::Object::Type::Repeater: {
            auto r = static_cast<model::Repeater *>(obj);
            vDebug << level << "{ Repeater: name: " << obj->name()
                   << " , a:" << !obj->isStatic()
                   << ", copies:" << r->maxCopies()
                   << ", offset:" << r->offset(0);
            visitChildren(r->mContent, level);
            vDebug << level << "} Repeater";
            break;
        }
        case model::Object::Type::Group: {
            vDebug << level << "{ Group: name: " << obj->name()
                   << " , a:" << !obj->isStatic();
            visitChildren(static_cast<model::Group *>(obj), level);
            vDebug << level << "} Group";
            break;
        }
        case model::Object::Type::Layer: {
            visit(static_cast<model::Layer *>(obj), level);
            break;
        }
        case model::Object::Type::Trim: {
            vDebug << level << "{ Trim: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case model::Object::Type::Rect: {
            vDebug << level << "{ Rect: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case model::Object::Type::RoundedCorner: {
            vDebug << level << "{ RoundedCorner: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case model::Object::Type::Ellipse: {
            vDebug << level << "{ Ellipse: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case model::Object::Type::Path: {
            vDebug << level << "{ Shape: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case model::Object::Type::Polystar: {
            vDebug << level << "{ Polystar: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case model::Object::Type::Transform: {
            vDebug << level << "{ Transform: name: " << obj->name()
                   << " , a: " << !obj->isStatic() << " }";
            break;
        }
        case model::Object::Type::Stroke: {
            vDebug << level << "{ Stroke: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case model::Object::Type::GStroke: {
            vDebug << level << "{ GStroke: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case model::Object::Type::Fill: {
            vDebug << level << "{ Fill: name: " << obj->name()
                   << " , a:" << !obj->isStatic() << " }";
            break;
        }
        case model::Object::Type::GFill: {
            auto f = static_cast<model::GradientFill *>(obj);
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

    std::string matteType(model::MatteType type)
    {
        switch (type) {
        case model::MatteType::None:
            return "Matte::None";
            break;
        case model::MatteType::Alpha:
            return "Matte::Alpha";
            break;
        case model::MatteType::AlphaInv:
            return "Matte::AlphaInv";
            break;
        case model::MatteType::Luma:
            return "Matte::Luma";
            break;
        case model::MatteType::LumaInv:
            return "Matte::LumaInv";
            break;
        default:
            return "Matte::Unknown";
            break;
        }
    }
    std::string layerType(model::Layer::Type type)
    {
        switch (type) {
        case model::Layer::Type::Precomp:
            return "Layer::Precomp";
            break;
        case model::Layer::Type::Null:
            return "Layer::Null";
            break;
        case model::Layer::Type::Shape:
            return "Layer::Shape";
            break;
        case model::Layer::Type::Solid:
            return "Layer::Solid";
            break;
        case model::Layer::Type::Image:
            return "Layer::Image";
            break;
        case model::Layer::Type::Text:
            return "Layer::Text";
            break;
        default:
            return "Layer::Unknown";
            break;
        }
    }
};

#endif

std::shared_ptr<model::Composition> model::parse(const char *       str, size_t len,
                                                 std::string        dir_path,
                                                 model::ColorFilter filter)
{
    LottieParserImpl obj(str, len, std::move(dir_path), std::move(filter));

    if (obj.VerifyType()) {
        obj.parseComposition();
        auto composition = obj.composition();
        if (composition) {
            composition->processRepeaterObjects();
            composition->updateStats();

#ifdef LOTTIE_DUMP_TREE_SUPPORT
            ObjectInspector inspector;
            inspector.visit(composition.get(), "");
#endif

            return composition;
        }
    }

    vWarning << "Input data is not Lottie format!";
    return {};
}

