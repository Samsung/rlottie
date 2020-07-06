#include "vpath.h"
#include <vector>
#include <string>
#include <memory>


class VTextShaperImpl;

struct FontProperty
{
    const char *family;
    const char *style;
    const char *weight;
    int         size;
};

struct VGlyphData
{
    VPath         path;
    float         x_advance{0};
    float         y_advance{0};
    float         x_offset{0};
    float         x_yoffset{0};
};

using VGlyphList = std::vector<VGlyphData>;

class VTextShaper
{
public:
    VTextShaper(FontProperty prop);
    ~VTextShaper();
    VGlyphList shape(std::string& text);
private:
    std::unique_ptr<VTextShaperImpl>    mImpl;
};
