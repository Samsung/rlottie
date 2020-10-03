#include <stdlib.h>
#include <string>

#include "rlottie.h"

#include <emscripten/bind.h>

using namespace emscripten;

typedef unsigned char uint8_t;

class __attribute__((visibility("default"))) RlottieWasm {
public:
    static std::unique_ptr<RlottieWasm> create(std::string jsonData)
    {
        const char *resource = jsonData.c_str();
        return std::unique_ptr<RlottieWasm>(new RlottieWasm(resource));
    }

    int frames() const { return mFrameCount; }

    bool load(std::string jsonData)
    {
        mPlayer = rlottie::Animation::loadFromData(std::move(jsonData), "", "",
                                                   false);
        mFrameCount = mPlayer ? mPlayer->totalFrame() : 0;
        return mPlayer ? true : false;
    }

    // canvas pixel pix[0] pix[1] pix[2] pix[3] {B G R A}
    // lottie pixel pix[0] pix[1] pix[2] pix[3] {R G B A}
    val render(int frame, int width, int height)
    {
        if (!mPlayer) return val(typed_memory_view<uint8_t>(0, nullptr));

        resize(width, height);
        mPlayer->renderSync(
            frame, rlottie::Surface((uint32_t *)mBuffer.get(), mWidth, mHeight,
                                    mWidth * 4));
        convertToCanvasFormat();

        return val(typed_memory_view(mWidth * mHeight * 4, mBuffer.get()));
    }

    void setFillColor(std::string keyPath, float r, float g, float b)
    {
        mPlayer->setValue<rlottie::Property::FillColor>(keyPath, rlottie::Color(r, g, b));
    }

    void setFillOpacity(std::string keyPath, float opacity)
    {
        mPlayer->setValue<rlottie::Property::FillOpacity>(keyPath, opacity);
    }

    void setStrokeColor(std::string keyPath, float r, float g, float b)
    {
        mPlayer->setValue<rlottie::Property::StrokeColor>(keyPath, rlottie::Color(r, g, b));
    }

    void setStrokeOpacity(std::string keyPath, float opacity)
    {
        mPlayer->setValue<rlottie::Property::StrokeOpacity>(keyPath, opacity);
    }

    void setStrokeWidth(std::string keyPath, float width)
    {
        mPlayer->setValue<rlottie::Property::StrokeWidth>(keyPath, width);
    }

    void setTrAnchor(std::string keyPath, float x, float y)
    {
        mPlayer->setValue<rlottie::Property::TrAnchor>(keyPath, rlottie::Point(x, y));
    }

    void setTrPosition(std::string keyPath, float x, float y)
    {
        mPlayer->setValue<rlottie::Property::TrPosition>(keyPath, rlottie::Point(x, y));
    }

    void setTrScale(std::string keyPath, float w, float h)
    {
        mPlayer->setValue<rlottie::Property::TrScale>(keyPath, rlottie::Size(w, h));
    }

    void setTrRotation(std::string keyPath, float degree)
    {
        mPlayer->setValue<rlottie::Property::TrRotation>(keyPath, degree);
    }

    void setTrOpacity(std::string keyPath, float opacity)
    {
        mPlayer->setValue<rlottie::Property::TrOpacity>(keyPath, opacity);
    }

    ~RlottieWasm() {}

private:
    void resize(int width, int height)
    {
        if (width == mWidth && height == mHeight) return;

        mWidth = width;
        mHeight = height;

        mBuffer = std::make_unique<uint8_t[]>(mWidth * mHeight * 4);
    }

    explicit RlottieWasm(const char *data)
    {
        mPlayer = rlottie::Animation::loadFromData(data, "", "", false);
        mFrameCount = mPlayer ? mPlayer->totalFrame() : 0;
    }

    void convertToCanvasFormat()
    {
        int      totalBytes = mWidth * mHeight * 4;
        uint8_t *buffer = mBuffer.get();
        for (int i = 0; i < totalBytes; i += 4) {
            unsigned char a = buffer[i + 3];
            // compute only if alpha is non zero
            if (a) {
                unsigned char r = buffer[i + 2];
                unsigned char g = buffer[i + 1];
                unsigned char b = buffer[i];

                if (a != 255) {  // un premultiply
                    r = (r * 255) / a;
                    g = (g * 255) / a;
                    b = (b * 255) / a;

                    buffer[i] = r;
                    buffer[i + 1] = g;
                    buffer[i + 2] = b;

                } else {
                    // only swizzle r and b
                    buffer[i] = r;
                    buffer[i + 2] = b;
                }
            }
        }
    }

private:
    int                                 mWidth{0};
    int                                 mHeight{0};
    int                                 mFrameCount{0};
    std::unique_ptr<uint8_t[]>          mBuffer;
    std::unique_ptr<rlottie::Animation> mPlayer;
};

// Binding code
EMSCRIPTEN_BINDINGS(rlottie_bindings)
{
    class_<RlottieWasm>("RlottieWasm")
        .constructor(&RlottieWasm::create)
        .function("load", &RlottieWasm::load, allow_raw_pointers())
        .function("frames", &RlottieWasm::frames)
        .function("render", &RlottieWasm::render)
        .function("setFillColor", &RlottieWasm::setFillColor)
        .function("setFillOpacity", &RlottieWasm::setFillOpacity)
        .function("setStrokeColor", &RlottieWasm::setStrokeColor)
        .function("setStrokeOpacity", &RlottieWasm::setStrokeOpacity)
        .function("setStrokeWidth", &RlottieWasm::setStrokeWidth)
        .function("setTrAnchor", &RlottieWasm::setTrAnchor)
        .function("setTrPosition", &RlottieWasm::setTrPosition)
        .function("setTrScale", &RlottieWasm::setTrScale)
        .function("setTrRotation", &RlottieWasm::setTrRotation)
        .function("setTrOpacity", &RlottieWasm::setTrOpacity);
}
