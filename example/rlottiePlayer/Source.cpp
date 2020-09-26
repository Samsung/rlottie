#include "rlottie.h"
using namespace rlottie;

std::unique_ptr<Animation> anim;
uint32_t *buffer;
size_t width, height;
size_t bytesPerLine;
uint32_t curColor = UINT32_MAX;

void initAnimation(size_t w, size_t h)
{
	width = w;
	height = h;
	bytesPerLine = width * sizeof(uint32_t);
	buffer = (uint32_t*)calloc(bytesPerLine * height, sizeof(uint32_t));
}

void setAnimation(char* path, size_t w, size_t h)
{
	anim = Animation::loadFromFile(path);
}

uint32_t* renderRLottieAnimation(uint32_t frameNum)
{
	static Surface surface = Surface(buffer, width, height, bytesPerLine);
	anim->renderSync(frameNum, surface);
	// background color
	for (int i = 0; i < height; i++)
		for (int j = 0; j < width; ++j)
		{
			uint32_t* v = buffer + i * width + j;
			if (*v == 0) *v = curColor;
		}
	return buffer;
}

void setAnimationColor(int r, int g, int b)
{
	curColor = ((255 << 16) * r) + ((255 << 8) * g) + 255 * b;
}

size_t getTotalFrame()
{
	return anim->totalFrame();
}

bool isAnimNULL()
{
	return anim == NULL;
}