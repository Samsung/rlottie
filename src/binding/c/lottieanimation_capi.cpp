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
#include "rlottie_capi.h"
#include "vdebug.h"

using namespace rlottie;

extern "C" {
#include <string.h>
#include <stdarg.h>

struct Lottie_Animation_S
{
    std::unique_ptr<Animation>      mAnimation;
    std::future<Surface>            mRenderTask;
    uint32_t                       *mBufferRef;
    LOTMarkerList                  *mMarkerList;
};

LOT_EXPORT Lottie_Animation_S *lottie_animation_from_file(const char *path)
{
    if (auto animation = Animation::loadFromFile(path) ) {
        Lottie_Animation_S *handle = new Lottie_Animation_S();
        handle->mAnimation = std::move(animation);
        return handle;
    } else {
        return nullptr;
    }
}

LOT_EXPORT Lottie_Animation_S *lottie_animation_from_data(const char *data, const char *key, const char *resourcePath)
{
    if (auto animation = Animation::loadFromData(data, key, resourcePath) ) {
        Lottie_Animation_S *handle = new Lottie_Animation_S();
        handle->mAnimation = std::move(animation);
        return handle;
    } else {
        return nullptr;
    }
}

LOT_EXPORT void lottie_animation_destroy(Lottie_Animation_S *animation)
{
    if (animation) {
        if (animation->mMarkerList) {
            for(size_t i = 0; i < animation->mMarkerList->size; i++) {
                if (animation->mMarkerList->ptr[i].name) free(animation->mMarkerList->ptr[i].name);
            }
            delete[] animation->mMarkerList->ptr;
            delete animation->mMarkerList;
        }

        if (animation->mRenderTask.valid()) {
            animation->mRenderTask.get();
        }
        animation->mAnimation = nullptr;
        delete animation;
    }
}

LOT_EXPORT void lottie_animation_get_size(const Lottie_Animation_S *animation, size_t *width, size_t *height)
{
   if (!animation) return;

   animation->mAnimation->size(*width, *height);
}

LOT_EXPORT double lottie_animation_get_duration(const Lottie_Animation_S *animation)
{
   if (!animation) return 0;

   return animation->mAnimation->duration();
}

LOT_EXPORT size_t lottie_animation_get_totalframe(const Lottie_Animation_S *animation)
{
   if (!animation) return 0;

   return animation->mAnimation->totalFrame();
}


LOT_EXPORT double lottie_animation_get_framerate(const Lottie_Animation_S *animation)
{
   if (!animation) return 0;

   return animation->mAnimation->frameRate();
}

LOT_EXPORT const LOTLayerNode * lottie_animation_render_tree(Lottie_Animation_S *animation, size_t frame_num, size_t width, size_t height)
{
    if (!animation) return nullptr;

    return animation->mAnimation->renderTree(frame_num, width, height);
}

LOT_EXPORT size_t
lottie_animation_get_frame_at_pos(const Lottie_Animation_S *animation, float pos)
{
    if (!animation) return 0;

    return animation->mAnimation->frameAtPos(pos);
}

LOT_EXPORT void
lottie_animation_render(Lottie_Animation_S *animation,
                        size_t frame_number,
                        uint32_t *buffer,
                        size_t width,
                        size_t height,
                        size_t bytes_per_line)
{
    if (!animation) return;

    rlottie::Surface surface(buffer, width, height, bytes_per_line);
    animation->mAnimation->renderSync(frame_number, surface);
}

LOT_EXPORT void
lottie_animation_render_async(Lottie_Animation_S *animation,
                              size_t frame_number,
                              uint32_t *buffer,
                              size_t width,
                              size_t height,
                              size_t bytes_per_line)
{
    if (!animation) return;

    rlottie::Surface surface(buffer, width, height, bytes_per_line);
    animation->mRenderTask = animation->mAnimation->render(frame_number, surface);
    animation->mBufferRef = buffer;
}

LOT_EXPORT uint32_t *
lottie_animation_render_flush(Lottie_Animation_S *animation)
{
    if (!animation) return nullptr;

    if (animation->mRenderTask.valid()) {
        animation->mRenderTask.get();
    }

    return animation->mBufferRef;
}

LOT_EXPORT void
lottie_animation_property_override(Lottie_Animation_S *animation,
                                   const Lottie_Animation_Property type,
                                   const char *keypath,
                                   ...)
{
    va_list prop;
    va_start(prop, keypath);
    const int arg_count = [type](){
                             switch (type) {
                              case LOTTIE_ANIMATION_PROPERTY_FILLCOLOR:
                              case LOTTIE_ANIMATION_PROPERTY_STROKECOLOR:
                                return 3;
                              case LOTTIE_ANIMATION_PROPERTY_FILLOPACITY:
                              case LOTTIE_ANIMATION_PROPERTY_STROKEOPACITY:
                              case LOTTIE_ANIMATION_PROPERTY_STROKEWIDTH:
                              case LOTTIE_ANIMATION_PROPERTY_TR_ROTATION:
                                return 1;
                              case LOTTIE_ANIMATION_PROPERTY_TR_POSITION:
                              case LOTTIE_ANIMATION_PROPERTY_TR_SCALE:
                                return 2;
                              default:
                                return 0;
                             }
                          }();
    double v[3] = {0};
    for (int i = 0; i < arg_count ; i++) {
      v[i] = va_arg(prop, double);
    }
    va_end(prop);

    switch(type) {
    case LOTTIE_ANIMATION_PROPERTY_FILLCOLOR: {
        double r = v[0];
        double g = v[1];
        double b = v[2];
        if (r > 1 || r < 0 || g > 1 || g < 0 || b > 1 || b < 0) break;
        animation->mAnimation->setValue<rlottie::Property::FillColor>(keypath, rlottie::Color(r, g, b));
        break;
    }
    case LOTTIE_ANIMATION_PROPERTY_FILLOPACITY: {
        double opacity = v[0];
        if (opacity > 100 || opacity < 0) break;
        animation->mAnimation->setValue<rlottie::Property::FillOpacity>(keypath, (float)opacity);
        break;
    }
    case LOTTIE_ANIMATION_PROPERTY_STROKECOLOR: {
        double r = v[0];
        double g = v[1];
        double b = v[2];
        if (r > 1 || r < 0 || g > 1 || g < 0 || b > 1 || b < 0) break;
        animation->mAnimation->setValue<rlottie::Property::StrokeColor>(keypath, rlottie::Color(r, g, b));
        break;
    }
    case LOTTIE_ANIMATION_PROPERTY_STROKEOPACITY: {
        double opacity = v[0];
        if (opacity > 100 || opacity < 0) break;
        animation->mAnimation->setValue<rlottie::Property::StrokeOpacity>(keypath, (float)opacity);
        break;
    }
    case LOTTIE_ANIMATION_PROPERTY_STROKEWIDTH: {
        double width = v[0];
        if (width < 0) break;
        animation->mAnimation->setValue<rlottie::Property::StrokeWidth>(keypath, (float)width);
        break;
    }
    case LOTTIE_ANIMATION_PROPERTY_TR_POSITION: {
        double x = v[0];
        double y = v[1];
        animation->mAnimation->setValue<rlottie::Property::TrPosition>(keypath, rlottie::Point((float)x, (float)y));
        break;
    }
    case LOTTIE_ANIMATION_PROPERTY_TR_SCALE: {
        double w = v[0];
        double h = v[1];
        animation->mAnimation->setValue<rlottie::Property::TrScale>(keypath, rlottie::Size((float)w, (float)h));
        break;
    }
    case LOTTIE_ANIMATION_PROPERTY_TR_ROTATION: {
        double r = v[0];
        animation->mAnimation->setValue<rlottie::Property::TrRotation>(keypath, (float)r);
        break;
    }
    case LOTTIE_ANIMATION_PROPERTY_TR_ANCHOR:
    case LOTTIE_ANIMATION_PROPERTY_TR_OPACITY:
        //@TODO handle propery update.
        break;
    }
}

LOT_EXPORT const LOTMarkerList*
lottie_animation_get_markerlist(Lottie_Animation_S *animation)
{
   if (!animation) return nullptr;

   auto markers = animation->mAnimation->markers();
   if (markers.size() == 0) return nullptr;
   if (animation->mMarkerList) return (const LOTMarkerList*)animation->mMarkerList;

   animation->mMarkerList = new LOTMarkerList();
   animation->mMarkerList->size = markers.size();
   animation->mMarkerList->ptr = new LOTMarker[markers.size()]();

   for(size_t i = 0; i < markers.size(); i++) {
       animation->mMarkerList->ptr[i].name = strdup(std::get<0>(markers[i]).c_str());
       animation->mMarkerList->ptr[i].startframe= std::get<1>(markers[i]);
       animation->mMarkerList->ptr[i].endframe= std::get<2>(markers[i]);
   }
   return (const LOTMarkerList*)animation->mMarkerList;
}

}
