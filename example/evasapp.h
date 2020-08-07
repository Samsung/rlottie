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

#ifndef EVASAPP_H
#define EVASAPP_H

#ifndef EFL_BETA_API_SUPPORT
#define EFL_BETA_API_SUPPORT
#endif

#ifndef EFL_EO_API_SUPPORT
#define EFL_EO_API_SUPPORT
#endif

#include <Eo.h>
#include <Efl.h>
#include <Evas.h>
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Ecore_Input.h>
#include<vector>
#include<string>


typedef void (*appCb)(void *userData, void *extra);
class EvasApp
{
public:
    EvasApp(int w, int h);
    void setup();
    void resize(int w, int h);
    int width() const{ return mw;}
    int height() const{ return mh;}
    void run();
    Ecore_Evas * ee() const{return mEcoreEvas;}
    Evas * evas() const {return mEvas;}
    void addExitCb(appCb exitcb, void *data) {mExitCb = exitcb; mExitData = data;}
    void addResizeCb(appCb resizecb, void *data) {mResizeCb = resizecb; mResizeData = data;}
    void addKeyCb(appCb keycb, void *data) {mKeyCb = keycb; mKeyData = data;}
    void addRenderPreCb(appCb renderPrecb, void *data) {mRenderPreCb = renderPrecb; mRenderPreData = data;}
    void addRenderPostCb(appCb renderPostcb, void *data) {mRenderPostCb = renderPostcb; mRenderPostData = data;}
    static std::vector<std::string> jsonFiles(const std::string &dir, bool recurse=false);
public:
    int           mw{0};
    int           mh{0};
    Ecore_Evas   *mEcoreEvas{nullptr};
    Evas         *mEvas{nullptr};
    Evas_Object  *mBackground{nullptr};
    appCb        mResizeCb{nullptr};
    void        *mResizeData{nullptr};
    appCb        mExitCb{nullptr};
    void        *mExitData{nullptr};
    appCb        mKeyCb{nullptr};
    void        *mKeyData{nullptr};
    appCb        mRenderPreCb{nullptr};
    void        *mRenderPreData{nullptr};
    appCb        mRenderPostCb{nullptr};
    void        *mRenderPostData{nullptr};
};
#endif //EVASAPP_H
