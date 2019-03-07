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

#ifndef _RLOTTIE_H_
#define _RLOTTIE_H_

#include <future>
#include <vector>
#include <memory>

#ifdef _WIN32
#ifdef LOT_BUILD
#ifdef DLL_EXPORT
#define LOT_EXPORT __declspec(dllexport)
#else
#define LOT_EXPORT
#endif
#else
#define LOT_EXPORT __declspec(dllimport)
#endif
#else
#ifdef __GNUC__
#if __GNUC__ >= 4
#define LOT_EXPORT __attribute__((visibility("default")))
#else
#define LOT_EXPORT
#endif
#else
#define LOT_EXPORT
#endif
#endif

class AnimationImpl;
struct LOTNode;
struct LOTLayerNode;

namespace rlottie {

class LOT_EXPORT Surface {
public:
    /**
     *  @brief Surface object constructor.
     *
     *  @param[in] buffer surface buffer.
     *  @param[in] width  surface width.
     *  @param[in] height  surface height.
     *  @param[in] bytesPerLine  number of bytes in a surface scanline.
     *
     *  @note Default surface format is ARGB32_Premultiplied.
     *
     *  @internal
     */
    Surface(uint32_t *buffer, size_t width, size_t height, size_t bytesPerLine);

    /**
     *  @brief Sets the Draw Area available on the Surface.
     *
     *  Lottie will use the draw region size to generate frame image
     *  and will update only the draw rgion of the surface.
     *
     *  @param[in] x      region area x position.
     *  @param[in] y      region area y position.
     *  @param[in] width  region area width.
     *  @param[in] height region area height.
     *
     *  @note Default surface format is ARGB32_Premultiplied.
     *  @note Default draw region area is [ 0 , 0, surface width , surface height]
     *
     *  @internal
     */
    void setDrawRegion(size_t x, size_t y, size_t width, size_t height);

    /**
     *  @brief Returns width of the surface.
     *
     *  @return surface width
     *
     *  @internal
     *
     */
    size_t width() const {return mWidth;}

    /**
     *  @brief Returns height of the surface.
     *
     *  @return surface height
     *
     *  @internal
     */
    size_t height() const {return mHeight;}

    /**
     *  @brief Returns number of bytes in the surface scanline.
     *
     *  @return number of bytes in scanline.
     *
     *  @internal
     */
    size_t  bytesPerLine() const {return mBytesPerLine;}

    /**
     *  @brief Returns buffer attached tp the surface.
     *
     *  @return buffer attaced to the Surface.
     *
     *  @internal
     */
    uint32_t *buffer() const {return mBuffer;}

    /**
     *  @brief Returns drawable area width of the surface.
     *
     *  @return drawable area width
     *
     *  @note Default value is width() of the surface
     *
     *  @internal
     *
     */
    size_t drawRegionWidth() const {return mDrawArea.w;}

    /**
     *  @brief Returns drawable area height of the surface.
     *
     *  @return drawable area height
     *
     *  @note Default value is height() of the surface
     *
     *  @internal
     */
    size_t drawRegionHeight() const {return mDrawArea.h;}

    /**
     *  @brief Returns drawable area's x position of the surface.
     *
     *  @return drawable area's x potition.
     *
     *  @note Default value is 0
     *
     *  @internal
     */
    size_t drawRegionPosX() const {return mDrawArea.x;}

    /**
     *  @brief Returns drawable area's y position of the surface.
     *
     *  @return drawable area's y potition.
     *
     *  @note Default value is 0
     *
     *  @internal
     */
    size_t drawRegionPosY() const {return mDrawArea.x;}

    /**
     *  @brief Default constructor.
     */
    Surface() = default;
private:
    uint32_t    *mBuffer{nullptr};
    size_t       mWidth{0};
    size_t       mHeight{0};
    size_t       mBytesPerLine{0};
    struct {
        size_t   x{0};
        size_t   y{0};
        size_t   w{0};
        size_t   h{0};
    }mDrawArea;
};

class LOT_EXPORT Animation {
public:

    /**
     *  @brief Constructs an animation object from file path.
     *
     *  @param[in] path Lottie resource file path
     *
     *  @return Animation object that can render the contents of the
     *          Lottie resource represented by file path.
     *
     *  @internal
     */
    static std::unique_ptr<Animation>
    loadFromFile(const std::string &path);

    /**
     *  @brief Constructs an animation object from JSON string data.
     *
     *  @param[in] jsonData The JSON string data.
     *  @param[in] key the string that will be used to cache the JSON string data.
     *  @param[in] resourcePath the path will be used to search for external resource.
     *
     *  @return Animation object that can render the contents of the
     *          Lottie resource represented by JSON string data.
     *
     *  @internal
     */
    static std::unique_ptr<Animation>
    loadFromData(std::string jsonData, const std::string &key, const std::string &resourcePath="");

    /**
     *  @brief Returns default framerate of the Lottie resource.
     *
     *  @return framerate of the Lottie resource
     *
     *  @internal
     *
     */
    double frameRate() const;

    /**
     *  @brief Returns total number of frames present in the Lottie resource.
     *
     *  @return frame count of the Lottie resource.
     *
     *  @note frame number starts with 0.
     *
     *  @internal
     */
    size_t totalFrame() const;

    /**
     *  @brief Returns default viewport size of the Lottie resource.
     *
     *  @param[out] width  default width of the viewport.
     *  @param[out] height default height of the viewport.
     *
     *  @internal
     *
     */
    void   size(size_t &width, size_t &height) const;

    /**
     *  @brief Returns total animation duration of Lottie resource in second.
     *         it uses totalFrame() and frameRate() to calculate the duration.
     *         duration = totalFrame() / frameRate().
     *
     *  @return total animation duration in second.
     *  @retval 0 if the Lottie resource has no animation.
     *
     *  @see totalFrame()
     *  @see frameRate()
     *
     *  @internal
     */
    double duration() const;

    /**
     *  @brief Returns frame number for a given position.
     *         this function helps to map the position value retuned
     *         by the animator to a frame number in side the Lottie resource.
     *         frame_number = lerp(start_frame, endframe, pos);
     *
     *  @param[in] pos normalized position value [0 ... 1]
     *
     *  @return frame numer maps to the position value [startFrame .... endFrame]
     *
     *  @internal
     */
    size_t frameAtPos(double pos);

    /**
     *  @brief Renders the content to surface Asynchronously.
     *         it gives a future in return to get the result of the
     *         rendering at a future point.
     *         To get best performance user has to start rendering as soon as
     *         it finds that content at {frameNo} has to be rendered and get the
     *         result from the future at the last moment when the surface is needed
     *         to draw into the screen.
     *
     *
     *  @param[in] frameNo Content corresponds to the @p frameNo needs to be drawn
     *  @param[in] surface Surface in which content will be drawn
     *
     *  @return future that will hold the result when rendering finished.
     *
     *  for Synchronus rendering @see renderSync
     *
     *  @see Surface
     *  @internal
     */
    std::future<Surface> render(size_t frameNo, Surface surface);

    /**
     *  @brief Renders the content to surface synchronously.
     *         for performance use the async rendering @see render
     *
     *  @param[in] frameNo Content corresponds to the @p frameNo needs to be drawn
     *  @param[in] surface Surface in which content will be drawn
     *
     *  @internal
     */
    void              renderSync(size_t frameNo, Surface surface);

    /**
     *  @brief Returns root layer of the composition updated with
     *         content of the Lottie resource at frame number @p frameNo.
     *
     *  @param[in] frameNo Content corresponds to the @p frameNo needs to be extracted.
     *  @param[in] width   content viewbox width
     *  @param[in] height  content viewbox height
     *
     *  @return Root layer node.
     *
     *  @internal
     */
    const LOTLayerNode * renderTree(size_t frameNo, size_t width, size_t height) const;

    /**
     *  @brief default destructor
     *
     *  @internal
     */
    ~Animation();

private:
    /**
     *  @brief default constructor
     *
     *  @internal
     */
    Animation();

    std::unique_ptr<AnimationImpl> d;
};

}  // namespace lotplayer

#endif  // _RLOTTIE_H_
