#include "vfont.h"



VFont::VFont(std::string fontPath)
{
    FT_Init_FreeType(&mLibrary);
    FT_New_Face(mLibrary, fontPath.c_str(), 0, &mFace);
}

VFont::~VFont()
{
    FT_Done_Face(mFace);
    FT_Done_FreeType(mLibrary);
}

int VFontHelper::moveTo(const FT_Vector *to, void *user)
{
    VPath path = *(VPath *)user;
    path.moveTo(TOFLOAT(to->x), TOFLOAT(to->y));
    return 0;
}

int VFontHelper::lineTo(const FT_Vector *to, void *user)
{
    VPath path = *(VPath *)user;
    path.lineTo(TOFLOAT(to->x), TOFLOAT(to->y));
    return 0;
}

int VFontHelper::conicTo(const FT_Vector *control, const FT_Vector *to, void *user)
{
    VPath path = *(VPath *)user;
    /*
    path.conicTo(
            TOFLOAT(control->x), TOFLOAT(control->y),
            TOFLOAT(to->x), TOFLOAT(to->y)
            );
    */
    return 0;
}

int VFontHelper::cubicTo(const FT_Vector *control1, const FT_Vector *control2,
        const FT_Vector *to, void *user)
{
    VPath path = *(VPath *)user;
    path.cubicTo(
            TOFLOAT(control1->x), TOFLOAT(control1->y),
            TOFLOAT(control2->x), TOFLOAT(control2->y),
            TOFLOAT(to->x), TOFLOAT(to->y));
    return 0;
}

VPath VFont::outline(FT_UInt glyphIndex)
{
    auto search = mMap.find(glyphIndex);
    if (search != mMap.end()) {
        return search->second;
    }
    else {
        FT_Outline_Funcs outline_funcs = {
            VFontHelper::moveTo,
            VFontHelper::lineTo,
            VFontHelper::conicTo,
            VFontHelper::cubicTo,
            0, 0
        };
        FT_Load_Glyph(mFace, glyphIndex, FT_LOAD_DEFAULT);
        VPath path;
        FT_Outline_Decompose(&mFace->glyph->outline, &outline_funcs, &path);
        mMap.insert({glyphIndex, path});
        return path;
    }
}
