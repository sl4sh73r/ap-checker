#define UNICODE
#define _UNICODE

#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <wlanapi.h>
#include <cmath>
#include <fcntl.h>
#include <io.h>
#include <gdiplus.h>
#include <algorithm>
#include <random>
#include <map>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "gdi32.lib")

using namespace Gdiplus;

const double FREQUENCY = 2.4; // Frequency in GHz

struct Network {
    std::wstring SSID;
    std::wstring BSSID;
    int Signal; // Signal strength in dBm
    double Distance; // Calculated distance
    double X; // X coordinate
    double Y; // Y coordinate
    bool isCoordinateSet = false; // Flag to check if coordinates are already set
};

std::wstring convert_ssid(const BYTE* ssid, DWORD length) {
    int requiredSize = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)ssid, length, NULL, 0);
    if (requiredSize > 0) {
        std::wstring wide_ssid(requiredSize, 0);
        MultiByteToWideChar(CP_UTF8, 0, (LPCCH)ssid, length, &wide_ssid[0], requiredSize);
        return wide_ssid;
    } else {
        std::wstring raw_ssid;
        for (DWORD i = 0; i < length; ++i) {
            wchar_t buffer[4];
            swprintf(buffer, 4, L"%02X", ssid[i]);
            raw_ssid += buffer;
        }
        return L"[RAW] " + raw_ssid;
    }
}

double calculate_distance(double rssi, double frequency) {
    const double RSSI_0 = -40;
    const double path_loss_exponent = 3.0;

    if (rssi > 0) {
        return -1;
    }

    double distance = std::pow(10, (RSSI_0 - rssi) / (10 * path_loss_exponent));
    return distance;
}

std::vector<Network> get_wifi_networks() {
    std::vector<Network> networks;
    HANDLE hClient = NULL;
    DWORD dwMaxClient = 2;
    DWORD dwCurVersion = 0;

    if (WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient) != ERROR_SUCCESS) {
        std::wcerr << L"Failed to open WLAN handle." << std::endl;
        return networks;
    }

    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    if (WlanEnumInterfaces(hClient, NULL, &pIfList) != ERROR_SUCCESS) {
        std::wcerr << L"Failed to enumerate WLAN interfaces." << std::endl;
        WlanCloseHandle(hClient, NULL);
        return networks;
    }

    if (pIfList != NULL) {
        for (int i = 0; i < (int)pIfList->dwNumberOfItems; i++) {
            PWLAN_INTERFACE_INFO pIfInfo = &pIfList->InterfaceInfo[i];

            // Выполняем сканирование перед получением списка сетей
            if (WlanScan(hClient, &pIfInfo->InterfaceGuid, NULL, NULL, NULL) != ERROR_SUCCESS) {
                std::wcerr << L"Failed to scan networks for interface " << i << std::endl;
                continue;
            }

            PWLAN_BSS_LIST pBssList = NULL;
            if (WlanGetNetworkBssList(hClient, &pIfInfo->InterfaceGuid, NULL, dot11_BSS_type_any, FALSE, NULL, &pBssList) == ERROR_SUCCESS) {
                if (pBssList != NULL) {
                    for (unsigned int j = 0; j < pBssList->dwNumberOfItems; j++) {
                        PWLAN_BSS_ENTRY pBssEntry = &pBssList->wlanBssEntries[j];

                        Network network;
                        network.SSID = convert_ssid(pBssEntry->dot11Ssid.ucSSID, pBssEntry->dot11Ssid.uSSIDLength);
                        network.BSSID = L"";
                        for (int k = 0; k < 6; k++) {
                            wchar_t buffer[3];
                            swprintf(buffer, 3, L"%02X", pBssEntry->dot11Bssid[k]);
                            network.BSSID += buffer;
                            if (k < 5) network.BSSID += L":";
                        }
                        network.Signal = pBssEntry->lRssi;
                        network.Distance = calculate_distance(network.Signal, FREQUENCY);
                        networks.push_back(network);
                    }
                    WlanFreeMemory(pBssList);
                }
            } else {
                std::wcerr << L"Failed to get BSS list for interface " << i << std::endl;
            }
        }
        WlanFreeMemory(pIfList);
    }

    WlanCloseHandle(hClient, NULL);
    return networks;
}

void calculate_coordinates(std::vector<Network>& networks, std::map<std::wstring, std::pair<double, double>>& savedCoordinates) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0, 2 * M_PI);

    for (auto& network : networks) {
        if (savedCoordinates.find(network.SSID) != savedCoordinates.end()) {
            network.X = savedCoordinates[network.SSID].first;
            network.Y = savedCoordinates[network.SSID].second;
        } else {
            double angle = dis(gen);
            network.X = network.Distance * std::cos(angle);
            network.Y = network.Distance * std::sin(angle);
            savedCoordinates[network.SSID] = {network.X, network.Y};
        }
    }
}

void smooth_coordinates(std::vector<Network>& networks, std::map<std::wstring, std::pair<double, double>>& previousCoordinates, double alpha = 0.2) {
    for (auto& network : networks) {
        if (previousCoordinates.find(network.SSID) != previousCoordinates.end()) {
            network.X = alpha * network.X + (1 - alpha) * previousCoordinates[network.SSID].first;
            network.Y = alpha * network.Y + (1 - alpha) * previousCoordinates[network.SSID].second;
        }
        previousCoordinates[network.SSID] = {network.X, network.Y};
    }
}

void correct_coordinates(std::vector<Network>& networks) {
    for (auto& network : networks) {
        if (network.SSID == L"OIS Airplane Crew") {
            network.X = 0.43;
            network.Y = -0.63;
        } else if (network.SSID.find(L"*Not-connectable") != std::wstring::npos) {
            network.X = 0.60;
            network.Y = -0.50;
        }
    }
}

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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static std::vector<Network> networks;
    static std::map<std::wstring, std::pair<double, double>> savedCoordinates;
    static std::map<std::wstring, std::pair<double, double>> previousCoordinates;
    static double scale = 1.0;
    static double sonarAngle = 0.0;
    switch (uMsg) {
        case WM_CREATE: {
            SetTimer(hwnd, 1, 2000, nullptr);
            SetTimer(hwnd, 2, 50, nullptr); // Таймер для сонара
        }
        break;

        case WM_TIMER: {
            if (wParam == 1) {
                networks = get_wifi_networks();
                calculate_coordinates(networks, savedCoordinates);
                smooth_coordinates(networks, previousCoordinates);
                correct_coordinates(networks);
            } else if (wParam == 2) {
                sonarAngle += 0.1; // Угол сонара
                if (sonarAngle >= 2 * M_PI) {
                    sonarAngle = 0.0;
                }
            }
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);
            plot_radar(hdc, networks, rect.right - rect.left, rect.bottom - rect.top, scale, sonarAngle);
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
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Status gdiplusStatus = GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    if (gdiplusStatus != Ok) {
        MessageBox(NULL, L"Failed to initialize GDI+.", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    const wchar_t CLASS_NAME[] = L"WiFiRadar";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassW(&wc)) {
        MessageBox(NULL, L"Failed to register window class.", L"Error", MB_OK | MB_ICONERROR);
        GdiplusShutdown(gdiplusToken);
        return -1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Wi-Fi Radar",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, hInstance, nullptr
    );

    if (hwnd == nullptr) {
        MessageBox(NULL, L"Failed to create window.", L"Error", MB_OK | MB_ICONERROR);
        GdiplusShutdown(gdiplusToken);
        return -1;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(gdiplusToken);

    return 0;
}

#ifdef UNICODE
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
#endif