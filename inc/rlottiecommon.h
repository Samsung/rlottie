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

#ifndef _RLOTTIE_COMMON_H_
#define _RLOTTIE_COMMON_H_

#if defined _WIN32 || defined __CYGWIN__
  #ifdef LOT_BUILD
    #define LOT_EXPORT __declspec(dllexport)
  #else
    #define LOT_EXPORT __declspec(dllimport)
  #endif
#else
  #ifdef LOT_BUILD
      #define LOT_EXPORT __attribute__ ((visibility ("default")))
  #else
      #define LOT_EXPORT
  #endif
#endif


/**
 * @defgroup Lottie_Animation Lottie_Animation
 *
 * Lottie Animation is a modern style vector based animation design. Its animation
 * resource(within json format) could be generated by Adobe After Effect using
 * bodymovin plugin. You can find a good examples in Lottie Community which
 * shares many free resources(see: www.lottiefiles.com).
 *
 * This Lottie_Animation is a common engine to manipulate, control Lottie
 * Animation from the Lottie resource - json file. It provides a scene-graph
 * node tree per frames by user demand as well as rasterized frame images.
 *
 */

/**
 * @ingroup Lottie_Animation
 */


/**
 * @brief Enumeration for Lottie Player error code.
 */
typedef enum
{
   //TODO: Coding convention??
    LOT_ANIMATION_ERROR_NONE = 0,
    LOT_ANIMATION_ERROR_NOT_PERMITTED,
    LOT_ANIMATION_ERROR_OUT_OF_MEMORY,
    LOT_ANIMATION_ERROR_INVALID_PARAMETER,
    LOT_ANIMATION_ERROR_RESULT_OUT_OF_RANGE,
    LOT_ANIMATION_ERROR_ALREADY_IN_PROGRESS,
    LOT_ANIMATION_ERROR_UNKNOWN
} LOTErrorType;

typedef enum
{
    BrushSolid = 0,
    BrushGradient
} LOTBrushType;

typedef enum
{
    FillEvenOdd = 0,
    FillWinding
} LOTFillRule;

typedef enum
{
    JoinMiter = 0,
    JoinBevel,
    JoinRound
} LOTJoinStyle;

typedef enum
{
    CapFlat = 0,
    CapSquare,
    CapRound
} LOTCapStyle;

typedef enum
{
    GradientLinear = 0,
    GradientRadial
} LOTGradientType;

typedef struct LOTGradientStop
{
    float         pos;
    unsigned char r, g, b, a;
} LOTGradientStop;

typedef enum
{
    MaskAdd = 0,
    MaskSubstract,
    MaskIntersect,
    MaskDifference
} LOTMaskType;

typedef struct LOTMask {
    struct {
        const float *ptPtr;
        int          ptCount;
        const char*  elmPtr;
        int          elmCount;
    } mPath;
    LOTMaskType mMode;
    int mAlpha;
}LOTMask;

typedef enum
{
    MatteNone = 0,
    MatteAlpha,
    MatteAlphaInv,
    MatteLuma,
    MatteLumaInv
} LOTMatteType;

typedef struct LOTNode {

#define ChangeFlagNone 0x0000
#define ChangeFlagPath 0x0001
#define ChangeFlagPaint 0x0010
#define ChangeFlagAll (ChangeFlagPath & ChangeFlagPaint)

    struct {
        const float *ptPtr;
        int          ptCount;
        const char*  elmPtr;
        int          elmCount;
    } mPath;

    struct {
        unsigned char r, g, b, a;
    } mColor;

    struct {
        unsigned char  enable;
        int       width;
        LOTCapStyle  cap;
        LOTJoinStyle join;
        int       meterLimit;
        float*    dashArray;
        int       dashArraySize;
    } mStroke;

    struct {
        LOTGradientType type;
        LOTGradientStop *stopPtr;
        unsigned int stopCount;
        struct {
            float x, y;
        } start, end, center, focal;
        float cradius;
        float fradius;
    } mGradient;

    struct {
        unsigned char* data;
        int width;
        int height;
        struct {
           float m11; float m12; float m13;
           float m21; float m22; float m23;
           float m31; float m32; float m33;
        } mMatrix;
    } mImageInfo;

    int       mFlag;
    LOTBrushType mBrushType;
    LOTFillRule  mFillRule;
} LOTNode;



typedef struct LOTLayerNode {

    struct {
        LOTMask        *ptr;
        unsigned int    size;
    } mMaskList;

    struct {
        const float *ptPtr;
        int          ptCount;
        const char*  elmPtr;
        int          elmCount;
    } mClipPath;

    struct {
        struct LOTLayerNode   **ptr;
        unsigned int          size;
    } mLayerList;

    struct {
        LOTNode        **ptr;
        unsigned int   size;
    } mNodeList;

    LOTMatteType mMatte;
    int          mVisible;
    int          mAlpha;
    const char  *name;

} LOTLayerNode;

/**
 * @}
 */

#endif  // _RLOTTIE_COMMON_H_
