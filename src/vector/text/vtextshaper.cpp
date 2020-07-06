#include "vtextshaper.h"

#ifdef LOTTIE_FONTPATH_SUPPORT

#include <unordered_map>
#include <mutex>
#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <fontconfig/fontconfig.h>
#include <hb.h>
#include <hb-ft.h>


std::string fontKey(FontProperty prop) {
    std::string key{prop.family};
    if (prop.style) key += prop.style;
    if (prop.weight) key += prop.weight;
    key += std::to_string(prop.size);
    return key;
}

class VFontConfig
{
public:
    static VFontConfig& instance() {
        static VFontConfig Singleton;
        return Singleton;
    }
    std::string fontPath(const char* fontFamily, const char* fontStyle, const char* fontWeight)
    {
        std::string result;
        FcPattern* pat = FcPatternCreate();

        FcPatternAddString (pat, FC_FAMILY, (FcChar8*) fontFamily);

        if (fontStyle) FcPatternAddString (pat, FC_STYLE, (FcChar8*) fontStyle);

        if (fontWeight) FcPatternAddInteger(pat, FC_WEIGHT, weight(fontWeight));

        FcConfigSubstitute(mFontconfig, pat, FcMatchPattern);
        FcDefaultSubstitute(pat);

        /* do matching */
        FcResult res;
        FcPattern* font = FcFontMatch(mFontconfig, pat, &res);
        if (font) {
            FcChar8* filename = nullptr;
            if (FcPatternGetString(font, FC_FILE, 0, &filename) == FcResultMatch) {
                result = (char*)filename;
            }
        } else {
             printf("Font config failed to find font family %s \n", fontFamily);
        }
        FcPatternDestroy(pat);
        return result;
    }

private:
    int weight(const char* fontWeight) {
        if (!strcasecmp(fontWeight, "normal")) return FC_WEIGHT_NORMAL;
        else if (!strcasecmp(fontWeight, "thin")) return FC_WEIGHT_THIN;
        else if (!strcasecmp(fontWeight, "light")) return FC_WEIGHT_LIGHT;
        else if (!strcasecmp(fontWeight, "bold")) return FC_WEIGHT_NORMAL;
        else return FC_WEIGHT_NORMAL;
    }
    VFontConfig() {
        mFontconfig = FcInitLoadConfigAndFonts();
    }
    ~VFontConfig() {
        FcConfigDestroy(mFontconfig);
    }
private:
    FcConfig *mFontconfig{nullptr};
};

class VFont
{
public:
    VFont(FT_Library library, std::string fontPath, int size);
    ~VFont();
    VPath outline(FT_UInt glyphIndex);
    FT_Face ftFace() const;
private:
#define FROM_FT_COORD(x) ((x) / 64.)
#define MINUS(x) ((-1) * (x))
    static int moveTo(const FT_Vector *to, void *user);
    static int lineTo(const FT_Vector *to, void *user);
    static int conicTo(const FT_Vector *control, const FT_Vector *to, void *user);
    static int cubicTo(const FT_Vector *control1, const FT_Vector *control2,
            const FT_Vector *to, void *user);
private:
    static FT_Face                      mFace;
    constexpr static FT_Outline_Funcs   mOutline_funcs = {
        moveTo,
        lineTo,
        conicTo,
        cubicTo,
        0, 0
    };
    std::unordered_map<FT_UInt, VPath>  mMap;
    std::mutex                          mMutex;
};

FT_Face VFont::mFace = {};
constexpr FT_Outline_Funcs VFont::mOutline_funcs;

VFont::VFont(FT_Library library, std::string fontPath, int size) {
    if (FT_New_Face(library, fontPath.c_str(), 0, &mFace)) {
        std::cout << "Font file load failed :" << fontPath << std::endl;
    }
    FT_Set_Char_Size(mFace, size << 6, size << 6, 0, 0);
}

VFont::~VFont() {
    FT_Done_Face(mFace);
}

FT_Face VFont::ftFace() const {
    return mFace;
}

int VFont::moveTo(const FT_Vector *to, void *user)
{
    VPath *path = (VPath *)user;
    path->moveTo(FROM_FT_COORD(to->x), FROM_FT_COORD(MINUS(to->y)));
    return 0;
}

int VFont::lineTo(const FT_Vector *to, void *user)
{
    VPath *path = (VPath *)user;
    path->lineTo(FROM_FT_COORD(to->x), FROM_FT_COORD(MINUS(to->y)));
    return 0;
}

int VFont::conicTo(const FT_Vector *control, const FT_Vector *to, void *user)
{
    VPath *path = (VPath *)user;
    path->cubicTo(
            FROM_FT_COORD(control->x), FROM_FT_COORD(MINUS(control->y)),
            FROM_FT_COORD(control->x), FROM_FT_COORD(MINUS(control->y)),
            FROM_FT_COORD(to->x), FROM_FT_COORD(MINUS(to->y)));
    return 0;
}

int VFont::cubicTo(const FT_Vector *control1, const FT_Vector *control2,
        const FT_Vector *to, void *user)
{
    VPath *path = (VPath *)user;
    path->cubicTo(
            FROM_FT_COORD(control1->x), FROM_FT_COORD(MINUS(control1->y)),
            FROM_FT_COORD(control2->x), FROM_FT_COORD(MINUS(control2->y)),
            FROM_FT_COORD(to->x), FROM_FT_COORD(MINUS(to->y)));
    return 0;
}

VPath VFont::outline(FT_UInt glyphIndex)
{
    std::lock_guard<std::mutex> guard(mMutex);
    auto search = mMap.find(glyphIndex);
    if (search != mMap.end()) {
        return search->second;
    }
    else {
        VPath path;

        FT_Load_Glyph(mFace, glyphIndex, FT_LOAD_DEFAULT);
        FT_Outline_Decompose(&mFace->glyph->outline, &mOutline_funcs, &path);
        mMap.insert({glyphIndex, path});

        return path;
    }
}

class VFontDB
{
public:
    static VFontDB& instance() {
        static VFontDB Singleton;
        return Singleton;
    }
    VFont *font(FontProperty prop) {
        std::lock_guard<std::mutex> guard(mMutex);

        auto key = fontKey(prop);
        auto search = mMap.find(key);
        if (search != mMap.end()) {
            return search->second.get();
        }
        else {
            auto path = VFontConfig::instance().fontPath(prop.family, prop.style, prop.weight);
            if (path.empty()) return nullptr;
            mMap.insert(std::make_pair(key,
                        std::make_unique<VFont>(mLibrary, std::move(path), prop.size)));
            return mMap[key].get();
        }
    }
private:
    VFontDB() {
        if (FT_Init_FreeType(&mLibrary)) {
            std::cout<<"FreeType Library Init Failed \n";
        }
    }
    ~VFontDB() {
        mMap.clear();
        FT_Done_FreeType(mLibrary);
    }
private:
    FT_Library                                              mLibrary;
    std::unordered_map<std::string, std::unique_ptr<VFont>> mMap;
    std::mutex                                              mMutex;
};

class VTextShaperImpl {
public:
    VTextShaperImpl(FontProperty prop);
    ~VTextShaperImpl();
    VGlyphList shape(std::string& text);
private:
    VFont       *mFont;
    hb_font_t   *mHbFont;
    hb_buffer_t *mHbBuffer;
};

VTextShaperImpl::VTextShaperImpl(FontProperty prop) {
    mFont = VFontDB::instance().font(prop);
    mHbBuffer = hb_buffer_create();
    mHbFont = hb_ft_font_create (mFont->ftFace(), NULL);
}

VTextShaperImpl::~VTextShaperImpl() {
    if (mHbBuffer) hb_buffer_destroy(mHbBuffer);
    if (mHbFont) hb_font_destroy(mHbFont);
}

VGlyphList VTextShaperImpl::shape(std::string& text) {
    hb_buffer_t *hb_buffer;
    hb_buffer = hb_buffer_create ();
    hb_buffer_add_utf8 (hb_buffer, text.c_str(), text.length(), 0, text.length());

    hb_buffer_set_direction(hb_buffer, HB_DIRECTION_LTR);
    hb_buffer_set_script(hb_buffer, HB_SCRIPT_LATIN);
    hb_buffer_set_language(hb_buffer, hb_language_from_string("en_US", -1));

    hb_shape(mHbFont, hb_buffer, NULL, 0);

    unsigned int len = hb_buffer_get_length (hb_buffer);
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos (hb_buffer, NULL);
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions (hb_buffer, NULL);

    VGlyphList result;
    result.reserve(len);
    for (unsigned int i = 0; i < len; i++) {
        hb_codepoint_t gid   = info[i].codepoint;
        float x_advance = pos[i].x_advance / 64.;
        float y_advance = pos[i].y_advance / 64.;
        float x_offset  = pos[i].x_offset / 64.;
        float y_offset  = pos[i].y_offset / 64.;

        result.push_back(VGlyphData {mFont->outline(gid),
                x_advance, y_advance, x_offset, y_offset});
    }

    hb_buffer_destroy(hb_buffer);

    return result;
}

VTextShaper::VTextShaper(FontProperty prop) {
    mImpl = std::make_unique<VTextShaperImpl>(prop);
}

VTextShaper::~VTextShaper() {
}

VGlyphList VTextShaper::shape(std::string& text) {
    return mImpl->shape(text);
}

#else
VTextShaper::VTextShaper(FontProperty /*prop*/) {
}

VTextShaper::~VTextShaper() {
}

VGlyphList VTextShaper::shape(std::string& /*text*/) {
    return {};
}

class VTextShaperImpl {
};
#endif
