#include "window.h"
#include "network.h"
#include "graphics.h"
#include <map>
#include <cmath>
#include <iostream> // Для std::wcout и std::endl
#include <string>
#include <windows.h>
#include <commctrl.h> // Для элементов управления

#pragma comment(lib, "comctl32.lib")

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static std::vector<Network> networks;
    static std::map<std::wstring, std::pair<double, double>> savedCoordinates;
    static std::map<std::wstring, std::pair<double, double>> customCoordinates;
    static double scale = 1.0;
    static double sonarAngle = 0.0;

    // Идентификаторы элементов управления
    #define IDC_LISTBOX 101
    #define IDC_EDIT_X 102
    #define IDC_EDIT_Y 103
    #define IDC_BUTTON_SAVE 104
    #define IDC_BUTTON_CALCULATE 105

    static HWND hListBox, hEditX, hEditY, hButtonSave, hButtonCalculate;
    static HBRUSH hBrushBackground;

    switch (uMsg) {
        case WM_CREATE: {
            SetTimer(hwnd, 1, 2000, nullptr);
            SetTimer(hwnd, 2, 50, nullptr); // Таймер для сонара

            // Создание кисти для фона
            hBrushBackground = CreateSolidBrush(RGB(240, 240, 240));

            // Создание списка точек доступа
            hListBox = CreateWindowW(L"LISTBOX", NULL,
                WS_CHILD | WS_VISIBLE | LBS_STANDARD | WS_VSCROLL | WS_BORDER,
                10, 10, 200, 100,
                hwnd, (HMENU)IDC_LISTBOX, nullptr, nullptr);

            // Создание поля ввода для координаты X
            hEditX = CreateWindowW(L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
                220, 10, 100, 20,
                hwnd, (HMENU)IDC_EDIT_X, nullptr, nullptr);

            // Создание поля ввода для координаты Y
            hEditY = CreateWindowW(L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
                330, 10, 100, 20,
                hwnd, (HMENU)IDC_EDIT_Y, nullptr, nullptr);

            // Создание кнопки для сохранения координат
            hButtonSave = CreateWindowW(L"BUTTON", L"Save",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                440, 10, 80, 20,
                hwnd, (HMENU)IDC_BUTTON_SAVE, nullptr, nullptr);

            // Создание кнопки для расчета азимута
            hButtonCalculate = CreateWindowW(L"BUTTON", L"Calculate",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                530, 10, 80, 20,
                hwnd, (HMENU)IDC_BUTTON_CALCULATE, nullptr, nullptr);

            break;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(0, 0, 0)); // Цвет текста
            SetBkColor(hdcStatic, RGB(240, 240, 240)); // Цвет фона
            return (INT_PTR)hBrushBackground;
        }
        case WM_ERASEBKGND: {
            return 1; // Отключает автоматическое стирание фона
        }
        case WM_TIMER: {
            if (wParam == 1) {
                networks = get_wifi_networks();
                SendMessage(hListBox, LB_RESETCONTENT, 0, 0);
                for (const auto& network : networks) {
                    SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)network.SSID.c_str());
                }
            } else if (wParam == 2) {
                sonarAngle += 0.1; // Угол сонара
                if (sonarAngle >= 2 * M_PI) {
                    sonarAngle = 0.0;
                }
            }
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;

        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_BUTTON_SAVE) {
                int index = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR) {
                    wchar_t bufferX[10], bufferY[10];
                    GetWindowTextW(hEditX, bufferX, 10);
                    GetWindowTextW(hEditY, bufferY, 10);

                    double x = _wtof(bufferX);
                    double y = _wtof(bufferY);

                    customCoordinates[networks[index].SSID] = {x, y};

                    MessageBoxW(hwnd, L"Coordinates saved!", L"Info", MB_OK);
                }
            } else if (LOWORD(wParam) == IDC_BUTTON_CALCULATE) {
                if (customCoordinates.size() >= 2) {
                    auto it = customCoordinates.begin();
                    auto pointA = it->second;
                    auto pointB = (++it)->second;

                    // Пример использования триангуляции
                    double alpha = M_PI / 4; // Угол в радианах
                    double beta = M_PI / 4; // Угол в радианах
                    auto position = triangulate_position(pointA, pointB, alpha, beta);
                    std::wcout << L"Triangulated position: (" << position.first << L", " << position.second << L")" << std::endl;
                } else {
                    MessageBoxW(hwnd, L"Please save coordinates for at least two access points.", L"Error", MB_OK);
                }
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbmMem = CreateCompatibleBitmap(hdc, ps.rcPaint.right, ps.rcPaint.bottom);
            HGDIOBJ hOld = SelectObject(hdcMem, hbmMem);

            FillRect(hdcMem, &ps.rcPaint, hBrushBackground); // Фон без мерцания
            plot_radar(hdcMem, networks, ps.rcPaint.right, ps.rcPaint.bottom, scale, sonarAngle);
            
            BitBlt(hdc, 0, 0, ps.rcPaint.right, ps.rcPaint.bottom, hdcMem, 0, 0, SRCCOPY);

            SelectObject(hdcMem, hOld);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);

            EndPaint(hwnd, &ps);
        }
        break;

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (delta > 0) {
                scale *= 1.1;
            } else {
                scale /= 1.1;
            }
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            KillTimer(hwnd, 2);
            DeleteObject(hBrushBackground);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}