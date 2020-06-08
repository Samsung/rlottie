/* 
 * Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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





