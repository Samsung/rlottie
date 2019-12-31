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
        Demo4(app, filePath);
        Demo5(app, filePath);
    }
    void Demo1(EvasApp *app, std::string &filePath) {
        /* Fill Color */
        view1.reset(new LottieView(app->evas()));
        view1->setFilePath(filePath.c_str());
        if (view1->player()) {
            view1->player()->setValue<rlottie::Property::FillColor>("Shape Layer 1.Ellipse 1.Fill 1",
                [](const rlottie::FrameInfo& info) {
                     if (info.curFrame() < 60 )
                         return rlottie::Color(0, 0, 1);
                     else {
                         return rlottie::Color(1, 0, 0);
                     }
                 });
        }
        view1->setPos(0, 0);
        view1->setSize(300, 300);
        view1->show();
        view1->play();
        view1->loop(true);
        view1->setRepeatMode(LottieView::RepeatMode::Reverse);
    }

    void Demo2(EvasApp *app, std::string &filePath) {
        /* Stroke Opacity */
        view2.reset(new LottieView(app->evas()));
        view2->setFilePath(filePath.c_str());
        if (view2->player()) {
            view2->player()->setValue<rlottie::Property::StrokeOpacity>("Shape Layer 2.Shape 1.Stroke 1",
                [](const rlottie::FrameInfo& info) {
                     if (info.curFrame() < 60 )
                         return 20;
                     else {
                         return 100;
                     }
                 });
        }
        view2->setPos(300, 0);
        view2->setSize(300, 300);
        view2->show();
        view2->play();
        view2->loop(true);
        view2->setRepeatMode(LottieView::RepeatMode::Reverse);
    }

    void Demo3(EvasApp *app, std::string &filePath) {
        /* Stroke Opacity */
        view3.reset(new LottieView(app->evas()));
        view3->setFilePath(filePath.c_str());
        if (view3->player()) {
            view3->player()->setValue<rlottie::Property::StrokeWidth>("**",
                    [](const rlottie::FrameInfo& info) {
                          if (info.curFrame() < 60 )
                              return 1.0;
                          else {
                               return 5.0;
                           }
                    });
        }
        view3->setPos(600, 0);
        view3->setSize(300, 300);
        view3->show();
        view3->play();
        view3->loop(true);
        view3->setRepeatMode(LottieView::RepeatMode::Reverse);
    }

    void Demo4(EvasApp *app, std::string &filePath) {
        /* Transform position */
        view4.reset(new LottieView(app->evas()));
        view4->setFilePath(filePath.c_str());
        if (view4->player()) {
            view4->player()->setValue<rlottie::Property::TrPosition>("Shape Layer 1.Ellipse 1",
                [](const rlottie::FrameInfo& info) {
                          return rlottie::Point(-20 + (double)info.curFrame()/2.0,
                                                -20 + (double)info.curFrame()/2.0);
                 });
        }
        view4->setPos(900, 0);
        view4->setSize(300, 300);
        view4->show();
        view4->play();
        view4->loop(true);
        view4->setRepeatMode(LottieView::RepeatMode::Reverse);
    }

    void Demo5(EvasApp *app, std::string &filePath) {
        /* Transform position */
        view5.reset(new LottieView(app->evas()));
        view5->setFilePath(filePath.c_str());
        if (view5->player()) {
            view5->player()->setValue<rlottie::Property::TrPosition>("Shape Layer 2.Shape 1",
                [](const rlottie::FrameInfo& info) {
                          return rlottie::Point(-20 + (double)info.curFrame()/2.0,
                                                -20 + (double)info.curFrame()/2.0);
                 });
        }
        view5->setPos(1200, 0);
        view5->setSize(300, 300);
        view5->show();
        view5->play();
        view5->loop(true);
        view5->setRepeatMode(LottieView::RepeatMode::Reverse);
    }
private:
    std::unique_ptr<LottieView>  view1;
    std::unique_ptr<LottieView>  view2;
    std::unique_ptr<LottieView>  view3;
    std::unique_ptr<LottieView>  view4;
    std::unique_ptr<LottieView>  view5;
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
   EvasApp *app = new EvasApp(1500, 300);
   app->setup();

   std::string filePath = DEMO_DIR;
   filePath +="done.json";

    auto demo = new Demo(app, filePath);

    app->addExitCb(onExitCb, demo);
    app->run();

   delete app;
   return 0;
}





