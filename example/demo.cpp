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

class Demo
{
public:
    Demo(EvasApp *app, std::string &filePath) {
        Demo1(app, filePath);
        Demo2(app, filePath);
        Demo3(app, filePath);
        Demo4(app, filePath);
        Demo5(app, filePath);
        Demo6(app, filePath);
        Demo7(app, filePath);
        Demo8(app, filePath);
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

    void Demo6(EvasApp *app, std::string &filePath) {
        /* Transform scale */
        view6.reset(new LottieView(app->evas()));
        view6->setFilePath(filePath.c_str());
        if (view6->player()) {
            view6->player()->setValue<rlottie::Property::TrScale>("Shape Layer 1.Ellipse 1",
                [](const rlottie::FrameInfo& info) {
                          return rlottie::Size(100 - info.curFrame(),
                                               50);
                 });
        }
        view6->setPos(1500, 0);
        view6->setSize(300, 300);
        view6->show();
        view6->play();
        view6->loop(true);
        view6->setRepeatMode(LottieView::RepeatMode::Reverse);
    }

    void Demo7(EvasApp *app, std::string &filePath) {
        /* Transform rotation */
        view7.reset(new LottieView(app->evas()));
        view7->setFilePath(filePath.c_str());
        if (view7->player()) {
            view7->player()->setValue<rlottie::Property::TrRotation>("Shape Layer 2.Shape 1",
                [](const rlottie::FrameInfo& info) {
                          return info.curFrame() * 20;
                 });
        }
        view7->setPos(1800, 0);
        view7->setSize(300, 300);
        view7->show();
        view7->play();
        view7->loop(true);
        view7->setRepeatMode(LottieView::RepeatMode::Reverse);
    }
    void Demo8(EvasApp *app, std::string &filePath) {
        /* Transform + color */
        view8.reset(new LottieView(app->evas()));
        view8->setFilePath(filePath.c_str());
        if (view8->player()) {
            view8->player()->setValue<rlottie::Property::TrRotation>("Shape Layer 1.Ellipse 1",
                [](const rlottie::FrameInfo& info) {
                          return info.curFrame() * 20;
                 });
            view8->player()->setValue<rlottie::Property::TrScale>("Shape Layer 1.Ellipse 1",
                [](const rlottie::FrameInfo& info) {
                          return rlottie::Size(50, 100 - info.curFrame());
                 });
            view8->player()->setValue<rlottie::Property::TrPosition>("Shape Layer 1.Ellipse 1",
                [](const rlottie::FrameInfo& info) {
                          return rlottie::Point(-20 + (double)info.curFrame()/2.0,
                                                -20 + (double)info.curFrame()/2.0);
                 });
            view8->player()->setValue<rlottie::Property::FillColor>("Shape Layer 1.Ellipse 1.Fill 1",
                [](const rlottie::FrameInfo& info) {
                     if (info.curFrame() < 60 )
                         return rlottie::Color(0, 0, 1);
                     else {
                         return rlottie::Color(1, 0, 0);
                     }
                 });
        }
        view8->setPos(2100, 0);
        view8->setSize(300, 300);
        view8->show();
        view8->play();
        view8->loop(true);
        view8->setRepeatMode(LottieView::RepeatMode::Reverse);
    }
private:
    std::unique_ptr<LottieView>  view1;
    std::unique_ptr<LottieView>  view2;
    std::unique_ptr<LottieView>  view3;
    std::unique_ptr<LottieView>  view4;
    std::unique_ptr<LottieView>  view5;
    std::unique_ptr<LottieView>  view6;
    std::unique_ptr<LottieView>  view7;
    std::unique_ptr<LottieView>  view8;
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
   EvasApp *app = new EvasApp(2400, 300);
   app->setup();

   std::string filePath = DEMO_DIR;
   filePath +="done.json";

    auto demo = new Demo(app, filePath);

    app->addExitCb(onExitCb, demo);
    app->run();

   delete app;
   return 0;
}





