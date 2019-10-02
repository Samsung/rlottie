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

static void
onExitCb(void *data, void */*extra*/)
{
    LottieView *view = (LottieView *)data;
    delete view;
}

int
main(void)
{
   EvasApp *app = new EvasApp(900, 300);
   app->setup();

   std::string filePath = DEMO_DIR;
   filePath +="done.json";

   /* Fill Color */
   LottieView *view = new LottieView(app->evas());
   view->setFilePath(filePath.c_str());
   if (view->player()) {
       view->player()->setValue<rlottie::Property::FillColor>("Shape Layer 1.Ellipse 1.Fill 1",
           [](const rlottie::FrameInfo& info) {
                if (info.curFrame() < 60 )
                    return rlottie::Color(0, 0, 1);
                else {
                    return rlottie::Color(1, 0, 0);
                }
            });
   }
   view->setPos(0, 0);
   view->setSize(300, 300);
   view->show();
   view->play();
   view->loop(true);
   view->setRepeatMode(LottieView::RepeatMode::Reverse);


   /* Stroke Opacity */
   view = new LottieView(app->evas());
   view->setFilePath(filePath.c_str());
   if (view->player()) {
       view->player()->setValue<rlottie::Property::StrokeOpacity>("Shape Layer 2.Shape 1.Stroke 1",
           [](const rlottie::FrameInfo& info) {
                if (info.curFrame() < 60 )
                    return 20;
                else {
                    return 100;
                }
            });
   }
   view->setPos(300, 0);
   view->setSize(300, 300);
   view->show();
   view->play();
   view->loop(true);
   view->setRepeatMode(LottieView::RepeatMode::Reverse);

   /* Stroke Width and Globstar path (All Content) */
   view = new LottieView(app->evas());
   view->setFilePath(filePath.c_str());
   if (view->player()) {
       view->player()->setValue<rlottie::Property::StrokeWidth>("**",
           [](const rlottie::FrameInfo& info) {
                if (info.curFrame() < 60 )
                    return 1.0;
                else {
                    return 5.0;
                }
            });
   }
   view->setPos(600, 0);
   view->setSize(300, 300);
   view->show();
   view->play();
   view->loop(true);
   view->setRepeatMode(LottieView::RepeatMode::Reverse);
   app->addExitCb(onExitCb, view);
   app->run();

   delete app;
   return 0;
}





