#pragma once

#include "resource.h"
#include <Commdlg.h>                    // OPENFILENAME
#include "atlconv.h"                    // String cast. (ex. LPWSTR <-> LPSTR)
#include <gdiplus.h>
#include <CommCtrl.h>                    // slider handle
#include <stdint.h>

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

void setAnimation(char* path, size_t w, size_t h);
void initAnimation(size_t w, size_t h);
uint32_t* renderRLottieAnimation(uint32_t frameNum);
size_t getTotalFrame();
bool isAnimNULL();
void setAnimationColor(int r, int g, int b);
void freeAnimation();

typedef struct RlottieBitmap
{
	Gdiplus::Bitmap* image = NULL;
	int x = 0;
	int y = 0;
	unsigned int width = 0;
	unsigned int height = 0;
}RlottieBitmap;