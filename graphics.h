#pragma once

#include <vector>
#include <windows.h>
#include <gdiplus.h>
#include "network.h"

void plot_radar(HDC hdc, const std::vector<Network>& networks, int width, int height, double scale, double sonarAngle);