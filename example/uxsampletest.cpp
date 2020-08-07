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

#include"evasapp.h"
#include "lottieview.h"
#include <memory>
#include<vector>
#include<string>

class UxSampleTest
{
public:
  UxSampleTest(EvasApp *app, bool renderMode) {
      mApp = app;
      mRenderMode = renderMode;
      mResourceList = EvasApp::jsonFiles(std::string(DEMO_DIR) + "UXSample_1920x1080/");
      mRepeatMode = LottieView::RepeatMode::Restart;
  }

  void showPrev() {
      if (mResourceList.empty()) return;
      mCurIndex--;
      if (mCurIndex < 0)
          mCurIndex = mResourceList.size() - 1;
      show();
  }

  void showNext() {
    if (mResourceList.empty()) return;

    mCurIndex++;
    if (mCurIndex >= int(mResourceList.size()))
        mCurIndex = 0;
    show();
  }

  void resize() {
      if (mView) {
          mView->setSize(mApp->width(), mApp->height());
      }
  }

private:
  void show() {
      mView = std::make_unique<LottieView>(mApp->evas(), Strategy::renderCAsync);
      mView->setFilePath(mResourceList[mCurIndex].c_str());
      mView->setPos(0, 0);
      mView->setSize(mApp->width(), mApp->height());
      mView->show();
      mView->play();
      mView->loop(true);
      mView->setRepeatMode(mRepeatMode);
  }

public:
  EvasApp                    *mApp;
  bool                        mRenderMode = false;
  int                         mCurIndex = -1;
  std::vector<std::string>    mResourceList;
  std::unique_ptr<LottieView> mView;
  LottieView::RepeatMode      mRepeatMode;
};

static void
onExitCb(void *data, void */*extra*/)
{
    UxSampleTest *view = (UxSampleTest *)data;
    delete view;
}

static void
onKeyCb(void *data, void *extra)
{
    UxSampleTest *view = (UxSampleTest *)data;
    char *keyname = (char *)extra;

    if (!strcmp(keyname, "Right") || !strcmp(keyname, "n")) {
        view->showNext();
    } else if (!strcmp(keyname, "Left") || !strcmp(keyname, "p")) {
        view->showPrev();
    } else if (!strcmp(keyname,"r")) {
        if (view->mRepeatMode == LottieView::RepeatMode::Restart) {
            view->mRepeatMode = LottieView::RepeatMode::Reverse;
        } else
            view->mRepeatMode = LottieView::RepeatMode::Restart;
        if (view->mView)
            view->mView->setRepeatMode(view->mRepeatMode);
    }
}

static void
onResizeCb(void *data, void */*extra*/)
{
    UxSampleTest *view = (UxSampleTest *)data;
    view->resize();
}

int
main(int argc, char **argv)
{
   EvasApp *app = new EvasApp(800, 800);
   app->setup();

   bool renderMode = true;
   if (argc > 1) {
       if (!strcmp(argv[1],"--disable-render"))
           renderMode = false;
   }
   UxSampleTest *view = new UxSampleTest(app, renderMode);
   view->showNext();

   app->addExitCb(onExitCb, view);
   app->addKeyCb(onKeyCb, view);
   app->addResizeCb(onResizeCb, view);

   app->run();
   delete app;
   return 0;
}
