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

#ifndef LOTTIEVIEW_H
#define LOTTIEVIEW_H

#ifndef EFL_BETA_API_SUPPORT
#define EFL_BETA_API_SUPPORT
#endif

#ifndef EFL_EO_API_SUPPORT
#define EFL_EO_API_SUPPORT
#endif

#include <Eo.h>
#include <Efl.h>
#include <Evas.h>
#include <Ecore.h>
#include <Ecore_Evas.h>
#include "rlottie.h"
#include "rlottie_capi.h"
#include<future>
#include <cmath>
#include <algorithm>
#include <vector>

class RenderStrategy {
public:
    virtual ~RenderStrategy() {
        evas_object_del(renderObject());
    }
    RenderStrategy(Evas_Object *obj, bool useImage = true):_renderObject(obj), _useImage(useImage){
        addCallback();
    }
    virtual rlottie::Animation *player() {return nullptr;}
    virtual void loadFromFile(const char *filePath) = 0;
    virtual void loadFromData(const std::string &jsonData, const std::string &key, const std::string &resourcePath) = 0;
    virtual size_t totalFrame() = 0;
    virtual double frameRate() = 0;
    virtual size_t frameAtPos(double pos) = 0;
    virtual double duration() = 0;
    void render(int frame) {
        _renderCount++;
        _redraw = renderRequest(frame);
        if (_redraw && _useImage)
            evas_object_image_pixels_dirty_set(renderObject(), EINA_TRUE);
    }
    void dataCb() {
        if (_redraw && _useImage) {
            evas_object_image_data_set(renderObject(), buffer());
        }
        _redraw = false;
    }
    virtual void resize(int width, int height) = 0;
    virtual void setPos(int x, int y) {evas_object_move(renderObject(), x, y);}
    typedef std::tuple<size_t, size_t> MarkerFrame;
    virtual MarkerFrame findFrameAtMarker(const std::string &markerName) = 0;
    void show() {evas_object_show(_renderObject);}
    void hide() {evas_object_hide(_renderObject);}
    void addCallback();
    Evas_Object* renderObject() const {return _renderObject;}
    size_t renderCount() const {return _renderCount;}
protected:
    virtual bool renderRequest(int) = 0;
    virtual uint32_t* buffer() = 0;
private:
    size_t       _renderCount{0};
    bool         _redraw{false};
    Evas_Object *_renderObject;
    bool         _useImage{true};
};

class CppApiBase : public RenderStrategy {
public:
    CppApiBase(Evas_Object *renderObject): RenderStrategy(renderObject) {}
    rlottie::Animation *player() {return mPlayer.get();}
    void loadFromFile(const char *filePath)
    {
        mPlayer = rlottie::Animation::loadFromFile(filePath);

        if (!mPlayer) {
            printf("load failed file %s\n", filePath);
        }
    }

    void loadFromData(const std::string &jsonData, const std::string &key, const std::string &resourcePath)
    {
        mPlayer = rlottie::Animation::loadFromData(jsonData, key, resourcePath);

        if (!mPlayer) {
            printf("load failed from data\n");
        }
    }

    size_t totalFrame() {
        return mPlayer->totalFrame();

    }
    double duration() {
        return mPlayer->duration();
    }

    double frameRate() {
        return mPlayer->frameRate();
    }

    size_t frameAtPos(double pos) {
        return  mPlayer->frameAtPos(pos);
    }

    MarkerFrame findFrameAtMarker(const std::string &markerName)
    {
        auto markerList = mPlayer->markers();
        auto marker = std::find_if(markerList.begin(), markerList.end()
                                   , [&markerName](const auto& e) {
                                        return std::get<0>(e) == markerName;
                                     });
        if (marker == markerList.end())
           return std::make_tuple(0, 0);
        return std::make_tuple(std::get<1>(*marker), std::get<2>(*marker));
    }

protected:
   std::unique_ptr<rlottie::Animation>       mPlayer;
};

class RlottieRenderStrategyCBase : public RenderStrategy {
public:
    RlottieRenderStrategyCBase(Evas *evas):RenderStrategy(evas_object_image_filled_add(evas)) {
        evas_object_image_colorspace_set(renderObject(), EVAS_COLORSPACE_ARGB8888);
        evas_object_image_alpha_set(renderObject(), EINA_TRUE);
    }
    void resize(int width, int height) {
        evas_object_resize(renderObject(), width, height);
        evas_object_image_size_set(renderObject(), width, height);
    }
};

class RlottieRenderStrategy : public CppApiBase {
public:
    RlottieRenderStrategy(Evas *evas):CppApiBase(evas_object_image_filled_add(evas)) {
        evas_object_image_colorspace_set(renderObject(), EVAS_COLORSPACE_ARGB8888);
        evas_object_image_alpha_set(renderObject(), EINA_TRUE);
    }
    void resize(int width, int height) {
        evas_object_resize(renderObject(), width, height);
        evas_object_image_size_set(renderObject(), width, height);
    }
};

class RlottieRenderStrategy_CPP : public RlottieRenderStrategy {
public:
    RlottieRenderStrategy_CPP(Evas *evas):RlottieRenderStrategy(evas) {}

    bool renderRequest(int frame) {
        int width , height;
        Evas_Object *image = renderObject();
        evas_object_image_size_get(image, &width, &height);
        mBuffer = (uint32_t *)evas_object_image_data_get(image, EINA_TRUE);
        size_t bytesperline =  evas_object_image_stride_get(image);
        rlottie::Surface surface(mBuffer, width, height, bytesperline);
        mPlayer->renderSync(frame, surface);
        return true;
    }
    uint32_t* buffer() {
        return mBuffer;
    }

private:
    uint32_t *              mBuffer;
};

class RlottieRenderStrategy_CPP_ASYNC : public RlottieRenderStrategy {
public:
    RlottieRenderStrategy_CPP_ASYNC(Evas *evas):RlottieRenderStrategy(evas) {}
    ~RlottieRenderStrategy_CPP_ASYNC() {
        if (mRenderTask.valid())
            mRenderTask.get();
    }
    bool renderRequest(int frame) {
        //addCallback();
        if (mRenderTask.valid()) return true;
        int width , height;
        Evas_Object *image = renderObject();
        evas_object_image_size_get(image, &width, &height);
        auto buffer = (uint32_t *)evas_object_image_data_get(image, EINA_TRUE);
        size_t bytesperline =  evas_object_image_stride_get(image);
        rlottie::Surface surface(buffer, width, height, bytesperline);
        mRenderTask = mPlayer->render(frame, surface);
        return true;
    }

    uint32_t* buffer() {
        auto surface = mRenderTask.get();
        return surface.buffer();
    }
private:
   std::future<rlottie::Surface>        mRenderTask;
};


class RlottieRenderStrategy_C : public RlottieRenderStrategyCBase {
public:
    RlottieRenderStrategy_C(Evas *evas):RlottieRenderStrategyCBase(evas) {}
    ~RlottieRenderStrategy_C() {
        if (mPlayer) lottie_animation_destroy(mPlayer);
    }
    void loadFromFile(const char *filePath)
    {
        mPlayer = lottie_animation_from_file(filePath);

        if (!mPlayer) {
            printf("load failed file %s\n", filePath);
        }
    }

    void loadFromData(const std::string &jsonData, const std::string &key, const std::string &resourcePath)
    {
        mPlayer = lottie_animation_from_data(jsonData.c_str(), key.c_str(), resourcePath.c_str());
        if (!mPlayer) {
            printf("load failed from data\n");
        }
    }

    size_t totalFrame() {
        return lottie_animation_get_totalframe(mPlayer);

    }

    double frameRate() {
        return lottie_animation_get_framerate(mPlayer);
    }

    size_t frameAtPos(double pos) {
        return  lottie_animation_get_frame_at_pos(mPlayer, pos);
    }

    double duration() {
        return lottie_animation_get_duration(mPlayer);
    }
    MarkerFrame findFrameAtMarker(const std::string &markerName)
    {
        printf("Can't not [%s] marker, CAPI isn't implements yet\n", markerName.c_str());
        return std::make_tuple(0, 0);
    }

    bool renderRequest(int frame) {
        int width , height;
        Evas_Object *image = renderObject();
        evas_object_image_size_get(image, &width, &height);
        mBuffer = (uint32_t *)evas_object_image_data_get(image, EINA_TRUE);
        size_t bytesperline =  evas_object_image_stride_get(image);
        lottie_animation_render(mPlayer, frame, mBuffer, width, height, bytesperline);
        return true;
    }

    uint32_t* buffer() {
        return mBuffer;
    }

private:
    uint32_t *              mBuffer;
protected:
   Lottie_Animation       *mPlayer;
};

class RlottieRenderStrategy_C_ASYNC : public RlottieRenderStrategy_C {
public:
    RlottieRenderStrategy_C_ASYNC(Evas *evas):RlottieRenderStrategy_C(evas) {}
    ~RlottieRenderStrategy_C_ASYNC() {
        if (mDirty) lottie_animation_render_flush(mPlayer);
    }
    bool renderRequest(int frame) {
        if (mDirty) return true;
        mDirty = true;
        Evas_Object *image = renderObject();
        evas_object_image_size_get(image, &mWidth, &mHeight);
        mBuffer = (uint32_t *)evas_object_image_data_get(image, EINA_TRUE);
        size_t bytesperline =  evas_object_image_stride_get(image);
        lottie_animation_render_async(mPlayer, frame, mBuffer, mWidth, mHeight, bytesperline);
        return true;
    }

    uint32_t* buffer() {
       lottie_animation_render_flush(mPlayer);
       mDirty =false;
       return mBuffer;
    }

private:
   uint32_t *              mBuffer;
   int                     mWidth;
   int                     mHeight;
   bool                    mDirty{false};
};

enum class  Strategy {
  renderCpp = 0,
  renderCppAsync,
  renderC,
  renderCAsync,
  eflVg
};

class LottieView
{
public:
    enum class RepeatMode {
        Restart,
        Reverse
    };
    LottieView(Evas *evas, Strategy s = Strategy::renderCppAsync);
    ~LottieView();
    rlottie::Animation *player(){return mRenderDelegate->player();}
    Evas_Object *getImage();
    void setSize(int w, int h);
    void setPos(int x, int y);
    void setFilePath(const char *filePath);
    void loadFromData(const std::string &jsonData, const std::string &key, const std::string &resourcePath="");
    void show();
    void hide();
    void loop(bool loop);
    void setSpeed(float speed) { mSpeed = speed;}
    void setRepeatCount(int count);
    void setRepeatMode(LottieView::RepeatMode mode);
    float getFrameRate() const { return mRenderDelegate->frameRate(); }
    long getTotalFrame() const { return mRenderDelegate->totalFrame(); }
public:
    void seek(float pos);
    float getPos();
    void finished();
    void play();
    void play(const std::string &marker);
    void play(const std::string &startmarker, const std::string endmarker);
    void pause();
    void stop();
    void initializeBufferObject(Evas *evas);
    void setMinProgress(float progress)
    {
        //clamp it to [0,1]
        mMinProgress = progress;
    }
    void setMaxProgress(float progress)
    {
        //clamp it to [0,1]
        mMaxprogress = progress;
    }

    size_t renderCount() const {
        return mRenderDelegate ? mRenderDelegate->renderCount() : 0;
    }
private:
    float mapProgress(float progress) {
        //clamp it to the segment
        progress = (mMinProgress + (mMaxprogress - mMinProgress) * progress);

        // currently playing and in reverse mode
        if (mPalying && mReverse)
            progress = mMaxprogress > mMinProgress ?
                        mMaxprogress - progress : mMinProgress - progress;


        return progress;
    }
    float duration() const {
        // usually we run the animation for mPlayer->duration()
        // but now run animation for segmented duration.
        return  mRenderDelegate->duration() * fabs(mMaxprogress - mMinProgress);
    }
    void createVgNode(LOTNode *node, Efl_VG *root);
    void update(const std::vector<LOTNode *> &);
    void updateTree(const LOTLayerNode *);
    void update(const LOTLayerNode *, Efl_VG *parent);
    void restart();
public:
    int                      mRepeatCount;
    LottieView::RepeatMode   mRepeatMode;
    size_t                   mCurFrame{UINT_MAX};
    Ecore_Animator          *mAnimator{nullptr};
    bool                     mLoop;
    int                      mCurCount;
    bool                     mReverse;
    bool                     mPalying;
    float                    mSpeed;
    float                    mPos;
    //keep a segment of the animation default is [0, 1]
    float                   mMinProgress{0};
    float                   mMaxprogress{1};
    std::unique_ptr<RenderStrategy>  mRenderDelegate;
};

#include<assert.h>
class EflVgRenderStrategy : public RenderStrategy {
public:
    EflVgRenderStrategy(Evas *evas):RenderStrategy(evas_object_vg_add(evas), false) {}

    void loadFromFile(const char *filePath) {
        evas_object_vg_file_set(renderObject(), filePath, NULL);
    }

    void loadFromData(const std::string&, const std::string&, const std::string&) {
        assert(false);
    }

    size_t totalFrame() {
        return evas_object_vg_animated_frame_count_get(renderObject()) - 1;
    }

    double frameRate() {
        return (double)totalFrame() / evas_object_vg_animated_frame_duration_get(renderObject(), 0, 0);
    }

    size_t frameAtPos(double pos) {
        return totalFrame() * pos;
    }

    double duration() {
        return evas_object_vg_animated_frame_duration_get(renderObject(), 0, 0);
    }

    void resize(int width, int height) {
        evas_object_resize(renderObject(), width, height);
    }

    uint32_t *buffer() {
        assert(false);
        return nullptr;
    }

    bool renderRequest(int frame) {
        evas_object_vg_animated_frame_set(renderObject(), frame);
        return false;
    }

    MarkerFrame findFrameAtMarker(const std::string&) {
        return std::make_tuple(0, 0);
    }
};

#endif //LOTTIEVIEW_H
