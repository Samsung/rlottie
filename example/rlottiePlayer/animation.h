#pragma once
#include <stdint.h>

void      setAnimation(char* path, size_t w, size_t h);
void      initAnimation(size_t w, size_t h);
uint32_t* renderRLottieAnimation(uint32_t frameNum);
size_t    getTotalFrame();
bool      isAnimNULL();
void      setAnimationColor(int r, int g, int b);
void      freeAnimation();