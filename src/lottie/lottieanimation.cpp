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

#include "rlottie.h"
#include "lottieitem.h"
#include "lottieloader.h"
#include "lottiemodel.h"

#include <fstream>

using namespace rlottie;

struct RenderTask {
    RenderTask() { receiver = sender.get_future(); }
    std::promise<Surface> sender;
    std::future<Surface>  receiver;
    AnimationImpl     *playerImpl{nullptr};
    size_t              frameNo{0};
    Surface            surface;
};
using SharedRenderTask = std::shared_ptr<RenderTask>;

class AnimationImpl {

public:
    void    init(const std::shared_ptr<LOTModel> &model);
    bool    update(size_t frameNo, const VSize &size);
    VSize   size() const {return mCompItem->size();}
    double  duration() const {return mModel->duration();}
    double  frameRate() const {return mModel->frameRate();}
    size_t  totalFrame() const {return mModel->totalFrame();}
    size_t  frameAtPos(double pos) const {return mModel->frameAtPos(pos);}
    Surface render(size_t frameNo, const Surface &surface);
    std::future<Surface> renderAsync(size_t frameNo, Surface &&surface);
    const LOTLayerNode *
                 renderTree(size_t frameNo, const VSize &size);
private:
    std::string                  mFilePath;
    std::shared_ptr<LOTModel>    mModel;
    std::unique_ptr<LOTCompItem> mCompItem;
    SharedRenderTask             mTask;
    std::atomic<bool>            mRenderInProgress;
};

const LOTLayerNode *AnimationImpl::renderTree(size_t frameNo, const VSize &size)
{
    if (update(frameNo, size)) {
       mCompItem->buildRenderTree();
    }
    return mCompItem->renderTree();
}

bool AnimationImpl::update(size_t frameNo, const VSize &size)
{
   frameNo += mModel->startFrame();

   if (frameNo > mModel->endFrame())
       frameNo = mModel->endFrame();

   if (frameNo < mModel->startFrame())
       frameNo = mModel->startFrame();

   mCompItem->resize(size);
   return mCompItem->update(frameNo);
}

Surface AnimationImpl::render(size_t frameNo, const Surface &surface)
{
    bool renderInProgress = mRenderInProgress.load();
    if (renderInProgress)
      {
        vCritical << "Already Rendering Scheduled for this Animation";
        return surface;
      }

    mRenderInProgress.store(true);
    update(frameNo, VSize(surface.width(), surface.height()));
    mCompItem->render(surface);
    mRenderInProgress.store(false);

    return surface;
}

void AnimationImpl::init(const std::shared_ptr<LOTModel> &model)
{
    mModel = model;
    mCompItem = std::make_unique<LOTCompItem>(mModel.get());
    mRenderInProgress = false;
}

/*
 * Implement a task stealing schduler to perform render task
 * As each player draws into its own buffer we can delegate this
 * task to a slave thread. The scheduler creates a threadpool depending
 * on the number of cores available in the system and does a simple fair
 * scheduling by assigning the task in a round-robin fashion. Each thread
 * in the threadpool has its own queue. once it finishes all the task on its
 * own queue it goes through rest of the queue and looks for task if it founds
 * one it steals the task from it and executes. if it couldn't find one then it
 * just waits for new task on its own queue.
 */

#include <vtaskqueue.h>
class RenderTaskScheduler {
    const unsigned           _count{std::thread::hardware_concurrency()};
    std::vector<std::thread> _threads;
    std::vector<TaskQueue<SharedRenderTask>> _q{_count};
    std::atomic<unsigned>              _index{0};

    void run(unsigned i)
    {
        while (true) {
            bool success = false;
            SharedRenderTask task;
            for (unsigned n = 0; n != _count * 32; ++n) {
                if (_q[(i + n) % _count].try_pop(task)) {
                    success = true;
                    break;
                }
            }
            if (!success && !_q[i].pop(task)) break;

            auto result = task->playerImpl->render(task->frameNo, task->surface);
            task->sender.set_value(result);
        }
    }

    RenderTaskScheduler()
    {
        for (unsigned n = 0; n != _count; ++n) {
            _threads.emplace_back([&, n] { run(n); });
        }
    }

public:
    static RenderTaskScheduler& instance()
    {
         static RenderTaskScheduler singleton;
         return singleton;
    }

    ~RenderTaskScheduler()
    {
        for (auto &e : _q) e.done();

        for (auto &e : _threads) e.join();
    }

    std::future<Surface> async(SharedRenderTask task)
    {
        auto receiver = std::move(task->receiver);
        auto i = _index++;

        for (unsigned n = 0; n != _count; ++n) {
            if (_q[(i + n) % _count].try_push(std::move(task))) return receiver;
        }

        _q[i % _count].push(std::move(task));

        return receiver;
    }
};


std::future<Surface> AnimationImpl::renderAsync(size_t frameNo, Surface &&surface)
{
    if (!mTask) {
        mTask = std::make_shared<RenderTask>();
    } else {
        mTask->sender = std::promise<Surface>();
        mTask->receiver = mTask->sender.get_future();
    }
    mTask->playerImpl = this;
    mTask->frameNo = frameNo;
    mTask->surface = std::move(surface);

    return RenderTaskScheduler::instance().async(mTask);
}

/**
 * \breif Brief abput the Api.
 * Description about the setFilePath Api
 * @param path  add the details
 */
std::unique_ptr<Animation>
Animation::loadFromData(std::string jsonData, const std::string &key, const std::string &resourcePath)
{
    if (jsonData.empty()) {
        vWarning << "jason data is empty";
        return nullptr;
    }

    LottieLoader loader;
    if (loader.loadFromData(std::move(jsonData), key,
                            (resourcePath.empty() ? " " : resourcePath))) {
        auto animation = std::unique_ptr<Animation>(new Animation);
        animation->d->init(loader.model());
        return animation;
    }
    return nullptr;
}

std::unique_ptr<Animation>
Animation::loadFromFile(const std::string &path)
{
    if (path.empty()) {
        vWarning << "File path is empty";
        return nullptr;
    }

    LottieLoader loader;
    if (loader.load(path)) {
        auto animation = std::unique_ptr<Animation>(new Animation);
        animation->d->init(loader.model());
        return animation;
    }
    return nullptr;
}

void Animation::size(size_t &width, size_t &height) const
{
    VSize sz = d->size();

    width = sz.width();
    height = sz.height();
}

double Animation::duration() const
{
    return d->duration();
}

double Animation::frameRate() const
{
    return d->frameRate();
}

size_t Animation::totalFrame() const
{
    return d->totalFrame();
}

size_t Animation::frameAtPos(double pos)
{
    return d->frameAtPos(pos);
}

const LOTLayerNode *
Animation::renderTree(size_t frameNo, size_t width, size_t height) const
{
    return d->renderTree(frameNo, VSize(width, height));
}

std::future<Surface> Animation::render(size_t frameNo, Surface surface)
{
    return d->renderAsync(frameNo, std::move(surface));
}

void Animation::renderSync(size_t frameNo, Surface surface)
{
    d->render(frameNo, surface);
}

Animation::Animation(): d(std::make_unique<AnimationImpl>()) {}

/*
 * this is only to supress build fail
 * because unique_ptr expects the destructor in the same translation unit.
 */
Animation::~Animation(){}

Surface::Surface(uint32_t *buffer,
                 size_t width, size_t height, size_t bytesPerLine)
                :mBuffer(buffer),
                 mWidth(width),
                 mHeight(height),
                 mBytesPerLine(bytesPerLine) {}


void initLogging()
{
#if defined(__ARM_NEON__)
    set_log_level(LogLevel::OFF);
#else
    initialize(GuaranteedLogger(), "/tmp/", "rlottie", 1);
    set_log_level(LogLevel::INFO);
#endif
}

V_CONSTRUCTOR_FUNCTION(initLogging)
