#pragma once

#include "resource.h"
#include "animation.h"
#include <Commdlg.h>                    // OPENFILENAME
#include "atlconv.h"                    // String cast. (ex. LPWSTR <-> LPSTR)
#include <gdiplus.h>
#include <CommCtrl.h>                    // slider handle

// interval
#define UI_INTERVAL 20

// length
#define WND_WIDTH 1000
#define WND_HEIGHT 800
#define BMP_MAX_LEN	500
#define BTN_WIDTH 100
#define BTN_HEIGHT 30
#define TEXT_HEIGHT 20
#define SLIDER_HEIGHT 25
#define RDOBTN_WIDTH 60
#define RDOBTN_HEIGHT 20
#define RESIZE_LENGTH 10

typedef struct RlottieBitmap
{
	Gdiplus::Bitmap* image = NULL;
	int x = 0;
	int y = 0;
	unsigned int width = 0;
	unsigned int height = 0;
}RlottieBitmap;