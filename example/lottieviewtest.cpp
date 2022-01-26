/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "evasapp.h"
#include "lottieview.h"
#include<iostream>
#include <dirent.h>
#include <stdio.h>
#include <chrono>
using namespace std;

static Eina_Bool onTestDone(void *data);
/*
 * To check the frame rate with rendermode off run
 * ECORE_EVAS_FPS_DEBUG=1 ./lottieviewTest --disable-render
 *
 * To check the frame rate with  render backend
 * ECORE_EVAS_FPS_DEBUG=1 ./lottieviewTest
 *
 */

class LottieViewTest
{
public:
  LottieViewTest(EvasApp *app, Strategy st, double timeout) {
      mStartTime = std::chrono::high_resolution_clock::now();
      mStrategy = st;
      mApp = app;

      if (timeout > 0) {
        ecore_timer_add(timeout, onTestDone, mApp);
      }
      // work around for 60fps
      ecore_animator_frametime_set(1.0f/120.0f);
  }

  void show(const char * filepath) {
        std::unique_ptr<LottieView> view(new LottieView(mApp->evas(), mStrategy));
        view->setFilePath(filepath);
        view->setPos(3, 3);
        int vw = mApp->width() - 6;
        view->setSize(vw, vw);
        view->show();
        view->play();
        view->loop(true);
        mViews.push_back(std::move(view));
  }


  void show(int numberOfImage) {
    auto resource = EvasApp::jsonFiles(std::string(DEMO_DIR));

    if (resource.empty()) return;

    int count = numberOfImage;
    int colums = (int) ceil(sqrt(count));
    int offset = 3;
    int vw = (mApp->width() - (offset * colums))/colums;
    int vh = vw;
    int posx = offset;
    int posy = offset;
    int resourceSize = resource.size();
    for (int i = 0 ; i < numberOfImage; i++) {
        int index = i % resourceSize;
        std::unique_ptr<LottieView> view(new LottieView(mApp->evas(), mStrategy));
        view->setFilePath(resource[index].c_str());
        view->setPos(posx, posy);
        view->setSize(vw, vh);
        view->show();
        view->play();
        view->loop(true);
        //view->setRepeatMode(LottieView::RepeatMode::Reverse);
        posx += vw+offset;
        if ((mApp->width() - posx) < vw) {
          posx = offset;
          posy = posy + vh + offset;
        }
        mViews.push_back(std::move(view));
    }
  }

    ~LottieViewTest() {
      const auto frames = mViews.empty() ? 0 : mViews[0]->renderCount();
      const double secs = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - mStartTime).count();
      std::cout<<"\tTestTime : "<< secs<<" sec \n\tTotalFrames : "<<frames<<"\n\tFramesPerSecond : "<< frames / secs <<" fps\n";
    }

  static int help() {
            printf("Usage ./lottieviewTest [-s] [strategy] [-t] [timeout] [-c] [count] [-f] path\n");
            printf("\n \t-t : timeout duration in seconds\n");
            printf("\n \t-c : number of resource in the grid\n");
            printf("\n \t-f : File to play\n");
            printf("\n \t-s : Rendering Strategy\n");
            printf("\t\t 0  - Test Lottie SYNC Renderer with CPP API\n");
            printf("\t\t 1  - Test Lottie ASYNC Renderer with CPP API\n");
            printf("\t\t 2  - Test Lottie SYNC Renderer with C API\n");
            printf("\t\t 3  - Test Lottie ASYNC Renderer with C API\n");
            printf("\t\t 4  - Test Lottie Tree Api using Efl VG Render\n");
            printf(" Default : ./lottieviewTest -s 1 \n");
            return 0;
  }
public:
  EvasApp     *mApp;
  Strategy     mStrategy;
  std::vector<std::unique_ptr<LottieView>>   mViews;
  std::chrono::high_resolution_clock::time_point  mStartTime;
};

static void
onExitCb(void *data, void */*extra*/)
{
    LottieViewTest *view = (LottieViewTest *)data;
    delete view;
}


static Eina_Bool
onTestDone(void *data)
{
    EvasApp *app = (EvasApp *)data;
    app->exit();
    return ECORE_CALLBACK_CANCEL;
}

int
main(int argc, char **argv)
{
    Strategy st = Strategy::renderCppAsync;
    auto index = 0;
    double timeOut = 0;
    size_t itemCount = 250;
    std::string filePath;
    while (index < argc) {
      const char* option = argv[index];
      index++;
      if (!strcmp(option,"--help") || !strcmp(option,"-h")) {
          return LottieViewTest::help();
      } else if (!strcmp(option,"-s")) {
         st = (index < argc) ? static_cast<Strategy>(atoi(argv[index])) : Strategy::renderCppAsync;
         index++;
      } else if (!strcmp(option,"-t")) {
         timeOut = (index < argc) ? atoi(argv[index]) : 10;
         index++;
      } else if (!strcmp(option,"-c")) {
         itemCount = (index < argc) ? atoi(argv[index]) : 10;
         index++;
      } else if (!strcmp(option,"-f")) {
         filePath = argv[index];
         index++;
      }
    }

   EvasApp *app = new EvasApp(800, 800);
   app->setup();

   LottieViewTest *view = new LottieViewTest(app, st, timeOut);
   if (filePath.length()) view->show(filePath.c_str());
   else view->show(itemCount);

   app->addExitCb(onExitCb, view);

   app->run();
   delete app;
   return 0;
}





