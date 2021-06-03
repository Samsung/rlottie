#include "gif.h"
#include <rlottie.h>

#include<cmath>
#include<iostream>
#include<string>
#include<vector>
#include<array>

#ifndef _WIN32
#include<libgen.h>
#else
#include <windows.h>
#include <stdlib.h>
#endif

class GifBuilder {
public:
    explicit GifBuilder(const std::string &fileName , const uint32_t width,
                        const uint32_t height, const int bgColor=0xffffffff, const uint32_t delay = 2)
    {
        GifBegin(&handle, fileName.c_str(), width, height, delay);
        bgColorR = (uint8_t) ((bgColor & 0xff0000) >> 16);
        bgColorG = (uint8_t) ((bgColor & 0x00ff00) >> 8);
        bgColorB = (uint8_t) ((bgColor & 0x0000ff));
    }
    ~GifBuilder()
    {
        GifEnd(&handle);
    }
    void addFrame(rlottie::Surface &s, uint32_t delay = 2)
    {
        argbTorgba(s);
        GifWriteFrame(&handle,
                      reinterpret_cast<uint8_t *>(s.buffer()),
                      s.width(),
                      s.height(),
                      delay);
    }
    void argbTorgba(rlottie::Surface &s)
    {
        uint8_t *buffer = reinterpret_cast<uint8_t *>(s.buffer());
        uint32_t totalBytes = s.height() * s.bytesPerLine();

        for (uint32_t i = 0; i < totalBytes; i += 4) {
           unsigned char a = buffer[i+3];
           // compute only if alpha is non zero
           if (a) {
               unsigned char r = buffer[i+2];
               unsigned char g = buffer[i+1];
               unsigned char b = buffer[i];

               if (a != 255) { //un premultiply
                   unsigned char r2 = (unsigned char) ((float) bgColorR * ((float) (255 - a) / 255));
                   unsigned char g2 = (unsigned char) ((float) bgColorG * ((float) (255 - a) / 255));
                   unsigned char b2 = (unsigned char) ((float) bgColorB * ((float) (255 - a) / 255));
                   buffer[i] = r + r2;
                   buffer[i+1] = g + g2;
                   buffer[i+2] = b + b2;

               } else {
                 // only swizzle r and b
                 buffer[i] = r;
                 buffer[i+2] = b;
               }
           } else {
               buffer[i+2] = bgColorB;
               buffer[i+1] = bgColorG;
               buffer[i] = bgColorR;
           }
        }
    }

private:
    GifWriter      handle;
    uint8_t bgColorR, bgColorG, bgColorB;
};

class App {
public:
    int render(uint32_t w, uint32_t h)
    {
        auto player = rlottie::Animation::loadFromFile(fileName);
        if (!player) return help();

        const double averageDelay = 100 / player->frameRate();
        const double averageDelayFractional = averageDelay - (int)averageDelay;
        const int longFrameFrequency = averageDelayFractional > 0 ? (int)round(1 / averageDelayFractional) : 1;

        // the most of gif viewers does not properly play gif with duration = 1
        const size_t longFrameDuration = (size_t)std::max(2.0, ceil(averageDelay));
        const size_t shortFrameDuration = (size_t)std::max(2.0, floor(averageDelay));

        auto buffer = std::unique_ptr<uint32_t[]>(new uint32_t[w * h]);
        size_t frameCount = player->totalFrame();

        GifBuilder builder(gifName.data(), w, h, bgColor);
        for (size_t i = 0; i < frameCount ; i++) {
            rlottie::Surface surface(buffer.get(), w, h, w * 4);
            player->renderSync(i, surface);

            const auto currentDelay = i % longFrameFrequency ? shortFrameDuration : longFrameDuration;
            builder.addFrame(surface, currentDelay);
        }
        return result();
    }

    int setup(int argc, char **argv, size_t *width, size_t *height)
    {
        char *path{nullptr};

        *width = *height = 200;   //default gif size

        if (argc > 1) path = argv[1];
        if (argc > 2) {
            char tmp[20];
            char *x = strstr(argv[2], "x");
            if (x) {
                snprintf(tmp, x - argv[2] + 1, "%s", argv[2]);
                *width = atoi(tmp);
                snprintf(tmp, sizeof(tmp), "%s", x + 1);
                *height = atoi(tmp);
            }
        }
        if (argc > 3) bgColor = strtol(argv[3], NULL, 16);

        if (!path) return help();

        std::array<char, 5000> memory;

#ifdef _WIN32
        path = _fullpath(memory.data(), path, memory.size());
#else
        path = realpath(path, memory.data());
#endif
        if (!path) return help();

        fileName = std::string(path);

        if (!jsonFile()) return help();

        gifName = basename(fileName);
        gifName.append(".gif");
        return 0;
    }

private:
    std::string basename(const std::string &str)
    {
        return str.substr(str.find_last_of("/\\") + 1);
    }

    bool jsonFile() {
        std::string extn = ".json";
        if ( fileName.size() <= extn.size() ||
             fileName.substr(fileName.size()- extn.size()) != extn )
            return false;

        return true;
    }

    int result() {
        std::cout<<"Generated GIF file : "<<gifName<<std::endl;
        return 0;
    }

    int help() {
        std::cout<<"Usage: \n   lottie2gif [lottieFileName] [Resolution] [bgColor]\n\nExamples: \n    $ lottie2gif input.json\n    $ lottie2gif input.json 200x200\n    $ lottie2gif input.json 200x200 ff00ff\n\n";
        return 1;
    }

private:
    int bgColor = 0xffffffff;
    std::string fileName;
    std::string gifName;
};

int
main(int argc, char **argv)
{
    App app;
    size_t w, h;

    if (app.setup(argc, argv, &w, &h)) return 1;

    app.render(w, h);

    return 0;
}
