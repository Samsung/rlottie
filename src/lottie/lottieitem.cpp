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

#include "lottieitem.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include "lottiekeypath.h"
#include "vbitmap.h"
#include "vpainter.h"
#include "vraster.h"

/* Lottie Layer Rules
 * 1. time stretch is pre calculated and applied to all the properties of the
 * lottilayer model and all its children
 * 2. The frame property could be reversed using,time-reverse layer property in
 * AE. which means (start frame > endFrame) 3.
 */

static bool transformProp(rlottie::Property prop)
{
    switch (prop) {
    case rlottie::Property::TrAnchor:
    case rlottie::Property::TrScale:
    case rlottie::Property::TrOpacity:
    case rlottie::Property::TrPosition:
    case rlottie::Property::TrRotation:
        return true;
    default:
        return false;
    }
}
static bool fillProp(rlottie::Property prop)
{
    switch (prop) {
    case rlottie::Property::FillColor:
    case rlottie::Property::FillOpacity:
        return true;
    default:
        return false;
    }
}

static bool strokeProp(rlottie::Property prop)
{
    switch (prop) {
    case rlottie::Property::StrokeColor:
    case rlottie::Property::StrokeOpacity:
    case rlottie::Property::StrokeWidth:
        return true;
    default:
        return false;
    }
}

LOTCompItem::LOTCompItem(LOTModel *model)
    : mCurFrameNo(-1)
{
    mCompData = model->mRoot.get();
    mRootLayer = createLayerItem(mCompData->mRootLayer.get());
    mRootLayer->setComplexContent(false);
    mViewSize = mCompData->size();
}

void LOTCompItem::setValue(const std::string &keypath, LOTVariant &value)
{
    LOTKeyPath key(keypath);
    mRootLayer->resolveKeyPath(key, 0, value);
}

std::unique_ptr<LOTLayerItem> LOTCompItem::createLayerItem(
    LOTLayerData *layerData)
{
    switch (layerData->mLayerType) {
    case LayerType::Precomp: {
        return std::make_unique<LOTCompLayerItem>(layerData);
    }
    case LayerType::Solid: {
        return std::make_unique<LOTSolidLayerItem>(layerData);
    }
    case LayerType::Shape: {
        return std::make_unique<LOTShapeLayerItem>(layerData);
    }
    case LayerType::Null: {
        return std::make_unique<LOTNullLayerItem>(layerData);
    }
    case LayerType::Image: {
        return std::make_unique<LOTImageLayerItem>(layerData);
    }
    default:
        return nullptr;
        break;
    }
}

bool LOTCompItem::update(int frameNo, const VSize &size, bool keepAspectRatio)
{
    // check if cached frame is same as requested frame.
    if ((mViewSize == size) &&
        (mCurFrameNo == frameNo) &&
        (mKeepAspectRatio == keepAspectRatio)) return false;

    mViewSize = size;
    mCurFrameNo = frameNo;
    mKeepAspectRatio = keepAspectRatio;

    /*
     * if viewbox dosen't scale exactly to the viewport
     * we scale the viewbox keeping AspectRatioPreserved and then align the
     * viewbox to the viewport using AlignCenter rule.
     */
    VMatrix m;
    VSize viewPort = mViewSize;
    VSize viewBox = mCompData->size();
    float sx = float(viewPort.width()) / viewBox.width();
    float sy = float(viewPort.height()) / viewBox.height();
    if (mKeepAspectRatio) {
        float scale = std::min(sx, sy);
        float tx = (viewPort.width() - viewBox.width() * scale) * 0.5f;
        float ty = (viewPort.height() - viewBox.height() * scale) * 0.5f;
        m.translate(tx, ty).scale(scale, scale);
    } else {
       m.scale(sx, sy);
    }
    mRootLayer->update(frameNo, m, 1.0);
    return true;
}

bool LOTCompItem::render(const rlottie::Surface &surface)
{
    mSurface.reset(reinterpret_cast<uchar *>(surface.buffer()),
                   uint(surface.width()), uint(surface.height()), uint(surface.bytesPerLine()),
                   VBitmap::Format::ARGB32_Premultiplied);

    /* schedule all preprocess task for this frame at once.
     */
    VRect clip(0, 0, int(surface.drawRegionWidth()), int(surface.drawRegionHeight()));
    mRootLayer->preprocess(clip);

    mPainter.begin(&mSurface);
    // set sub surface area for drawing.
    mPainter.setDrawRegion(
        VRect(int(surface.drawRegionPosX()), int(surface.drawRegionPosY()),
              int(surface.drawRegionWidth()), int(surface.drawRegionHeight())));
    mRootLayer->render(&mPainter, {}, {});
    mPainter.end();
    return true;
}

void LOTMaskItem::update(int frameNo, const VMatrix &            parentMatrix,
                         float /*parentAlpha*/, const DirtyFlag &flag)
{
    if (flag.testFlag(DirtyFlagBit::None) && mData->isStatic()) return;

    if (mData->mShape.isStatic()) {
        if (mLocalPath.empty()) {
            mData->mShape.updatePath(frameNo, mLocalPath);
        }
    } else {
        mData->mShape.updatePath(frameNo, mLocalPath);
    }
    /* mask item dosen't inherit opacity */
    mCombinedAlpha = mData->opacity(frameNo);

    mFinalPath.clone(mLocalPath);
    mFinalPath.transform(parentMatrix);

    mRasterRequest = true;
}

VRle LOTMaskItem::rle()
{
    if (mRasterRequest) {
        mRasterRequest = false;
        if (!vCompare(mCombinedAlpha, 1.0f))
            mRasterizer.rle() *= uchar(mCombinedAlpha * 255);
        if (mData->mInv) mRasterizer.rle().invert();
    }
    return mRasterizer.rle();
}

void LOTMaskItem::preprocess(const VRect &clip)
{
    if (mRasterRequest) mRasterizer.rasterize(mFinalPath, FillRule::Winding, clip);
}

void LOTLayerItem::render(VPainter *painter, const VRle &inheritMask,
                          const VRle &matteRle)
{
    mDrawableList.clear();
    renderList(mDrawableList);

    VRle mask;
    if (mLayerMask) {
        mask = mLayerMask->maskRle(painter->clipBoundingRect());
        if (!inheritMask.empty()) mask = mask & inheritMask;
        // if resulting mask is empty then return.
        if (mask.empty()) return;
    } else {
        mask = inheritMask;
    }

    for (auto &i : mDrawableList) {
        painter->setBrush(i->mBrush);
        VRle rle = i->rle();
        if (matteRle.empty()) {
            if (mask.empty()) {
                // no mask no matte
                painter->drawRle(VPoint(), rle);
            } else {
                // only mask
                painter->drawRle(rle, mask);
            }

        } else {
            if (!mask.empty()) rle = rle & mask;

            if (rle.empty()) continue;
            if (matteType() == MatteType::AlphaInv) {
                rle = rle - matteRle;
                painter->drawRle(VPoint(), rle);
            } else {
                // render with matteRle as clip.
                painter->drawRle(rle, matteRle);
            }
        }
    }
}

void LOTLayerMaskItem::preprocess(const VRect &clip)
{
    for (auto &i : mMasks) {
        i.preprocess(clip);
    }
}


LOTLayerMaskItem::LOTLayerMaskItem(LOTLayerData *layerData)
{
    if (!layerData->mExtra) return;

    mMasks.reserve(layerData->mExtra->mMasks.size());

    for (auto &i : layerData->mExtra->mMasks) {
        mMasks.emplace_back(i.get());
        mStatic &= i->isStatic();
    }
}

void LOTLayerMaskItem::update(int frameNo, const VMatrix &parentMatrix,
                              float parentAlpha, const DirtyFlag &flag)
{
    if (flag.testFlag(DirtyFlagBit::None) && isStatic()) return;

    for (auto &i : mMasks) {
        i.update(frameNo, parentMatrix, parentAlpha, flag);
    }
    mDirty = true;
}

VRle LOTLayerMaskItem::maskRle(const VRect &clipRect)
{
    if (!mDirty) return mRle;

    VRle rle;
    for (auto &i : mMasks) {
        switch (i.maskMode()) {
        case LOTMaskData::Mode::Add: {
            rle = rle + i.rle();
            break;
        }
        case LOTMaskData::Mode::Substarct: {
            if (rle.empty() && !clipRect.empty()) rle = VRle::toRle(clipRect);
            rle = rle - i.rle();
            break;
        }
        case LOTMaskData::Mode::Intersect: {
            if (rle.empty() && !clipRect.empty()) rle = VRle::toRle(clipRect);
            rle = rle & i.rle();
            break;
        }
        case LOTMaskData::Mode::Difference: {
            rle = rle ^ i.rle();
            break;
        }
        default:
            break;
        }
    }

    if (!rle.empty() && !rle.unique()) {
        mRle.clone(rle);
    } else {
        mRle = rle;
    }
    mDirty = false;
    return mRle;
}

LOTLayerItem::LOTLayerItem(LOTLayerData *layerData) : mLayerData(layerData)
{
    if (mLayerData->mHasMask)
        mLayerMask = std::make_unique<LOTLayerMaskItem>(mLayerData);
}

bool LOTLayerItem::resolveKeyPath(LOTKeyPath &keyPath, uint depth,
                                  LOTVariant &value)
{
    if (!keyPath.matches(name(), depth)) {
        return false;
    }

    if (!keyPath.skip(name())) {
        if (keyPath.fullyResolvesTo(name(), depth) &&
            transformProp(value.property())) {
            //@TODO handle propery update.
        }
    }
    return true;
}

bool LOTShapeLayerItem::resolveKeyPath(LOTKeyPath &keyPath, uint depth,
                                       LOTVariant &value)
{
    if (LOTLayerItem::resolveKeyPath(keyPath, depth, value)) {
        if (keyPath.propagate(name(), depth)) {
            uint newDepth = keyPath.nextDepth(name(), depth);
            mRoot->resolveKeyPath(keyPath, newDepth, value);
        }
        return true;
    }
    return false;
}

bool LOTCompLayerItem::resolveKeyPath(LOTKeyPath &keyPath, uint depth,
                                      LOTVariant &value)
{
    if (LOTLayerItem::resolveKeyPath(keyPath, depth, value)) {
        if (keyPath.propagate(name(), depth)) {
            uint newDepth = keyPath.nextDepth(name(), depth);
            for (const auto &layer : mLayers) {
                layer->resolveKeyPath(keyPath, newDepth, value);
            }
        }
        return true;
    }
    return false;
}

void LOTLayerItem::update(int frameNumber, const VMatrix &parentMatrix,
                          float parentAlpha)
{
    mFrameNo = frameNumber;
    // 1. check if the layer is part of the current frame
    if (!visible()) return;

    float alpha = parentAlpha * opacity(frameNo());
    if (vIsZero(alpha)) {
        mCombinedAlpha = 0;
        return;
    }

    // 2. calculate the parent matrix and alpha
    VMatrix m = matrix(frameNo());
    m *= parentMatrix;

    // 3. update the dirty flag based on the change
    if (mCombinedMatrix != m) {
        mDirtyFlag |= DirtyFlagBit::Matrix;
        mCombinedMatrix = m;
    }

    if (!vCompare(mCombinedAlpha, alpha)) {
        mDirtyFlag |= DirtyFlagBit::Alpha;
        mCombinedAlpha = alpha;
    }

    // 4. update the mask
    if (mLayerMask) {
        mLayerMask->update(frameNo(), mCombinedMatrix, mCombinedAlpha,
                           mDirtyFlag);
    }

    // 5. if no parent property change and layer is static then nothing to do.
    if (!mLayerData->precompLayer() && flag().testFlag(DirtyFlagBit::None) &&
        isStatic())
        return;

    // 6. update the content of the layer
    updateContent();

    // 7. reset the dirty flag
    mDirtyFlag = DirtyFlagBit::None;
}

VMatrix LOTLayerItem::matrix(int frameNo) const
{
    return mParentLayer
               ? (mLayerData->matrix(frameNo) * mParentLayer->matrix(frameNo))
               : mLayerData->matrix(frameNo);
}

bool LOTLayerItem::visible() const
{
    return (frameNo() >= mLayerData->inFrame() &&
            frameNo() < mLayerData->outFrame());
}

void LOTLayerItem::preprocess(const VRect& clip)
{
    // layer dosen't contribute to the frame
    if (skipRendering()) return;

    // preprocess layer masks
    if (mLayerMask) mLayerMask->preprocess(clip);

    preprocessStage(clip);
}

LOTCompLayerItem::LOTCompLayerItem(LOTLayerData *layerModel)
    : LOTLayerItem(layerModel)
{
    if (!mLayerData->mChildren.empty())
        mLayers.reserve(mLayerData->mChildren.size());

    // 1. keep the layer in back-to-front order.
    // as lottie model keeps the data in front-toback-order.
    for (auto it = mLayerData->mChildren.crbegin();
         it != mLayerData->mChildren.rend(); ++it ) {
        auto model = static_cast<LOTLayerData *>((*it).get());
        auto item = LOTCompItem::createLayerItem(model);
        if (item) mLayers.push_back(std::move(item));
    }

    // 2. update parent layer
    for (const auto &layer : mLayers) {
        int id = layer->parentId();
        if (id >= 0) {
            auto search =
                std::find_if(mLayers.begin(), mLayers.end(),
                             [id](const auto &val) { return val->id() == id; });
            if (search != mLayers.end()) layer->setParentLayer((*search).get());
        }
    }

    // 4. check if its a nested composition
    if (!layerModel->layerSize().empty()) {
        mClipper = std::make_unique<LOTClipperItem>(layerModel->layerSize());
    }

    if (mLayers.size() > 1) setComplexContent(true);
}

void LOTCompLayerItem::render(VPainter *painter, const VRle &inheritMask,
                              const VRle &matteRle)
{
    if (vIsZero(combinedAlpha())) return;

    if (vCompare(combinedAlpha(), 1.0)) {
        renderHelper(painter, inheritMask, matteRle);
    } else {
        if (complexContent()) {
            VSize    size = painter->clipBoundingRect().size();
            VPainter srcPainter;
            VBitmap  srcBitmap(size.width(), size.height(),
                              VBitmap::Format::ARGB32_Premultiplied);
            srcPainter.begin(&srcBitmap);
            renderHelper(&srcPainter, inheritMask, matteRle);
            srcPainter.end();
            painter->drawBitmap(VPoint(), srcBitmap, uchar(combinedAlpha() * 255.0f));
        } else {
            renderHelper(painter, inheritMask, matteRle);
        }
    }
}

void LOTCompLayerItem::renderHelper(VPainter *painter, const VRle &inheritMask,
                                    const VRle &matteRle)
{
    VRle mask;
    if (mLayerMask) {
        mask = mLayerMask->maskRle(painter->clipBoundingRect());
        if (!inheritMask.empty()) mask = mask & inheritMask;
        // if resulting mask is empty then return.
        if (mask.empty()) return;
    } else {
        mask = inheritMask;
    }

    if (mClipper) {
        mask = mClipper->rle(mask);
        if (mask.empty()) return;
    }

    LOTLayerItem *matte = nullptr;
    for (const auto &layer : mLayers) {
        if (layer->hasMatte()) {
            matte = layer.get();
        } else {
            if (layer->visible()) {
                if (matte) {
                    if (matte->visible())
                        renderMatteLayer(painter, mask, matteRle, matte,
                                         layer.get());
                } else {
                    layer->render(painter, mask, matteRle);
                }
            }
            matte = nullptr;
        }
    }
}

void LOTCompLayerItem::renderMatteLayer(VPainter *painter, const VRle &mask,
                                        const VRle &  matteRle,
                                        LOTLayerItem *layer, LOTLayerItem *src)
{
    VSize size = painter->clipBoundingRect().size();
    // Decide if we can use fast matte.
    // 1. draw src layer to matte buffer
    VPainter srcPainter;
    src->bitmap().reset(size.width(), size.height(),
                        VBitmap::Format::ARGB32_Premultiplied);
    srcPainter.begin(&src->bitmap());
    src->render(&srcPainter, mask, matteRle);
    srcPainter.end();

    // 2. draw layer to layer buffer
    VPainter layerPainter;
    layer->bitmap().reset(size.width(), size.height(),
                          VBitmap::Format::ARGB32_Premultiplied);
    layerPainter.begin(&layer->bitmap());
    layer->render(&layerPainter, mask, matteRle);

    // 2.1update composition mode
    switch (layer->matteType()) {
    case MatteType::Alpha:
    case MatteType::Luma: {
        layerPainter.setCompositionMode(
            VPainter::CompositionMode::CompModeDestIn);
        break;
    }
    case MatteType::AlphaInv:
    case MatteType::LumaInv: {
        layerPainter.setCompositionMode(
            VPainter::CompositionMode::CompModeDestOut);
        break;
    }
    default:
        break;
    }

    // 2.2 update srcBuffer if the matte is luma type
    if (layer->matteType() == MatteType::Luma ||
        layer->matteType() == MatteType::LumaInv) {
        src->bitmap().updateLuma();
    }

    // 2.3 draw src buffer as mask
    layerPainter.drawBitmap(VPoint(), src->bitmap());
    layerPainter.end();
    // 3. draw the result buffer into painter
    painter->drawBitmap(VPoint(), layer->bitmap());
}

void LOTClipperItem::update(const VMatrix &matrix)
{
    mPath.reset();
    mPath.addRect(VRectF(0, 0, mSize.width(), mSize.height()));
    mPath.transform(matrix);
    mRasterRequest = true;
}

void LOTClipperItem::preprocess(const VRect &clip)
{
    if (mRasterRequest)
        mRasterizer.rasterize(mPath, FillRule::Winding, clip);

    mRasterRequest = false;
}

VRle LOTClipperItem::rle(const VRle& mask)
{
    if (mask.empty())
        return mRasterizer.rle();

    mMaskedRle.clone(mask);
    mMaskedRle &= mRasterizer.rle();
    return mMaskedRle;
}

void LOTCompLayerItem::updateContent()
{
    if (mClipper && flag().testFlag(DirtyFlagBit::Matrix)) {
        mClipper->update(combinedMatrix());
    }
    int   mappedFrame = mLayerData->timeRemap(frameNo());
    float alpha = combinedAlpha();
    if (complexContent()) alpha = 1;
    for (const auto &layer : mLayers) {
        layer->update(mappedFrame, combinedMatrix(), alpha);
    }
}

void LOTCompLayerItem::preprocessStage(const VRect &clip)
{
    // if layer has clipper
    if (mClipper) mClipper->preprocess(clip);

    LOTLayerItem *matte = nullptr;
    for (const auto &layer : mLayers) {
        if (layer->hasMatte()) {
            matte = layer.get();
        } else {
            if (layer->visible()) {
                if (matte) {
                    if (matte->visible()) {
                        layer->preprocess(clip);
                        matte->preprocess(clip);
                    }
                } else {
                    layer->preprocess(clip);
                }
            }
            matte = nullptr;
        }
    }
}
void LOTCompLayerItem::renderList(std::vector<VDrawable *> &list)
{
    if (skipRendering()) return;

    LOTLayerItem *matte = nullptr;
    for (const auto &layer : mLayers) {
        if (layer->hasMatte()) {
            matte = layer.get();
        } else {
            if (layer->visible()) {
                if (matte) {
                    if (matte->visible()) {
                        layer->renderList(list);
                        matte->renderList(list);
                    }
                } else {
                    layer->renderList(list);
                }
            }
            matte = nullptr;
        }
    }
}

LOTSolidLayerItem::LOTSolidLayerItem(LOTLayerData *layerData)
    : LOTLayerItem(layerData)
{
}

void LOTSolidLayerItem::updateContent()
{
    if (flag() & DirtyFlagBit::Matrix) {
        VPath path;
        path.addRect(
            VRectF(0, 0,
                   mLayerData->layerSize().width(),
                   mLayerData->layerSize().height()));
        path.transform(combinedMatrix());
        mRenderNode.mFlag |= VDrawable::DirtyState::Path;
        mRenderNode.mPath = path;
    }
    if (flag() & DirtyFlagBit::Alpha) {
        LottieColor color = mLayerData->solidColor();
        VBrush      brush(color.toColor(combinedAlpha()));
        mRenderNode.setBrush(brush);
        mRenderNode.mFlag |= VDrawable::DirtyState::Brush;
    }
}

void LOTSolidLayerItem::preprocessStage(const VRect& clip)
{
    mRenderNode.preprocess(clip);
}

void LOTSolidLayerItem::renderList(std::vector<VDrawable *> &list)
{
    if (skipRendering()) return;

    list.push_back(&mRenderNode);
}

LOTImageLayerItem::LOTImageLayerItem(LOTLayerData *layerData)
    : LOTLayerItem(layerData)
{
    if (!mLayerData->asset()) return;

    mTexture.mBitmap = mLayerData->asset()->bitmap();
    VBrush brush(&mTexture);
    mRenderNode.setBrush(brush);
}

void LOTImageLayerItem::updateContent()
{
    if (!mLayerData->asset()) return;

    if (flag() & DirtyFlagBit::Matrix) {
        VPath path;
        path.addRect(VRectF(0, 0, mLayerData->asset()->mWidth,
                            mLayerData->asset()->mHeight));
        path.transform(combinedMatrix());
        mRenderNode.mFlag |= VDrawable::DirtyState::Path;
        mRenderNode.mPath = path;
        mTexture.mMatrix = combinedMatrix();
    }

    if (flag() & DirtyFlagBit::Alpha) {
        mTexture.mAlpha = int(combinedAlpha() * 255);
    }
}

void LOTImageLayerItem::preprocessStage(const VRect& clip)
{
    mRenderNode.preprocess(clip);
}

void LOTImageLayerItem::renderList(std::vector<VDrawable *> &list)
{
    if (skipRendering()) return;

    list.push_back(&mRenderNode);
}

LOTNullLayerItem::LOTNullLayerItem(LOTLayerData *layerData)
    : LOTLayerItem(layerData)
{
}
void LOTNullLayerItem::updateContent() {}

LOTShapeLayerItem::LOTShapeLayerItem(LOTLayerData *layerData)
    : LOTLayerItem(layerData),
      mRoot(std::make_unique<LOTContentGroupItem>(nullptr))
{
    mRoot->addChildren(layerData);

    std::vector<LOTPathDataItem *> list;
    mRoot->processPaintItems(list);

    if (layerData->hasPathOperator()) {
        list.clear();
        mRoot->processTrimItems(list);
    }
}

std::unique_ptr<LOTContentItem> LOTShapeLayerItem::createContentItem(
    LOTData *contentData)
{
    switch (contentData->type()) {
    case LOTData::Type::ShapeGroup: {
        return std::make_unique<LOTContentGroupItem>(
            static_cast<LOTGroupData *>(contentData));
    }
    case LOTData::Type::Rect: {
        return std::make_unique<LOTRectItem>(
            static_cast<LOTRectData *>(contentData));
    }
    case LOTData::Type::Ellipse: {
        return std::make_unique<LOTEllipseItem>(
            static_cast<LOTEllipseData *>(contentData));
    }
    case LOTData::Type::Shape: {
        return std::make_unique<LOTShapeItem>(
            static_cast<LOTShapeData *>(contentData));
    }
    case LOTData::Type::Polystar: {
        return std::make_unique<LOTPolystarItem>(
            static_cast<LOTPolystarData *>(contentData));
    }
    case LOTData::Type::Fill: {
        return std::make_unique<LOTFillItem>(
            static_cast<LOTFillData *>(contentData));
    }
    case LOTData::Type::GFill: {
        return std::make_unique<LOTGFillItem>(
            static_cast<LOTGFillData *>(contentData));
    }
    case LOTData::Type::Stroke: {
        return std::make_unique<LOTStrokeItem>(
            static_cast<LOTStrokeData *>(contentData));
    }
    case LOTData::Type::GStroke: {
        return std::make_unique<LOTGStrokeItem>(
            static_cast<LOTGStrokeData *>(contentData));
    }
    case LOTData::Type::Repeater: {
        return std::make_unique<LOTRepeaterItem>(
            static_cast<LOTRepeaterData *>(contentData));
    }
    case LOTData::Type::Trim: {
        return std::make_unique<LOTTrimItem>(
            static_cast<LOTTrimData *>(contentData));
    }
    default:
        return nullptr;
        break;
    }
}

void LOTShapeLayerItem::updateContent()
{
    mRoot->update(frameNo(), combinedMatrix(), combinedAlpha(), flag());

    if (mLayerData->hasPathOperator()) {
        mRoot->applyTrim();
    }
}

void LOTShapeLayerItem::preprocessStage(const VRect& clip)
{
    mDrawableList.clear();
    mRoot->renderList(mDrawableList);

    for (auto &drawable : mDrawableList) drawable->preprocess(clip);

}

void LOTShapeLayerItem::renderList(std::vector<VDrawable *> &list)
{
    if (skipRendering()) return;
    mRoot->renderList(list);
}

bool LOTContentGroupItem::resolveKeyPath(LOTKeyPath &keyPath, uint depth,
                                         LOTVariant &value)
{
    if (!keyPath.matches(name(), depth)) {
        return false;
    }

    if (!keyPath.skip(name())) {
        if (keyPath.fullyResolvesTo(name(), depth) &&
            transformProp(value.property())) {
            //@TODO handle property update
        }
    }

    if (keyPath.propagate(name(), depth)) {
        uint newDepth = keyPath.nextDepth(name(), depth);
        for (auto &child : mContents) {
            child->resolveKeyPath(keyPath, newDepth, value);
        }
    }
    return true;
}

bool LOTFillItem::resolveKeyPath(LOTKeyPath &keyPath, uint depth,
                                 LOTVariant &value)
{
    if (!keyPath.matches(mModel.name(), depth)) {
        return false;
    }

    if (keyPath.fullyResolvesTo(mModel.name(), depth) &&
        fillProp(value.property())) {
        mModel.filter().addValue(value);
        return true;
    }
    return false;
}

bool LOTStrokeItem::resolveKeyPath(LOTKeyPath &keyPath, uint depth,
                                   LOTVariant &value)
{
    if (!keyPath.matches(mModel.name(), depth)) {
        return false;
    }

    if (keyPath.fullyResolvesTo(mModel.name(), depth) &&
        strokeProp(value.property())) {
        mModel.filter().addValue(value);
        return true;
    }
    return false;
}

LOTContentGroupItem::LOTContentGroupItem(LOTGroupData *data)
    : mData(data)
{
    addChildren(mData);
}

void LOTContentGroupItem::addChildren(LOTGroupData *data)
{
    if (!data) return;

    if (!data->mChildren.empty()) mContents.reserve(data->mChildren.size());

    // keep the content in back-to-front order.
    // as lottie model keeps it in front-to-back order.
    for (auto it = data->mChildren.crbegin(); it != data->mChildren.rend();
         ++it ) {
        auto content = LOTShapeLayerItem::createContentItem((*it).get());
        if (content) {
            mContents.push_back(std::move(content));
        }
    }
}

void LOTContentGroupItem::update(int frameNo, const VMatrix &parentMatrix,
                                 float parentAlpha, const DirtyFlag &flag)
{
    DirtyFlag newFlag = flag;
    float alpha;

    if (mData && mData->mTransform) {
        VMatrix m = mData->mTransform->matrix(frameNo);
        m *= parentMatrix;

        if (!(flag & DirtyFlagBit::Matrix) && !mData->mTransform->isStatic() &&
            (m != mMatrix)) {
            newFlag |= DirtyFlagBit::Matrix;
        }

        mMatrix = m;

        alpha = parentAlpha * mData->mTransform->opacity(frameNo);
        if (!vCompare(alpha, parentAlpha)) {
            newFlag |= DirtyFlagBit::Alpha;
        }
    } else {
        mMatrix = parentMatrix;
        alpha = parentAlpha;
    }

    for (const auto &content : mContents) {
        content->update(frameNo, matrix(), alpha, newFlag);
    }
}

void LOTContentGroupItem::applyTrim()
{
    for (auto i = mContents.rbegin(); i != mContents.rend(); ++i) {
        auto content = (*i).get();
        switch (content->type()) {
        case ContentType::Trim: {
            static_cast<LOTTrimItem *>(content)->update();
            break;
        }
        case ContentType::Group: {
            static_cast<LOTContentGroupItem *>(content)->applyTrim();
            break;
        }
        default:
            break;
        }
    }
}

void LOTContentGroupItem::renderList(std::vector<VDrawable *> &list)
{
    for (const auto &content : mContents) {
        content->renderList(list);
    }
}

void LOTContentGroupItem::processPaintItems(
    std::vector<LOTPathDataItem *> &list)
{
    size_t curOpCount = list.size();
    for (auto i = mContents.rbegin(); i != mContents.rend(); ++i) {
        auto content = (*i).get();
        switch (content->type()) {
        case ContentType::Path: {
            auto pathItem = static_cast<LOTPathDataItem *>(content);
            pathItem->setParent(this);
            list.push_back(pathItem);
            break;
        }
        case ContentType::Paint: {
            static_cast<LOTPaintDataItem *>(content)->addPathItems(list,
                                                                   curOpCount);
            break;
        }
        case ContentType::Group: {
            static_cast<LOTContentGroupItem *>(content)->processPaintItems(
                list);
            break;
        }
        default:
            break;
        }
    }
}

void LOTContentGroupItem::processTrimItems(std::vector<LOTPathDataItem *> &list)
{
    size_t curOpCount = list.size();
    for (auto i = mContents.rbegin(); i != mContents.rend(); ++i) {
        auto content = (*i).get();

        switch (content->type()) {
        case ContentType::Path: {
            list.push_back(static_cast<LOTPathDataItem *>(content));
            break;
        }
        case ContentType::Trim: {
            static_cast<LOTTrimItem *>(content)->addPathItems(list, curOpCount);
            break;
        }
        case ContentType::Group: {
            static_cast<LOTContentGroupItem *>(content)->processTrimItems(list);
            break;
        }
        default:
            break;
        }
    }
}

/*
 * LOTPathDataItem uses 3 path objects for path object reuse.
 * mLocalPath -  keeps track of the local path of the item before
 * applying path operation and transformation.
 * mTemp - keeps a referece to the mLocalPath and can be updated by the
 *          path operation objects(trim, merge path),
 *  mFinalPath - it takes a deep copy of the intermediate path(mTemp) each time
 *  when the path is dirty(so if path changes every frame we don't realloc just
 * copy to the final path). NOTE: As path objects are COW objects we have to be
 * carefull about the refcount so that we don't generate deep copy while
 * modifying the path objects.
 */
void LOTPathDataItem::update(int              frameNo, const VMatrix &, float,
                             const DirtyFlag &flag)
{
    mPathChanged = false;

    // 1. update the local path if needed
    if (hasChanged(frameNo)) {
        // loose the reference to mLocalPath if any
        // from the last frame update.
        mTemp = VPath();

        updatePath(mLocalPath, frameNo);
        mPathChanged = true;
        mNeedUpdate = true;
    }
    // 2. keep a reference path in temp in case there is some
    // path operation like trim which will update the path.
    // we don't want to update the local path.
    mTemp = mLocalPath;

    // 3. compute the final path with parentMatrix
    if ((flag & DirtyFlagBit::Matrix) || mPathChanged) {
        mPathChanged = true;
    }
}

const VPath &LOTPathDataItem::finalPath()
{
    if (mPathChanged || mNeedUpdate) {
        mFinalPath.clone(mTemp);
        mFinalPath.transform(
            static_cast<LOTContentGroupItem *>(parent())->matrix());
        mNeedUpdate = false;
    }
    return mFinalPath;
}
LOTRectItem::LOTRectItem(LOTRectData *data)
    : LOTPathDataItem(data->isStatic()), mData(data)
{
}

void LOTRectItem::updatePath(VPath &path, int frameNo)
{
    VPointF pos = mData->mPos.value(frameNo);
    VPointF size = mData->mSize.value(frameNo);
    float   roundness = mData->mRound.value(frameNo);
    VRectF  r(pos.x() - size.x() / 2, pos.y() - size.y() / 2, size.x(),
             size.y());

    path.reset();
    path.addRoundRect(r, roundness, mData->direction());
}

LOTEllipseItem::LOTEllipseItem(LOTEllipseData *data)
    : LOTPathDataItem(data->isStatic()), mData(data)
{
}

void LOTEllipseItem::updatePath(VPath &path, int frameNo)
{
    VPointF pos = mData->mPos.value(frameNo);
    VPointF size = mData->mSize.value(frameNo);
    VRectF  r(pos.x() - size.x() / 2, pos.y() - size.y() / 2, size.x(),
             size.y());

    path.reset();
    path.addOval(r, mData->direction());
}

LOTShapeItem::LOTShapeItem(LOTShapeData *data)
    : LOTPathDataItem(data->isStatic()), mData(data)
{
}

void LOTShapeItem::updatePath(VPath &path, int frameNo)
{
    mData->mShape.updatePath(frameNo, path);
}

LOTPolystarItem::LOTPolystarItem(LOTPolystarData *data)
    : LOTPathDataItem(data->isStatic()), mData(data)
{
}

void LOTPolystarItem::updatePath(VPath &path, int frameNo)
{
    VPointF pos = mData->mPos.value(frameNo);
    float   points = mData->mPointCount.value(frameNo);
    float   innerRadius = mData->mInnerRadius.value(frameNo);
    float   outerRadius = mData->mOuterRadius.value(frameNo);
    float   innerRoundness = mData->mInnerRoundness.value(frameNo);
    float   outerRoundness = mData->mOuterRoundness.value(frameNo);
    float   rotation = mData->mRotation.value(frameNo);

    path.reset();
    VMatrix m;

    if (mData->mPolyType == LOTPolystarData::PolyType::Star) {
        path.addPolystar(points, innerRadius, outerRadius, innerRoundness,
                         outerRoundness, 0.0, 0.0, 0.0, mData->direction());
    } else {
        path.addPolygon(points, outerRadius, outerRoundness, 0.0, 0.0, 0.0,
                        mData->direction());
    }

    m.translate(pos.x(), pos.y()).rotate(rotation);
    m.rotate(rotation);
    path.transform(m);
}

/*
 * PaintData Node handling
 *
 */
LOTPaintDataItem::LOTPaintDataItem(bool staticContent)
    : mStaticContent(staticContent)
{
}

void LOTPaintDataItem::update(int   frameNo, const VMatrix & parentMatrix,
                              float parentAlpha, const DirtyFlag &/*flag*/)
{
    mRenderNodeUpdate = true;
    mContentToRender = updateContent(frameNo, parentMatrix, parentAlpha);
}

void LOTPaintDataItem::updateRenderNode()
{
    bool dirty = false;
    for (auto &i : mPathItems) {
        if (i->dirty()) {
            dirty = true;
            break;
        }
    }

    if (dirty || mPath.empty()) {
        mPath.reset();

        for (auto &i : mPathItems) {
            mPath.addPath(i->finalPath());
        }
        mDrawable.setPath(mPath);
    } else {
        if (mDrawable.mFlag & VDrawable::DirtyState::Path)
            mDrawable.mPath = mPath;
    }
}

void LOTPaintDataItem::renderList(std::vector<VDrawable *> &list)
{
    if (!mContentToRender) return;

    if (mRenderNodeUpdate) {
        updateRenderNode();
        mRenderNodeUpdate = false;
    }
    list.push_back(&mDrawable);
}

void LOTPaintDataItem::addPathItems(std::vector<LOTPathDataItem *> &list,
                                    size_t                          startOffset)
{
    std::copy(list.begin() + startOffset, list.end(),
              back_inserter(mPathItems));
}

LOTFillItem::LOTFillItem(LOTFillData *data)
    : LOTPaintDataItem(data->isStatic()), mModel(data)
{
}

bool LOTFillItem::updateContent(int frameNo, const VMatrix &, float alpha)
{
    auto combinedAlpha = alpha * mModel.opacity(frameNo);
    auto color = mModel.color(frameNo).toColor(combinedAlpha);

    if (color.isTransparent()) return false;

    VBrush brush(color);
    mDrawable.setBrush(brush);
    mDrawable.setFillRule(mModel.fillRule());

    return true;
}

LOTGFillItem::LOTGFillItem(LOTGFillData *data)
    : LOTPaintDataItem(data->isStatic()), mData(data)
{
}

bool LOTGFillItem::updateContent(int frameNo, const VMatrix &matrix, float alpha)
{
    float combinedAlpha = alpha * mData->opacity(frameNo);
    if (vIsZero(combinedAlpha)) return false;

    mData->update(mGradient, frameNo);
    mGradient->setAlpha(combinedAlpha);
    mGradient->mMatrix = matrix;
    mDrawable.setBrush(VBrush(mGradient.get()));
    mDrawable.setFillRule(mData->fillRule());

    return true;
}

LOTStrokeItem::LOTStrokeItem(LOTStrokeData *data)
    : LOTPaintDataItem(data->isStatic()), mModel(data){}

static thread_local std::vector<float> Dash_Vector;

bool LOTStrokeItem::updateContent(int frameNo, const VMatrix &matrix, float alpha)
{
    auto combinedAlpha = alpha * mModel.opacity(frameNo);
    auto color = mModel.color(frameNo).toColor(combinedAlpha);

    if (color.isTransparent()) return false;

    VBrush brush(color);
    mDrawable.setBrush(brush);
    float scale = matrix.scale();
    mDrawable.setStrokeInfo(mModel.capStyle(), mModel.joinStyle(),
                            mModel.miterLimit(), mModel.strokeWidth(frameNo) * scale);

    if (mModel.hasDashInfo()) {
        Dash_Vector.clear();
        mModel.getDashInfo(frameNo, Dash_Vector);
        if (!Dash_Vector.empty()) {
            for (auto &elm : Dash_Vector) elm *= scale;
            mDrawable.setDashInfo(Dash_Vector);
        }
    }

    return true;
}

LOTGStrokeItem::LOTGStrokeItem(LOTGStrokeData *data)
    : LOTPaintDataItem(data->isStatic()), mData(data){}

bool LOTGStrokeItem::updateContent(int frameNo, const VMatrix &matrix, float alpha)
{
    float combinedAlpha = alpha * mData->opacity(frameNo);
    if (vIsZero(combinedAlpha)) return false;

    mData->update(mGradient, frameNo);
    mGradient->setAlpha(combinedAlpha);
    mGradient->mMatrix = matrix;
    auto scale = mGradient->mMatrix.scale();
    mDrawable.setBrush(VBrush(mGradient.get()));
    mDrawable.setStrokeInfo(mData->capStyle(),  mData->joinStyle(),
                            mData->miterLimit(), mData->width(frameNo) * scale);

    if (mData->hasDashInfo()) {
        Dash_Vector.clear();
        mData->getDashInfo(frameNo, Dash_Vector);
        if (!Dash_Vector.empty()) {
            for (auto &elm : Dash_Vector) elm *= scale;
            mDrawable.setDashInfo(Dash_Vector);
        }
    }

    return true;
}

LOTTrimItem::LOTTrimItem(LOTTrimData *data)
    : mData(data)
{
}

void LOTTrimItem::update(int frameNo, const VMatrix & /*parentMatrix*/,
                         float /*parentAlpha*/, const DirtyFlag & /*flag*/)
{
    mDirty = false;

    if (mCache.mFrameNo == frameNo) return;

    LOTTrimData::Segment segment = mData->segment(frameNo);

    if (!(vCompare(mCache.mSegment.start, segment.start) &&
          vCompare(mCache.mSegment.end, segment.end))) {
        mDirty = true;
        mCache.mSegment = segment;
    }
    mCache.mFrameNo = frameNo;
}

void LOTTrimItem::update()
{
    // when both path and trim are not dirty
    if (!(mDirty || pathDirty())) return;

    if (vCompare(mCache.mSegment.start, mCache.mSegment.end)) {
        for (auto &i : mPathItems) {
            i->updatePath(VPath());
        }
        return;
    }

    if (vCompare(std::fabs(mCache.mSegment.start - mCache.mSegment.end), 1)) {
        for (auto &i : mPathItems) {
            i->updatePath(i->localPath());
        }
        return;
    }

    if (mData->type() == LOTTrimData::TrimType::Simultaneously) {
        for (auto &i : mPathItems) {
            mPathMesure.setRange(mCache.mSegment.start, mCache.mSegment.end);
            i->updatePath(mPathMesure.trim(i->localPath()));
        }
    } else {  // LOTTrimData::TrimType::Individually
        float totalLength = 0.0;
        for (auto &i : mPathItems) {
            totalLength += i->localPath().length();
        }
        float start = totalLength * mCache.mSegment.start;
        float end = totalLength * mCache.mSegment.end;

        if (start < end) {
            float curLen = 0.0;
            for (auto &i : mPathItems) {
                if (curLen > end) {
                    // update with empty path.
                    i->updatePath(VPath());
                    continue;
                }
                float len = i->localPath().length();

                if (curLen < start && curLen + len < start) {
                    curLen += len;
                    // update with empty path.
                    i->updatePath(VPath());
                    continue;
                } else if (start <= curLen && end >= curLen + len) {
                    // inside segment
                    curLen += len;
                    continue;
                } else {
                    float local_start = start > curLen ? start - curLen : 0;
                    local_start /= len;
                    float local_end = curLen + len < end ? len : end - curLen;
                    local_end /= len;
                    mPathMesure.setRange(local_start, local_end);
                    i->updatePath(mPathMesure.trim(i->localPath()));
                    curLen += len;
                }
            }
        }
    }
}

void LOTTrimItem::addPathItems(std::vector<LOTPathDataItem *> &list,
                               size_t                          startOffset)
{
    std::copy(list.begin() + startOffset, list.end(),
              back_inserter(mPathItems));
}

LOTRepeaterItem::LOTRepeaterItem(LOTRepeaterData *data) : mRepeaterData(data)
{
    assert(mRepeaterData->content());

    mCopies = mRepeaterData->maxCopies();

    for (int i = 0; i < mCopies; i++) {
        auto content =
            std::make_unique<LOTContentGroupItem>(mRepeaterData->content());
        //content->setParent(this);
        mContents.push_back(std::move(content));
    }
}

void LOTRepeaterItem::update(int frameNo, const VMatrix &parentMatrix,
                             float parentAlpha, const DirtyFlag &flag)
{
    DirtyFlag newFlag = flag;

    float copies = mRepeaterData->copies(frameNo);
    int   visibleCopies = int(copies);

    if (visibleCopies == 0) {
        mHidden = true;
        return;
    }

    mHidden = false;

    if (!mRepeaterData->isStatic()) newFlag |= DirtyFlagBit::Matrix;

    float offset = mRepeaterData->offset(frameNo);
    float startOpacity = mRepeaterData->mTransform.startOpacity(frameNo);
    float endOpacity = mRepeaterData->mTransform.endOpacity(frameNo);

    newFlag |= DirtyFlagBit::Alpha;

    for (int i = 0; i < mCopies; ++i) {
        float newAlpha =
            parentAlpha * lerp(startOpacity, endOpacity, i / copies);

        // hide rest of the copies , @TODO find a better solution.
        if (i >= visibleCopies) newAlpha = 0;

        VMatrix result = mRepeaterData->mTransform.matrix(frameNo, i + offset) *
                         parentMatrix;
        mContents[i]->update(frameNo, result, newAlpha, newFlag);
    }
}

void LOTRepeaterItem::renderList(std::vector<VDrawable *> &list)
{
    if (mHidden) return;
    return LOTContentGroupItem::renderList(list);
}
