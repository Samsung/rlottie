#include <rlottie.h>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace {

struct Size {
    size_t width{0};
    size_t height{0};
};

struct Options {
    std::vector<std::string> assets;
    std::vector<Size> sizes;
    size_t iterations{120};
    size_t warmup{10};
    bool async{false};
    bool csv{false};
    bool profile{false};
    std::string dumpFirstFrame;
};

bool startsWith(const std::string &value, const std::string &prefix)
{
    return value.rfind(prefix, 0) == 0;
}

bool isAbsolutePath(const std::string &path)
{
    if (path.empty()) return false;
#ifdef _WIN32
    return path.size() > 2 && path[1] == ':';
#else
    return path[0] == '/';
#endif
}

std::string trim(const std::string &value)
{
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

bool parseSize(const std::string &text, Size &size)
{
    const auto pos = text.find('x');
    if (pos == std::string::npos) return false;

    auto widthText = trim(text.substr(0, pos));
    auto heightText = trim(text.substr(pos + 1));
    if (widthText.empty() || heightText.empty()) return false;

    size.width = static_cast<size_t>(std::stoul(widthText));
    size.height = static_cast<size_t>(std::stoul(heightText));
    return size.width > 0 && size.height > 0;
}

std::string assetPath(const std::string &asset)
{
    if (isAbsolutePath(asset)) return asset;
    return std::string(DEMO_DIR) + asset;
}

void appendAsset(Options &options, const std::string &asset)
{
    auto value = trim(asset);
    if (!value.empty()) options.assets.push_back(value);
}

bool loadAssetList(Options &options, const std::string &path)
{
    std::ifstream stream(path);
    if (!stream.is_open()) return false;

    std::string line;
    while (std::getline(stream, line)) {
        auto value = trim(line);
        if (value.empty() || startsWith(value, "#")) continue;
        appendAsset(options, value);
    }
    return true;
}

int help()
{
    std::cout
        << "Usage: lottiebench [options]\n\n"
        << "Options:\n"
        << "  --asset <relative-or-absolute-json>\n"
        << "  --asset-list <file>\n"
        << "  --size <WxH>               repeatable\n"
        << "  --iterations <count>\n"
        << "  --warmup <count>\n"
        << "  --async                    use async render path\n"
        << "  --csv                      emit CSV output\n"
        << "  --profile                  emit rlottie internal timing to stderr\n"
        << "  --dump-first-frame <ppm>   write first-frame RGB dump\n"
        << "  --help\n\n"
        << "Example:\n"
        << "  lottiebench --asset mask.json --size 240x240 --size 360x360\n"
        << "  lottiebench --asset-list benchmarks/tizen_cpu_corpus.txt --size 240x240 --csv\n";
    return 0;
}

bool parseOptions(int argc, char **argv, Options &options)
{
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--help" || arg == "-h") {
            help();
            return false;
        } else if (arg == "--asset" && index + 1 < argc) {
            appendAsset(options, argv[++index]);
        } else if (arg == "--asset-list" && index + 1 < argc) {
            if (!loadAssetList(options, argv[++index])) {
                std::cerr << "Failed to load asset list\n";
                return false;
            }
        } else if (arg == "--size" && index + 1 < argc) {
            Size size;
            if (!parseSize(argv[++index], size)) {
                std::cerr << "Invalid size\n";
                return false;
            }
            options.sizes.push_back(size);
        } else if (arg == "--iterations" && index + 1 < argc) {
            options.iterations = static_cast<size_t>(std::stoul(argv[++index]));
        } else if (arg == "--warmup" && index + 1 < argc) {
            options.warmup = static_cast<size_t>(std::stoul(argv[++index]));
        } else if (arg == "--async") {
            options.async = true;
        } else if (arg == "--csv") {
            options.csv = true;
        } else if (arg == "--profile") {
            options.profile = true;
        } else if (arg == "--dump-first-frame" && index + 1 < argc) {
            options.dumpFirstFrame = argv[++index];
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }

    if (options.assets.empty()) {
        std::cerr << "No assets provided\n";
        return false;
    }
    if (options.sizes.empty()) {
        options.sizes.push_back({240, 240});
        options.sizes.push_back({360, 360});
    }
    if (!options.dumpFirstFrame.empty() &&
        (options.assets.size() != 1 || options.sizes.size() != 1)) {
        std::cerr << "--dump-first-frame requires exactly one asset and one size\n";
        return false;
    }
    return true;
}

struct Metrics {
    std::string asset;
    Size size;
    size_t frames{0};
    size_t iterations{0};
    double parseMs{0};
    double firstFrameMs{0};
    double steadyMs{0};
    double avgFrameMs{0};
    double fps{0};
    uint64_t rssParseKb{0};
    uint64_t rssFirstFrameKb{0};
    uint64_t rssSteadyKb{0};
    uint64_t nonzeroPixels{0};
    uint64_t alphaSum{0};
    uint64_t redSum{0};
    uint64_t greenSum{0};
    uint64_t blueSum{0};
};

void printProfileStats(const std::string &asset, Size size, bool async,
                       const char *phase,
                       const rlottie::PerformanceStats &stats)
{
    const char *mode = async ? "async" : "sync";
    std::cerr << "profile," << phase << "," << asset << "," << size.width
              << "," << size.height << "," << mode
              << ",composition_update_calls=" << stats.compositionUpdate.calls
              << ",composition_update_ms=" << stats.compositionUpdate.totalMs
              << ",composition_render_calls=" << stats.compositionRender.calls
              << ",composition_render_ms=" << stats.compositionRender.totalMs
              << ",comp_update_calls="
              << stats.compLayerUpdateContent.calls
              << ",comp_update_ms="
              << stats.compLayerUpdateContent.totalMs
              << ",shape_update_calls="
              << stats.shapeLayerUpdateContent.calls
              << ",shape_update_ms="
              << stats.shapeLayerUpdateContent.totalMs
              << ",paint_update_calls="
              << stats.paintUpdateRenderNode.calls
              << ",paint_update_ms="
              << stats.paintUpdateRenderNode.totalMs
              << ",render_matte_calls=" << stats.renderMatteLayer.calls
              << ",render_matte_ms=" << stats.renderMatteLayer.totalMs << "\n";
}

uint64_t currentRssKb()
{
#if defined(__APPLE__)
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) ==
        KERN_SUCCESS) {
        return static_cast<uint64_t>(info.resident_size / 1024);
    }
    return 0;
#elif defined(__linux__)
    std::ifstream stream("/proc/self/statm");
    uint64_t total = 0;
    uint64_t resident = 0;
    if (!(stream >> total >> resident)) return 0;
    const auto pageSize = static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
    return (resident * pageSize) / 1024;
#else
    return 0;
#endif
}

void captureSignature(const uint32_t *buffer, size_t pixelCount, Metrics &metrics)
{
    metrics.nonzeroPixels = 0;
    metrics.alphaSum = 0;
    metrics.redSum = 0;
    metrics.greenSum = 0;
    metrics.blueSum = 0;

    for (size_t i = 0; i < pixelCount; ++i) {
        const auto px = buffer[i];
        const auto a = uint8_t(px >> 24);
        const auto r = uint8_t((px >> 16) & 0xff);
        const auto g = uint8_t((px >> 8) & 0xff);
        const auto b = uint8_t(px & 0xff);
        if (a) ++metrics.nonzeroPixels;
        metrics.alphaSum += a;
        metrics.redSum += r;
        metrics.greenSum += g;
        metrics.blueSum += b;
    }
}

bool writePpm(const std::string &path, const uint32_t *buffer, Size size)
{
    std::ofstream stream(path, std::ios::binary);
    if (!stream.is_open()) return false;

    stream << "P6\n" << size.width << " " << size.height << "\n255\n";
    for (size_t i = 0; i < size.width * size.height; ++i) {
        const auto px = buffer[i];
        const unsigned char rgb[3] = {
            static_cast<unsigned char>((px >> 16) & 0xff),
            static_cast<unsigned char>((px >> 8) & 0xff),
            static_cast<unsigned char>(px & 0xff),
        };
        stream.write(reinterpret_cast<const char *>(rgb), sizeof(rgb));
    }
    return stream.good();
}

Metrics runCase(const std::string &asset, Size size, const Options &options)
{
    Metrics metrics;
    metrics.asset = asset;
    metrics.size = size;
    metrics.iterations = options.iterations;

    auto parseStart = std::chrono::steady_clock::now();
    auto animation = rlottie::Animation::loadFromFile(assetPath(asset));
    auto parseEnd = std::chrono::steady_clock::now();
    metrics.parseMs = std::chrono::duration<double, std::milli>(parseEnd -
                                                                parseStart)
                          .count();
    metrics.rssParseKb = currentRssKb();

    if (!animation) return metrics;

    metrics.frames = animation->totalFrame();
    if (!metrics.frames) return metrics;

    std::vector<uint32_t> buffer(size.width * size.height, 0u);
    rlottie::Surface surface(buffer.data(), size.width, size.height,
                             size.width * 4);

    auto renderFrame = [&](size_t frameNo) {
        if (options.async) {
            auto handle = animation->render(frameNo, surface);
            handle.get();
        } else {
            animation->renderSync(frameNo, surface);
        }
    };

    if (options.profile) {
        rlottie::configurePerformanceStats(true);
        rlottie::resetPerformanceStats();
    }

    auto firstStart = std::chrono::steady_clock::now();
    renderFrame(0);
    auto firstEnd = std::chrono::steady_clock::now();
    metrics.firstFrameMs =
        std::chrono::duration<double, std::milli>(firstEnd - firstStart).count();
    metrics.rssFirstFrameKb = currentRssKb();
    captureSignature(buffer.data(), size.width * size.height, metrics);
    if (!options.dumpFirstFrame.empty() &&
        !writePpm(options.dumpFirstFrame, buffer.data(), size)) {
        std::cerr << "Failed to dump first frame: " << options.dumpFirstFrame
                  << "\n";
    }
    if (options.profile) {
        printProfileStats(asset, size, options.async, "first_frame",
                          rlottie::performanceStats());
    }

    for (size_t i = 0; i < options.warmup; ++i) {
        renderFrame(i % metrics.frames);
    }
    if (options.profile) rlottie::resetPerformanceStats();

    auto steadyStart = std::chrono::steady_clock::now();
    for (size_t i = 0; i < options.iterations; ++i) {
        renderFrame(i % metrics.frames);
    }
    auto steadyEnd = std::chrono::steady_clock::now();

    metrics.steadyMs =
        std::chrono::duration<double, std::milli>(steadyEnd - steadyStart).count();
    metrics.avgFrameMs =
        options.iterations ? metrics.steadyMs / options.iterations : 0.0;
    metrics.fps = metrics.avgFrameMs > 0.0 ? 1000.0 / metrics.avgFrameMs : 0.0;
    metrics.rssSteadyKb = currentRssKb();
    if (options.profile) {
        printProfileStats(asset, size, options.async, "steady",
                          rlottie::performanceStats());
        rlottie::configurePerformanceStats(false);
    }
    return metrics;
}

void printHeader(bool csv)
{
    if (csv) {
        std::cout << "asset,width,height,mode,frames,iterations,parse_ms,first_frame_ms,steady_ms,avg_frame_ms,fps,rss_parse_kb,rss_first_frame_kb,rss_steady_kb,nonzero_pixels,alpha_sum,red_sum,green_sum,blue_sum\n";
    } else {
        std::cout << "asset,width,height,mode,frames,iterations,parse_ms,first_frame_ms,steady_ms,avg_frame_ms,fps,rss_parse_kb,rss_first_frame_kb,rss_steady_kb,nonzero_pixels,alpha_sum,red_sum,green_sum,blue_sum\n";
    }
}

void printMetrics(const Metrics &metrics, bool async, bool csv)
{
    const char *mode = async ? "async" : "sync";
    std::cout << metrics.asset << "," << metrics.size.width << ","
              << metrics.size.height << "," << mode << "," << metrics.frames
              << "," << metrics.iterations << "," << metrics.parseMs << ","
              << metrics.firstFrameMs << "," << metrics.steadyMs << ","
              << metrics.avgFrameMs << "," << metrics.fps << ","
              << metrics.rssParseKb << "," << metrics.rssFirstFrameKb << ","
              << metrics.rssSteadyKb << ","
              << metrics.nonzeroPixels << "," << metrics.alphaSum << ","
              << metrics.redSum << "," << metrics.greenSum << ","
              << metrics.blueSum << "\n";
    if (!csv && metrics.frames == 0) {
        std::cerr << "Failed to load asset: " << metrics.asset << "\n";
    }
}

}  // namespace

int main(int argc, char **argv)
{
    Options options;
    if (!parseOptions(argc, argv, options)) return 1;

    printHeader(options.csv);
    for (const auto &asset : options.assets) {
        for (const auto &size : options.sizes) {
            auto metrics = runCase(asset, size, options);
            printMetrics(metrics, options.async, options.csv);
        }
    }
    return 0;
}
