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
using namespace std;

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
  LottieViewTest(EvasApp *app, Strategy st) {
      mStrategy = st;
      mApp = app;
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

public:
  EvasApp     *mApp;
  Strategy     mStrategy;
  std::vector<std::unique_ptr<LottieView>>   mViews;
};

static void
onExitCb(void *data, void */*extra*/)
{
    LottieViewTest *view = (LottieViewTest *)data;
    delete view;
}

int
main(int argc, char **argv)
{
    if (argc > 1) {
        if (!strcmp(argv[1],"--help") || !strcmp(argv[1],"-h")) {
            printf("Usage ./lottieviewTest 1 \n");
            printf("\t 0  - Test Lottie SYNC Renderer with CPP API\n");
            printf("\t 1  - Test Lottie ASYNC Renderer with CPP API\n");
            printf("\t 2  - Test Lottie SYNC Renderer with C API\n");
            printf("\t 3  - Test Lottie ASYNC Renderer with C API\n");
            printf("\t 4  - Test Lottie Tree Api using Efl VG Render\n");
            printf("\t Default is ./lottieviewTest 1 \n");
            return 0;
        }
    } else {
        printf("Run ./lottieviewTest -h  for more option\n");
    }

   Strategy st = Strategy::renderCppAsync;
   if (argc > 1) {
       int option = atoi(argv[1]);
       st = static_cast<Strategy>(option);
   }

   EvasApp *app = new EvasApp(800, 800);
   app->setup();

   LottieViewTest *view = new LottieViewTest(app, st);
   view->show(250);

   app->addExitCb(onExitCb, view);

   app->run();
   delete app;
   return 0;
}





