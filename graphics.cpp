#include "graphics.h"
#include <cmath>
#include <iostream>

using namespace Gdiplus;

void plot_radar(HDC hdc, const std::vector<Network>& networks, int width, int height, double scale, double sonarAngle) {
    if (hdc == NULL) {
        std::wcerr << L"Invalid HDC" << std::endl;
        return;
    }

    Graphics graphics(hdc);
    graphics.Clear(Color(50, 50, 50)); // Серый фон
    Pen pen(Color(255, 255, 0, 0)); // Красный цвет для точек
    Pen gridPen(Color(255, 0, 255, 0)); // Зеленый цвет для сетки
    Font font(L"Arial", 10);
    SolidBrush brush(Color(255, 255, 0, 0)); // Красный цвет для текста

    // Рисуем круглый радар с линиями
    int centerX = width / 2;
    int centerY = height / 2;
    int radius = static_cast<int>((std::min(centerX, centerY) - 10) * scale);

    for (int i = 1; i <= 5; ++i) {
        graphics.DrawEllipse(&gridPen, centerX - i * radius / 5, centerY - i * radius / 5, 2 * i * radius / 5, 2 * i * radius / 5);
    }

    for (int i = 0; i < 360; i += 30) {
        double angle = i * M_PI / 180;
        int x = static_cast<int>(centerX + radius * std::cos(angle));
        int y = static_cast<int>(centerY + radius * std::sin(angle));
        graphics.DrawLine(&gridPen, centerX, centerY, x, y);
    }

    // Отображаем вас в центре
    graphics.FillEllipse(&brush, centerX - 5, centerY - 5, 10, 10);
    graphics.DrawString(L"Я", -1, &font, PointF(centerX + 10, centerY), &brush);

    // Рисуем сонар
    Pen sonarPen(Color(255, 0, 255, 0), 2);
    int sonarX = static_cast<int>(centerX + radius * std::cos(sonarAngle));
    int sonarY = static_cast<int>(centerY + radius * std::sin(sonarAngle));
    graphics.DrawLine(&sonarPen, centerX, centerY, sonarX, sonarY);

    // Отображаем точки и текст
    for (const auto& network : networks) {
        double r = std::sqrt(network.X * network.X + network.Y * network.Y);
        double theta = std::atan2(network.Y, network.X);
        int x = static_cast<int>(centerX + (r / 100) * radius * std::cos(theta));
        int y = static_cast<int>(centerY + (r / 100) * radius * std::sin(theta));

        // Проверяем, чтобы текст не накладывался
        for (const auto& other : networks) {
            if (&network != &other) {
                double otherR = std::sqrt(other.X * other.X + other.Y * other.Y);
                double otherTheta = std::atan2(other.Y, other.X);
                int otherX = static_cast<int>(centerX + (otherR / 100) * radius * std::cos(otherTheta));
                int otherY = static_cast<int>(centerY + (otherR / 100) * radius * std::sin(otherTheta));
                if (std::abs(x - otherX) < 20 && std::abs(y - otherY) < 20) {
                    y += 20; // Смещаем текст вниз, если точки слишком близко
                }
            }
        }

        graphics.DrawEllipse(&pen, x - 2, y - 2, 4, 4);
        graphics.DrawString(network.SSID.c_str(), -1, &font, PointF(x, y), &brush);
    }
}