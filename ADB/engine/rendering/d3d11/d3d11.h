#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

typedef struct renderer renderer;
renderer * D3D11Initialize  (HWND HWindow);