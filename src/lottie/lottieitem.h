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

#ifndef LOTTIEITEM_H
#define LOTTIEITEM_H

#include<sstream>
#include<memory>

#include"lottieproxymodel.h"
#include"vmatrix.h"
#include"vpath.h"
#include"vpoint.h"
#include"vpathmesure.h"
#include"rlottiecommon.h"
#include"rlottie.h"
#include"vpainter.h"
#include"vdrawable.h"
#include"lottiekeypath.h"
#include"varenaalloc.h"

V_USE_NAMESPACE

enum class DirtyFlagBit : uchar
{
   None   = 0x00,
   Matrix = 0x01,
   Alpha  = 0x02,
   All    = (Matrix | Alpha)
};

class LOTLayerItem;
class LOTMaskItem;
class VDrawable;

class LOTDrawable : public VDrawable
{
public:
    void sync();
public:
    std::unique_ptr<LOTNode>  mCNode{nullptr};

    ~LOTDrawable() {
        if (mCNode && mCNode->mGradient.stopPtr)
          free(mCNode->mGradient.stopPtr);
    }
};

class SurfaceCache
{
public:  
  SurfaceCache(){mCache.reserve(10);}

  VBitmap make_surface(size_t width, size_t height, VBitmap::Format format=VBitmap::Format::ARGB32_Premultiplied)
  {
    if (mCache.empty()) return {width, height, format};

    auto surface = mCache.back();
    surface.reset(width, height, format);

    mCache.pop_back();
    return surface;
  }

  void release_surface(VBitmap& surface)
  {
     mCache.push_back(surface);
  }

private:
  std::vector<VBitmap> mCache;
};

class LOTCompItem
{
public:
   explicit LOTCompItem(LOTModel *model);
   bool update(int frameNo, const VSize &size, bool keepAspectRatio);
   VSize size() const { return mViewSize;}
   void buildRenderTree();
   const LOTLayerNode * renderTree()const;
   bool render(const rlottie::Surface &surface);
   void setValue(const std::string &keypath, LOTVariant &value);
private:
   SurfaceCache                                mSurfaceCache;
   VBitmap                                     mSurface;
   VMatrix                                     mScaleMatrix;
   VSize                                       mViewSize;
   LOTCompositionData                         *mCompData{nullptr};
   LOTLayerItem                               *mRootLayer{nullptr};
   VArenaAlloc                                 mAllocator{2048};
   int                                         mCurFrameNo;
   bool                                        mKeepAspectRatio{true};
};

class LOTLayerMaskItem;

class LOTClipperItem
{
public:
    explicit LOTClipperItem(VSize size): mSize(size){}
    void update(const VMatrix &matrix);
    void preprocess(const VRect &clip);
    VRle rle(const VRle& mask);
public:
    VSize                    mSize;
    VPath                    mPath;
    VRle                     mMaskedRle;
    VRasterizer              mRasterizer;
    bool                     mRasterRequest{false};
};

typedef vFlag<DirtyFlagBit> DirtyFlag;

struct LOTCApiData
{
    LOTCApiData();
    LOTLayerNode                  mLayer;
    std::vector<LOTMask>          mMasks;
    std::vector<LOTLayerNode *>   mLayers;
    std::vector<LOTNode *>        mCNodeList;
};

template< class T>
class VSpan
{
public:
    using reference         = T &;
    using pointer           = T *;
    using const_pointer     = T const *;
    using const_reference   = T const &;
    using index_type        = size_t;

    using iterator          = pointer;
    using const_iterator    = const_pointer;

    VSpan() = default;
    VSpan(pointer data, index_type size):_data(data), _size(size){}

    constexpr pointer data() const noexcept {return _data; }
    constexpr index_type size() const noexcept {return _size; }
    constexpr bool empty() const noexcept { return size() == 0 ;}
    constexpr iterator begin() const noexcept { return data(); }
    constexpr iterator end() const noexcept {return data() + size() ;}
    constexpr const_iterator cbegin() const noexcept {return  data();}
    constexpr const_iterator cend() const noexcept { return data() + size();}
    constexpr reference operator[]( index_type idx ) const { return *( data() + idx );}

private:
    pointer      _data{nullptr};
    index_type   _size{0};
};

using DrawableList = VSpan<VDrawable *>;

class LOTLayerItem
{
public:
   virtual ~LOTLayerItem() = default;
   LOTLayerItem& operator=(LOTLayerItem&&) noexcept = delete;
   LOTLayerItem(LOTLayerData *layerData);
   int id() const {return mLayerData->id();}
   int parentId() const {return mLayerData->parentId();}
   void setParentLayer(LOTLayerItem *parent){mParentLayer = parent;}
   void setComplexContent(bool value) { mComplexContent = value;}
   bool complexContent() const {return mComplexContent;}
   virtual void update(int frameNo, const VMatrix &parentMatrix, float parentAlpha);
   VMatrix matrix(int frameNo) const;
   void preprocess(const VRect& clip);
   virtual DrawableList renderList(){ return {};}
   virtual void render(VPainter *painter, const VRle &mask, const VRle &matteRle, SurfaceCache& cache);
   bool hasMatte() { if (mLayerData->mMatteType == MatteType::None) return false; return true; }
   MatteType matteType() const { return mLayerData->mMatteType;}
   bool visible() const;
   virtual void buildLayerNode();
   LOTLayerNode& clayer() {return mCApiData->mLayer;}
   std::vector<LOTLayerNode *>& clayers() {return mCApiData->mLayers;}
   std::vector<LOTMask>& cmasks() {return mCApiData->mMasks;}
   std::vector<LOTNode *>& cnodes() {return mCApiData->mCNodeList;}
   const char* name() const {return mLayerData->name();}
   virtual bool resolveKeyPath(LOTKeyPath &keyPath, uint depth, LOTVariant &value);
protected:
   virtual void preprocessStage(const VRect& clip) = 0;
   virtual void updateContent() = 0;
   inline VMatrix combinedMatrix() const {return mCombinedMatrix;}
   inline int frameNo() const {return mFrameNo;}
   inline float combinedAlpha() const {return mCombinedAlpha;}
   inline bool isStatic() const {return mLayerData->isStatic();}
   float opacity(int frameNo) const {return mLayerData->opacity(frameNo);}
   inline DirtyFlag flag() const {return mDirtyFlag;}
   bool skipRendering() const {return (!visible() || vIsZero(combinedAlpha()));}
protected:
   std::unique_ptr<LOTLayerMaskItem>           mLayerMask;
   LOTLayerData                               *mLayerData{nullptr};
   LOTLayerItem                               *mParentLayer{nullptr};
   VMatrix                                     mCombinedMatrix;
   float                                       mCombinedAlpha{0.0};
   int                                         mFrameNo{-1};
   DirtyFlag                                   mDirtyFlag{DirtyFlagBit::All};
   bool                                        mComplexContent{false};
   std::unique_ptr<LOTCApiData>                mCApiData;
};

class LOTCompLayerItem: public LOTLayerItem
{
public:
   explicit LOTCompLayerItem(LOTLayerData *layerData, VArenaAlloc* allocator);

   void render(VPainter *painter, const VRle &mask, const VRle &matteRle, SurfaceCache& cache) final;
   void buildLayerNode() final;
   bool resolveKeyPath(LOTKeyPath &keyPath, uint depth, LOTVariant &value) override;
protected:
   void preprocessStage(const VRect& clip) final;
   void updateContent() final;
private:
    void renderHelper(VPainter *painter, const VRle &mask, const VRle &matteRle, SurfaceCache& cache);
    void renderMatteLayer(VPainter *painter, const VRle &inheritMask, const VRle &matteRle,
                          LOTLayerItem *layer, LOTLayerItem *src, SurfaceCache& cache);
private:
   std::vector<LOTLayerItem*>            mLayers;
   std::unique_ptr<LOTClipperItem>       mClipper;
};

class LOTSolidLayerItem: public LOTLayerItem
{
public:
   explicit LOTSolidLayerItem(LOTLayerData *layerData);
   void buildLayerNode() final;
   DrawableList renderList() final;
protected:
   void preprocessStage(const VRect& clip) final;
   void updateContent() final;
private:
   LOTDrawable                  mRenderNode;
   VDrawable                   *mDrawableList{nullptr}; //to work with the Span api
};

class LOTContentItem;
class LOTContentGroupItem;
class LOTShapeLayerItem: public LOTLayerItem
{
public:
   explicit LOTShapeLayerItem(LOTLayerData *layerData, VArenaAlloc* allocator);
   DrawableList renderList() final;
   void buildLayerNode() final;
   bool resolveKeyPath(LOTKeyPath &keyPath, uint depth, LOTVariant &value) override;
protected:
   void preprocessStage(const VRect& clip) final;
   void updateContent() final;
   std::vector<VDrawable *>             mDrawableList;
   LOTContentGroupItem                 *mRoot{nullptr};
};

class LOTNullLayerItem: public LOTLayerItem
{
public:
   explicit LOTNullLayerItem(LOTLayerData *layerData);
protected:
   void preprocessStage(const VRect&) final {}
   void updateContent() final;
};

class LOTImageLayerItem: public LOTLayerItem
{
public:
   explicit LOTImageLayerItem(LOTLayerData *layerData);
   void buildLayerNode() final;
   DrawableList renderList() final;
protected:
   void preprocessStage(const VRect& clip) final;
   void updateContent() final;
private:
   LOTDrawable                  mRenderNode;
   VTexture                     mTexture;
   VDrawable                   *mDrawableList{nullptr}; //to work with the Span api
};

class LOTMaskItem
{
public:
    explicit LOTMaskItem(LOTMaskData *data): mData(data){}
    void update(int frameNo, const VMatrix &parentMatrix, float parentAlpha, const DirtyFlag &flag);
    LOTMaskData::Mode maskMode() const { return mData->mMode;}
    VRle rle();
    void preprocess(const VRect &clip);
public:
    LOTMaskData             *mData{nullptr};
    VPath                    mLocalPath;
    VPath                    mFinalPath;
    VRasterizer              mRasterizer;
    float                    mCombinedAlpha{0};
    bool                     mRasterRequest{false};
};

class LottieTextPath {
public:
    VPath path;
    float x_advance;
    float y_advance;
    float size;
};

class LOTTextLayerItem: public LOTLayerItem
{
public:
   explicit LOTTextLayerItem(LOTLayerData *layerData, VArenaAlloc* allocator);
   DrawableList renderList() final;
   void buildLayerNode() final;
   bool resolveKeyPath(LOTKeyPath &keyPath, uint depth, LOTVariant &value) override;
protected:
   void preprocessStage(const VRect& clip) final;
   void updateContent() final;
   LOTContentGroupItem                       *mRoot{nullptr};
   LOTTextProperties                         curTextProperties;
   std::vector<std::unique_ptr<LOTDrawable>> mRenderNode;
   std::vector<VDrawable *>                  mDrawableList;
   std::vector<LottieTextPath>               mLastTextPathList;

   void getTextPath(std::vector<LottieTextPath> &textPathList, int frameNo) {
       auto curTextProperties = mLayerData->extra()->textLayer()->getTextProperties(frameNo);

       if (mLayerData->extra()->mCompRef &&
               !mLayerData->extra()->mCompRef->mChars.empty()) {
           for (auto textChar : curTextProperties.mText) {
               // TODO: Multiline support
               /*
               if ((textChar == '\r') || (textChar == '\r')) {
                   curLineNum++;
                   continue;
               }
                */
               for (auto  charData : mLayerData->extra()->mCompRef->mChars) {
                   if ((textChar == *charData.mCh.c_str()) &&
                           (curTextProperties.mSize == charData.mSize) &&
                           (mLayerData->extra()->mCompRef->compareFontFamily(curTextProperties.mFont, charData.mFontFamily) == 0)) {
                       textPathList.emplace_back();
                       auto &textPath = textPathList.back();

                       for (auto shapeData : charData.mShapePathData) {
                           textPath.path.addPath(shapeData);
                           break;
                       }
                       textPath.x_advance = charData.mWidth;
                       textPath.y_advance = curTextProperties.mLineHeight;
                       textPath.size = charData.mSize;
                   }
               }
           }
           mLastTextPathList = textPathList;
       }
   }

   bool isStatic() {
       return mLayerData->extra()->textLayer()->isStatic();
   }

   void getLottieTextProperties(LottieTextProperties &obj, int frameNo) {
        mLayerData->extra()->textLayer()->getLottieTextProperties(obj, frameNo);
   }

   void doStroke(VPath &path, LottieColor &strokeColor, float opacity, float strokeWidth) {
       auto strokeDrawable = std::make_unique<LOTDrawable>();
       mRenderNode.push_back(std::move(strokeDrawable));
       auto renderNode = mRenderNode.back().get();

       renderNode->setType(VDrawable::Type::Stroke);
       renderNode->mFlag |= VDrawable::DirtyState::Path;
       renderNode->mPath = path;

       VBrush strokeBrush(strokeColor.r * 255, strokeColor.g * 255, strokeColor.b * 255, opacity / 100 * 255);
       renderNode->setBrush(strokeBrush);

       // FIXME: The magic number 1.5!
       renderNode->setStrokeInfo(CapStyle::Flat, JoinStyle::Miter,
               10.0, strokeWidth * 1.5);
       renderNode->mFlag |= VDrawable::DirtyState::Stroke;
   }

   void doFill(VPath &path, LottieColor &fillColor, float opacity) {
       auto fillDrawable = std::make_unique<LOTDrawable>();
       mRenderNode.push_back(std::move(fillDrawable));
       auto renderNode = mRenderNode.back().get();
       renderNode->mFlag |= VDrawable::DirtyState::Path;
       renderNode->mPath = path;

       VBrush fillBrush(fillColor.r * 255, fillColor.g * 255, fillColor.b * 255, opacity / 100 * 255);
       renderNode->setBrush(fillBrush);
       renderNode->mFlag |= VDrawable::DirtyState::Brush;
   }
};

/*
 * Handels mask property of a layer item
 */
class LOTLayerMaskItem
{
public:
    explicit LOTLayerMaskItem(LOTLayerData *layerData);
    void update(int frameNo, const VMatrix &parentMatrix, float parentAlpha, const DirtyFlag &flag);
    bool isStatic() const {return mStatic;}
    VRle maskRle(const VRect &clipRect);
    void preprocess(const VRect &clip);
public:
    std::vector<LOTMaskItem>   mMasks;
    VRle                       mRle;
    bool                       mStatic{true};
    bool                       mDirty{true};
};

class LOTPathDataItem;
class LOTPaintDataItem;
class LOTTrimItem;

enum class ContentType : uchar
{
    Unknown,
    Group,
    Path,
    Paint,
    Trim
};

class LOTContentGroupItem;
class LOTContentItem
{
public:
   virtual ~LOTContentItem() = default;
   LOTContentItem& operator=(LOTContentItem&&) noexcept = delete;
   virtual void update(int frameNo, const VMatrix &parentMatrix, float parentAlpha, const DirtyFlag &flag) = 0;   virtual void renderList(std::vector<VDrawable *> &){}
   virtual bool resolveKeyPath(LOTKeyPath &, uint, LOTVariant &) {return false;}
   virtual ContentType type() const {return ContentType::Unknown;}
};

class LOTContentGroupItem: public LOTContentItem
{
public:
   LOTContentGroupItem() = default;
   explicit LOTContentGroupItem(LOTGroupData *data, VArenaAlloc* allocator);
   void addChildren(LOTGroupData *data, VArenaAlloc* allocator);
   void update(int frameNo, const VMatrix &parentMatrix, float parentAlpha, const DirtyFlag &flag) override;
   void applyTrim();
   void processTrimItems(std::vector<LOTPathDataItem *> &list);
   void processPaintItems(std::vector<LOTPathDataItem *> &list);
   void renderList(std::vector<VDrawable *> &list) override;
   ContentType type() const final {return ContentType::Group;}
   const VMatrix & matrix() const { return mMatrix;}
   const char* name() const
   {
       static const char* TAG = "__";
       return mModel.hasModel() ? mModel.name() : TAG;
   }
   bool resolveKeyPath(LOTKeyPath &keyPath, uint depth, LOTVariant &value) override;
protected:
   std::vector<LOTContentItem*>   mContents;
   VMatrix                                        mMatrix;
private:
   LOTProxyModel<LOTGroupData> mModel;
};

class LOTPathDataItem : public LOTContentItem
{
public:
   LOTPathDataItem(bool staticPath): mStaticPath(staticPath){}
   void update(int frameNo, const VMatrix &parentMatrix, float parentAlpha, const DirtyFlag &flag) final;
   ContentType type() const final {return ContentType::Path;}
   bool dirty() const {return mDirtyPath;}
   const VPath &localPath() const {return mTemp;}
   void finalPath(VPath& result);
   void updatePath(const VPath &path) {mTemp = path; mDirtyPath = true;}
   bool staticPath() const { return mStaticPath; }
   void setParent(LOTContentGroupItem *parent) {mParent = parent;}
   LOTContentGroupItem *parent() const {return mParent;}
protected:
   virtual void updatePath(VPath& path, int frameNo) = 0;
   virtual bool hasChanged(int prevFrame, int curFrame) = 0;
private:
   bool hasChanged(int frameNo) {
       int prevFrame = mFrameNo;
       mFrameNo = frameNo;
       if (prevFrame == -1) return true;
       if (mStaticPath ||
           (prevFrame == frameNo)) return false;
       return hasChanged(prevFrame, frameNo);
   }
   LOTContentGroupItem                    *mParent{nullptr};
   VPath                                   mLocalPath;
   VPath                                   mTemp;
   int                                     mFrameNo{-1};
   bool                                    mDirtyPath{true};
   bool                                    mStaticPath;
};

class LOTRectItem: public LOTPathDataItem
{
public:
   explicit LOTRectItem(LOTRectData *data);
protected:
   void updatePath(VPath& path, int frameNo) final;
   LOTRectData           *mData{nullptr};

   bool hasChanged(int prevFrame, int curFrame) final {
       return (mData->mPos.changed(prevFrame, curFrame) ||
               mData->mSize.changed(prevFrame, curFrame) ||
               mData->mRound.changed(prevFrame, curFrame));
   }
};

class LOTEllipseItem: public LOTPathDataItem
{
public:
   explicit LOTEllipseItem(LOTEllipseData *data);
private:
   void updatePath(VPath& path, int frameNo) final;
   LOTEllipseData           *mData{nullptr};
   bool hasChanged(int prevFrame, int curFrame) final {
       return (mData->mPos.changed(prevFrame, curFrame) ||
               mData->mSize.changed(prevFrame, curFrame));
   }
};

class LOTShapeItem: public LOTPathDataItem
{
public:
   explicit LOTShapeItem(LOTShapeData *data);
private:
   void updatePath(VPath& path, int frameNo) final;
   LOTShapeData             *mData{nullptr};
   bool hasChanged(int prevFrame, int curFrame) final {
       return mData->mShape.changed(prevFrame, curFrame);
   }
};

class LOTPolystarItem: public LOTPathDataItem
{
public:
   explicit LOTPolystarItem(LOTPolystarData *data);
private:
   void updatePath(VPath& path, int frameNo) final;
   LOTPolystarData             *mData{nullptr};

   bool hasChanged(int prevFrame, int curFrame) final {
       return (mData->mPos.changed(prevFrame, curFrame) ||
               mData->mPointCount.changed(prevFrame, curFrame) ||
               mData->mInnerRadius.changed(prevFrame, curFrame) ||
               mData->mOuterRadius.changed(prevFrame, curFrame) ||
               mData->mInnerRoundness.changed(prevFrame, curFrame) ||
               mData->mOuterRoundness.changed(prevFrame, curFrame) ||
               mData->mRotation.changed(prevFrame, curFrame));
   }
};



class LOTPaintDataItem : public LOTContentItem
{
public:
   LOTPaintDataItem(bool staticContent);
   void addPathItems(std::vector<LOTPathDataItem *> &list, size_t startOffset);
   void update(int frameNo, const VMatrix &parentMatrix, float parentAlpha, const DirtyFlag &flag) override;
   void renderList(std::vector<VDrawable *> &list) final;
   ContentType type() const final {return ContentType::Paint;}
protected:
   virtual bool updateContent(int frameNo, const VMatrix &matrix, float alpha) = 0;
private:
   void updateRenderNode();
protected:
   std::vector<LOTPathDataItem *>   mPathItems;
   LOTDrawable                      mDrawable;
   VPath                            mPath;
   DirtyFlag                        mFlag;
   bool                             mStaticContent;
   bool                             mRenderNodeUpdate{true};
   bool                             mContentToRender{true};
};

class LOTFillItem : public LOTPaintDataItem
{
public:
   explicit LOTFillItem(LOTFillData *data);
protected:
   bool updateContent(int frameNo, const VMatrix &matrix, float alpha) final;
   bool resolveKeyPath(LOTKeyPath &keyPath, uint depth, LOTVariant &value) final;
private:
   LOTProxyModel<LOTFillData> mModel;
};

class LOTGFillItem : public LOTPaintDataItem
{
public:
   explicit LOTGFillItem(LOTGFillData *data);
protected:
   bool updateContent(int frameNo, const VMatrix &matrix, float alpha) final;
private:
   LOTGFillData                 *mData{nullptr};
   std::unique_ptr<VGradient>    mGradient;
};

class LOTStrokeItem : public LOTPaintDataItem
{
public:
   explicit LOTStrokeItem(LOTStrokeData *data);
protected:
   bool updateContent(int frameNo, const VMatrix &matrix, float alpha) final;
   bool resolveKeyPath(LOTKeyPath &keyPath, uint depth, LOTVariant &value) final;
private:
   LOTProxyModel<LOTStrokeData> mModel;
};

class LOTGStrokeItem : public LOTPaintDataItem
{
public:
   explicit LOTGStrokeItem(LOTGStrokeData *data);
protected:
   bool updateContent(int frameNo, const VMatrix &matrix, float alpha) final;
private:
   LOTGStrokeData               *mData{nullptr};
   std::unique_ptr<VGradient>    mGradient;
};


// Trim Item

class LOTTrimItem : public LOTContentItem
{
public:
   explicit LOTTrimItem(LOTTrimData *data);
   void update(int frameNo, const VMatrix &parentMatrix, float parentAlpha, const DirtyFlag &flag) final;
   ContentType type() const final {return ContentType::Trim;}
   void update();
   void addPathItems(std::vector<LOTPathDataItem *> &list, size_t startOffset);
private:
   bool pathDirty() const {
       for (auto &i : mPathItems) {
           if (i->dirty())
               return true;
       }
       return false;
   }
   struct Cache {
        int                     mFrameNo{-1};
        LOTTrimData::Segment    mSegment{};
   };
   Cache                            mCache;
   std::vector<LOTPathDataItem *>   mPathItems;
   LOTTrimData                     *mData{nullptr};
   VPathMesure                      mPathMesure;
   bool                             mDirty{true};
};

class LOTRepeaterItem : public LOTContentGroupItem
{
public:
   explicit LOTRepeaterItem(LOTRepeaterData *data, VArenaAlloc* allocator);
   void update(int frameNo, const VMatrix &parentMatrix, float parentAlpha, const DirtyFlag &flag) final;
   void renderList(std::vector<VDrawable *> &list) final;
private:
   LOTRepeaterData             *mRepeaterData{nullptr};
   bool                         mHidden{false};
   int                          mCopies{0};
};


#endif // LOTTIEITEM_H


