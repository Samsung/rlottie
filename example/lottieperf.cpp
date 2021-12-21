#include <memory>
#include <vector>
#include <dirent.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <cstring>

#include <rlottie.h>

static bool isJsonFile(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if(!dot || dot == filename) return false;
  return !strcmp(dot + 1, "json");
}

static std::vector<std::string>
jsonFiles(const std::string &dirName)
{
    DIR *d;
    struct dirent *dir;
    std::vector<std::string> result;
    d = opendir(dirName.c_str());
    if (d) {
      while ((dir = readdir(d)) != NULL) {
        if (isJsonFile(dir->d_name))
          result.push_back(dirName + dir->d_name);
      }
      closedir(d);
    }

    std::sort(result.begin(), result.end(), [](auto & a, auto &b){return a < b;});

    return result;
}

class Renderer
{
public:
    explicit Renderer(const std::string& filename)
    {
        _animation = rlottie::Animation::loadFromFile(filename);
        _frames = _animation->totalFrame();
        _buffer = std::make_unique<uint32_t[]>(100 * 100);
        _surface = rlottie::Surface(_buffer.get(), 100, 100, 100 * 4);
    }
    void render()
    {
        if (_cur >= _frames) _cur = 0;
        _animation->renderSync(_cur++, _surface);
    }
    void renderAsync()
    {
        if (_cur >= _frames) _cur = 0;
        _future = _animation->render(_cur++, _surface);
    }
    void get()
    {
        if (_future.valid()) _future.get();
    }
private:
    std::unique_ptr<uint32_t[]>         _buffer;
    std::unique_ptr<rlottie::Animation> _animation;
    size_t                              _frames{0};
    size_t                              _cur{0};
    rlottie::Surface                   _surface;
    std::future<rlottie::Surface>      _future;
};

class PerfTest
{
public:
    explicit PerfTest(size_t resourceCount, size_t iterations):
        _resourceCount(resourceCount), _iterations(iterations)
    {
        _resourceList = jsonFiles(std::string(DEMO_DIR));
    }
    explicit PerfTest(const std::string& filename):
        _resourceCount(1), _iterations(1)
    {
        _resourceList.push_back(filename);
    }    
    void test(bool async)
    {
        setup();
        std::cout<<" Test Started : .... \n";
        auto start = std::chrono::high_resolution_clock::now();
        benchmark(async);
        std::chrono::duration<double> secs = std::chrono::high_resolution_clock::now() - start;
        std::chrono::duration<double, std::milli> millisecs = secs;
        std::cout<< " Test Finished.\n";
        std::cout<< " \nPerformance Report: \n\n";
        std::cout<< " \t Resource Rendered per Frame : "<< _resourceCount <<"\n";
        std::cout<< " \t Render Buffer Size          : (100 X 100) \n";
        std::cout<< " \t Render Mode                 : "<< (async ? "Async" : "Sync")<<"\n";
        std::cout<< " \t Total Frames Rendered       : "<< _iterations<<"\n";
        std::cout<< " \t Total Render Time           : "<< secs.count()<<"sec\n";
        std::cout<< " \t Avrage Time per Resource    : "<< millisecs.count() / (_iterations * _resourceCount)<<"ms\n";
        std::cout<< " \t Avrage Time Per Frame       : "<< millisecs.count() / _iterations <<"ms\n";
        std::cout<< " \t FPS                         : "<< _iterations / secs.count() <<"fps\n\n";
    }
private:
    void setup()
    {
        for (auto i = 0u; i < _resourceCount; i++) {
            auto index = i % _resourceList.size();
            _renderers.push_back(std::make_unique<Renderer>(_resourceList[index]));
        }
    }

    void benchmark(bool async)
    {
        for (auto i = 0u; i < _iterations; i++) {
            if (async) {
                for (const auto &e : _renderers) e->renderAsync();
                for (const auto &e : _renderers) e->get();
            } else {
                for (const auto &e : _renderers) e->render();
            }
        }
    }

private:
    size_t  _resourceCount;
    size_t  _iterations;

    std::vector<std::string>                 _resourceList;
    std::vector<std::unique_ptr<Renderer>>   _renderers;
};

static int help()
{
    std::cout<<"\nUsage : ./perf [--sync] [-c] [resource count] [-i] [iteration count] [-f] [filename] \n";
    std::cout<<"\nExample : ./perf -c 50 -i 100 \n";
    std::cout<<"\n\t runs perf test for 100 iterations. renders 50 resource per iteration\n\n";
    return 0;
}
int
main(int argc, char ** argv)
{
    bool async = true;
    size_t resourceCount = 250;
    size_t iterations = 500;
    auto index = 0;
    std::string filename;

    while (index < argc) {
      const char* option = argv[index];
      index++;
      if (!strcmp(option,"--help") || !strcmp(option,"-h")) {
          return help();
      } else if (!strcmp(option,"--sync")) {
          async = false;
      } else if (!strcmp(option,"-c")) {
         resourceCount = (index < argc) ? atoi(argv[index]) : resourceCount;
         index++;
      } else if (!strcmp(option,"-i")) {
         iterations = (index < argc) ? atoi(argv[index]) : iterations;
         index++;
      } else if (!strcmp(option,"-f")) {
         filename = (index < argc) ? argv[index] : "";
         index++;
      }
   }

    
    PerfTest obj = filename.length() ? PerfTest(filename) : PerfTest(resourceCount, iterations);
    obj.test(async);
    return 0;
}
