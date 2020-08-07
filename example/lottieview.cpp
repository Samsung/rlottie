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

#include"lottieview.h"

using namespace rlottie;

static void
_image_update_cb(void *data, Evas_Object *obj EINA_UNUSED)
{
   RenderStrategy *info = (RenderStrategy *)data;
   info->dataCb();
}

void RenderStrategy::addCallback(){
    if (_useImage)
      evas_object_image_pixels_get_callback_set(_renderObject, _image_update_cb, this);
}

static Eina_Bool
animator(void *data , double pos)
{
    LottieView *view = static_cast<LottieView *>(data);

    view->seek(pos);
    if (pos == 1.0) {
      view->mAnimator = NULL;
      view->finished();
      return EINA_FALSE;
    }
    return EINA_TRUE;
}

LottieView::LottieView(Evas *evas, Strategy s) {
    mPalying = false;
    mReverse = false;
    mRepeatCount = 0;
    mRepeatMode = LottieView::RepeatMode::Restart;
    mLoop = false;
    mSpeed = 1;

    switch (s) {
    case Strategy::renderCpp: {
        mRenderDelegate = std::make_unique<RlottieRenderStrategy_CPP>(evas);
        break;
    }
    case Strategy::renderCppAsync: {
        mRenderDelegate = std::make_unique<RlottieRenderStrategy_CPP_ASYNC>(evas);
        break;
    }
    case Strategy::renderC: {
        mRenderDelegate = std::make_unique<RlottieRenderStrategy_C>(evas);
        break;
    }
    case Strategy::renderCAsync: {
        mRenderDelegate = std::make_unique<RlottieRenderStrategy_C_ASYNC>(evas);
        break;
    }
    case Strategy::eflVg: {
        mRenderDelegate = std::make_unique<EflVgRenderStrategy>(evas);
        break;
    }
    default:
        mRenderDelegate = std::make_unique<RlottieRenderStrategy_CPP>(evas);
        break;
    }
}

LottieView::~LottieView()
{
    if (mAnimator) ecore_animator_del(mAnimator);
}

Evas_Object *LottieView::getImage() {
    return mRenderDelegate->renderObject();
}

void LottieView::show()
{
    mRenderDelegate->show();
    seek(0);
}

void LottieView::hide()
{
    mRenderDelegate->hide();
}

void LottieView::seek(float pos)
{
    if (!mRenderDelegate) return;


    mPos = mapProgress(pos);

    // check if the pos maps to the current frame
    if (mCurFrame == mRenderDelegate->frameAtPos(mPos)) return;

    mCurFrame = mRenderDelegate->frameAtPos(mPos);

    mRenderDelegate->render(mCurFrame);
}

float LottieView::getPos()
{
   return mPos;
}

void LottieView::setFilePath(const char *filePath)
{
    mRenderDelegate->loadFromFile(filePath);
}

void LottieView::loadFromData(const std::string &jsonData, const std::string &key, const std::string &resourcePath)
{
    mRenderDelegate->loadFromData(jsonData, key, resourcePath);
}

void LottieView::setSize(int w, int h)
{
    mRenderDelegate->resize(w, h);
}

void LottieView::setPos(int x, int y)
{
    mRenderDelegate->setPos(x, y);
}

void LottieView::finished()
{
    restart();
}

void LottieView::loop(bool loop)
{
    mLoop = loop;
}

void LottieView::setRepeatCount(int count)
{
    mRepeatCount = count;
}

void LottieView::setRepeatMode(LottieView::RepeatMode mode)
{
    mRepeatMode = mode;
}

void LottieView::play()
{
    if (mAnimator) ecore_animator_del(mAnimator);
    mAnimator = ecore_animator_timeline_add(duration()/mSpeed, animator, this);
    mReverse = false;
    mCurCount = mRepeatCount;
    mPalying = true;
}

void LottieView::play(const std::string &marker)
{
    size_t totalframe = getTotalFrame();
    auto frame = mRenderDelegate->findFrameAtMarker(marker);

    setMinProgress((float)std::get<0>(frame) / (float)totalframe);
    if (std::get<1>(frame) != 0)
      setMaxProgress((float)std::get<1>(frame) / (float)totalframe);

    this->play();
}

void LottieView::play(const std::string &startmarker, const std::string endmarker)
{
    size_t totalframe = getTotalFrame();
    auto stframe = mRenderDelegate->findFrameAtMarker(startmarker);
    auto edframe = mRenderDelegate->findFrameAtMarker(endmarker);

    setMinProgress((float)std::get<0>(stframe) / (float)totalframe);
    if (std::get<0>(edframe) != 0)
      setMaxProgress((float)std::get<0>(edframe) / (float)totalframe);

    this->play();
}

void LottieView::pause()
{

}

void LottieView::stop()
{
    mPalying = false;
    if (mAnimator) {
        ecore_animator_del(mAnimator);
        mAnimator = NULL;
    }
}

void LottieView::restart()
{
    mCurCount--;
    if (mLoop || mRepeatCount) {
        if (mRepeatMode == LottieView::RepeatMode::Reverse)
            mReverse = !mReverse;
        else
            mReverse = false;

        if (mAnimator) ecore_animator_del(mAnimator);
        mAnimator = ecore_animator_timeline_add(duration()/mSpeed, animator, this);
    }
}
