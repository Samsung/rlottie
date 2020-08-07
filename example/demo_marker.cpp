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
#include <stdio.h>
#include <fstream>
#include <sstream>
using namespace std;

class DemoMarker
{
public:
   DemoMarker(EvasApp *app, std::string filePath) {
      view1.reset(new LottieView(app->evas()));
      view1->setFilePath(filePath.c_str());
      view1->setPos(0, 0);
      view1->setSize(400, 400);
      view1->show();
      view1->play();
      view1->loop(true);

      /* Play with marker */
      view2.reset(new LottieView(app->evas()));
      view2->setFilePath(filePath.c_str());
      view2->setPos(400, 0);
      view2->setSize(400, 400);
      view2->show();
      view2->play("second");
      view2->loop(true);

      /* Play marker to marker */
      view3.reset(new LottieView(app->evas()));
      view3->setFilePath(filePath.c_str());
      view3->setPos(800, 0);
      view3->setSize(400, 400);
      view3->show();
      view3->play("second", "third");
      view3->loop(true);
   }

private:
    std::unique_ptr<LottieView>  view1;
    std::unique_ptr<LottieView>  view2;
    std::unique_ptr<LottieView>  view3;
};

static void
onExitCb(void *data, void */*extra*/)
{
    DemoMarker *demo = (DemoMarker *)data;
    delete demo;
}

int
main(void)
{
   EvasApp *app = new EvasApp(1200, 400);
   app->setup();

   std::string filePath = DEMO_DIR;
   filePath +="marker.json";

   auto demo = new DemoMarker(app, filePath);

   app->addExitCb(onExitCb, demo);
   app->run();

   delete app;
   return 0;
}





