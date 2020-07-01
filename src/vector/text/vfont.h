#ifndef VFONT_H
#define VFONT_H

#include "vpath.h"
#include <string>
#include <unordered_map>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H


class VFontHelper {
#define TOFLOAT(a) ((float) a)
public:
    static int moveTo(const FT_Vector *to, void *user);
    static int lineTo(const FT_Vector *to, void *user);
    static int conicTo(const FT_Vector *control, const FT_Vector *to, void *user);
    static int cubicTo(const FT_Vector *control1, const FT_Vector *control2, const FT_Vector *to, void *user);
};

class VFont {
public:
    VFont(std::string fontPath);
    ~VFont();
    VPath outline(FT_UInt glyphIndex);
private:
    FT_Library                          mLibrary;
    FT_Face                             mFace;
    std::unordered_map<FT_UInt, VPath>  mMap;
};
#endif
