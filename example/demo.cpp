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

class Demo
{
public:
    Demo(EvasApp *app, std::string &filePath) {
        Demo1(app, filePath);
        Demo2(app, filePath);
        Demo3(app, filePath);

    }
    void Demo1(EvasApp *app, std::string &filePath) {
        /* Fill Color */
        view.reset(new LottieView(app->evas()));
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
    }

    void Demo2(EvasApp *app, std::string &filePath) {
        /* Stroke Opacity */
        view1.reset(new LottieView(app->evas()));
        view1->setFilePath(filePath.c_str());
        if (view1->player()) {
            view1->player()->setValue<rlottie::Property::StrokeOpacity>("Shape Layer 2.Shape 1.Stroke 1",
                [](const rlottie::FrameInfo& info) {
                     if (info.curFrame() < 60 )
                         return 20;
                     else {
                         return 100;
                     }
                 });
        }
        view1->setPos(300, 0);
        view1->setSize(300, 300);
        view1->show();
        view1->play();
        view1->loop(true);
        view1->setRepeatMode(LottieView::RepeatMode::Reverse);
    }

    void Demo3(EvasApp *app, std::string &filePath) {
        /* Stroke Opacity */
        view2.reset(new LottieView(app->evas()));
        view2->setFilePath(filePath.c_str());
        if (view2->player()) {
            view2->player()->setValue<rlottie::Property::StrokeWidth>("**",
                    [](const rlottie::FrameInfo& info) {
                          if (info.curFrame() < 60 )
                              return 1.0;
                          else {
                               return 5.0;
                           }
                    });
        }
        view2->setPos(600, 0);
        view2->setSize(300, 300);
        view2->show();
        view2->play();
        view2->loop(true);
        view2->setRepeatMode(LottieView::RepeatMode::Reverse);
    }

private:
    std::unique_ptr<LottieView>  view;
    std::unique_ptr<LottieView>  view1;
    std::unique_ptr<LottieView>  view2;
};

static void
onExitCb(void *data, void */*extra*/)
{
    Demo *demo = (Demo *)data;
    delete demo;
}

int
main(void)
{
   EvasApp *app = new EvasApp(900, 300);
   app->setup();

   std::string filePath = DEMO_DIR;
   filePath +="done.json";

    auto demo = new Demo(app, filePath);

    app->addExitCb(onExitCb, demo);
    app->run();

   delete app;
   return 0;
}





