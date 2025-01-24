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
#include <algorithm> // Добавляем этот заголовочный файл

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
    std::vector<int> SignalHistory; // История сигналов для графика
};

struct GraphData {
    std::vector<int> SignalHistory;
    std::wstring SSID;
};

std::wstring convert_ssid(const BYTE* ssid, DWORD length) {
    bool is_ascii = true;
    for (DWORD i = 0; i < length; ++i) {
        if (ssid[i] < 0x20 || ssid[i] > 0x7E) {
            is_ascii = false;
            break;
        }
    }

    if (is_ascii) {
        return std::wstring(ssid, ssid + length);
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

double calculate_distance(double rssi, double frequency) {
    const double RSSI_0 = -40;
    const double path_loss_exponent = 3.0;

    if (rssi > 0) {
        return -1;
    }

    double distance = std::pow(10, (RSSI_0 - rssi) / (10 * path_loss_exponent));
    return distance;
}

void DrawGraph(HDC hdc, const std::vector<int>& data, int width, int height) {
    if (hdc == NULL) {
        std::wcerr << L"Invalid HDC" << std::endl;
        return;
    }

    if (data.empty()) {
        std::wcerr << L"Data vector is empty" << std::endl;
        return;
    }

    Graphics graphics(hdc);
    Pen pen(Color(255, 0, 255, 0));
    Pen gridPen(Color(255, 200, 200, 200));

    // Рисуем сетку
    for (int i = 0; i <= 10; ++i) {
        int y = i * height / 10;
        graphics.DrawLine(&gridPen, 0, y, width, y);
    }

    int max_value = *std::max_element(data.begin(), data.end());
    int min_value = *std::min_element(data.begin(), data.end());

    for (size_t i = 1; i < data.size(); ++i) {
        int x1 = (i - 1) * width / data.size();
        int y1;
        if (max_value != min_value) {
            y1 = height - (data[i - 1] - min_value) * height / (max_value - min_value);
        } else {
            y1 = height; // или другое значение по умолчанию
        }
        int x2 = i * width / data.size();
        int y2 = height - (data[i] - min_value) * height / (max_value - min_value);
        graphics.DrawLine(&pen, x1, y1, x2, y2);
    }
}

LRESULT CALLBACK GraphWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static GraphData* pGraphData;
    switch (uMsg) {
        case WM_CREATE: {
            pGraphData = reinterpret_cast<GraphData*>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams);
            if (pGraphData == nullptr) {
                std::wcerr << L"Failed to get graph data." << std::endl;
            }
            SetTimer(hwnd, 1, 2000, NULL); // Устанавливаем таймер для обновления графика
        }
        break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);
            if (pGraphData != nullptr) {
                DrawGraph(hdc, pGraphData->SignalHistory, rect.right - rect.left, rect.bottom - rect.top);
                // Отображаем название сети
                SetBkMode(hdc, TRANSPARENT);
                TextOut(hdc, 10, 10, pGraphData->SSID.c_str(), pGraphData->SSID.length());
            } else {
                std::wcerr << L"Graph data is null." << std::endl;
            }
            EndPaint(hwnd, &ps);
        }
        break;

        case WM_TIMER: {
            // Обновляем данные графика
            std::vector<Network> networks = get_wifi_networks();
            for (auto& network : networks) {
                if (network.SSID == pGraphData->SSID) {
                    pGraphData->SignalHistory.push_back(network.Signal);
                    if (pGraphData->SignalHistory.size() > 100) {
                        pGraphData->SignalHistory.erase(pGraphData->SignalHistory.begin());
                    }
                    InvalidateRect(hwnd, NULL, TRUE); // Перерисовываем окно
                    break;
                }
            }
        }
        break;

        case WM_DESTROY:
            KillTimer(hwnd, 1); // Удаляем таймер
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void ShowGraphPopup(HWND hwndParent, const Network& network) {
    const wchar_t CLASS_NAME[] = L"GraphWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = GraphWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassW(&wc)) {
        MessageBox(NULL, L"Failed to register graph window class.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    GraphData* graphData = new GraphData{ network.SignalHistory, network.SSID };

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        (L"Signal Strength Graph - " + network.SSID).c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        hwndParent, nullptr, GetModuleHandle(NULL), graphData
    );

    if (hwnd == nullptr) {
        MessageBox(NULL, L"Failed to create graph window.", L"Error", MB_OK | MB_ICONERROR);
        delete graphData;
        return;
    }

    ShowWindow(hwnd, SW_SHOW);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hListView;
    static HIMAGELIST hImageList;
    static std::vector<Network> networks;
    switch (uMsg) {
        case WM_CREATE: {
            INITCOMMONCONTROLSEX icex;
            icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
            icex.dwICC = ICC_LISTVIEW_CLASSES;
            if (!InitCommonControlsEx(&icex)) {
                MessageBox(hwnd, L"Failed to initialize common controls.", L"Error", MB_OK | MB_ICONERROR);
                return -1;
            }

            hListView = CreateWindowW(WC_LISTVIEWW, L"",
                                     WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                                     10, 10, 780, 480,
                                     hwnd, nullptr, nullptr, nullptr);

            if (!hListView) {
                MessageBox(hwnd, L"Failed to create list view.", L"Error", MB_OK | MB_ICONERROR);
                return -1;
            }

            hImageList = ImageList_Create(1, 1, ILC_COLOR32, 1, 1);
            ListView_SetImageList(hListView, hImageList, LVSIL_NORMAL);

            LVCOLUMNW lvColumn;
            lvColumn.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

            lvColumn.cx = 200;
            lvColumn.pszText = const_cast<LPWSTR>(L"SSID");
            ListView_InsertColumn(hListView, 0, &lvColumn);

            lvColumn.cx = 200;
            lvColumn.pszText = const_cast<LPWSTR>(L"BSSID");
            ListView_InsertColumn(hListView, 1, &lvColumn);

            lvColumn.cx = 100;
            lvColumn.pszText = const_cast<LPWSTR>(L"Signal (dBm)");
            ListView_InsertColumn(hListView, 2, &lvColumn);

            lvColumn.cx = 100;
            lvColumn.pszText = const_cast<LPWSTR>(L"Distance (m)");
            ListView_InsertColumn(hListView, 3, &lvColumn);

            SetTimer(hwnd, 1, 2000, nullptr);
        }
        break;

        case WM_SIZE: {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            SetWindowPos(hListView, NULL, 10, 10, rcClient.right - 20, rcClient.bottom - 20, SWP_NOZORDER);
        }
        break;

        case WM_TIMER: {
            ListView_DeleteAllItems(hListView);
            ImageList_RemoveAll(hImageList);

            networks = get_wifi_networks();

            if (networks.empty()) {
                std::wcerr << L"No networks found" << std::endl;
            } else {
                std::wcout << L"Found " << networks.size() << L" networks" << std::endl;
            }

            for (auto& network : networks) {
                LVITEMW lvItem;
                lvItem.mask = LVIF_TEXT;
                lvItem.iItem = ListView_GetItemCount(hListView);
                lvItem.iSubItem = 0;
                lvItem.pszText = const_cast<LPWSTR>(network.SSID.c_str());
                ListView_InsertItem(hListView, &lvItem);

                ListView_SetItemText(hListView, lvItem.iItem, 1, const_cast<LPWSTR>(network.BSSID.c_str()));

                std::wstring signal_str = std::to_wstring(network.Signal) + L" dBm";
                ListView_SetItemText(hListView, lvItem.iItem, 2, const_cast<LPWSTR>(signal_str.c_str()));

                double distance = calculate_distance(network.Signal, FREQUENCY);
                std::wstring distance_str = std::to_wstring(distance) + L" m";
                ListView_SetItemText(hListView, lvItem.iItem, 3, const_cast<LPWSTR>(distance_str.c_str()));

                // Обновляем историю сигналов
                network.SignalHistory.push_back(network.Signal);
                if (network.SignalHistory.size() > 100) {
                    network.SignalHistory.erase(network.SignalHistory.begin());
                }
            }

            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;

        case WM_NOTIFY: {
            if (((LPNMHDR)lParam)->hwndFrom == hListView && ((LPNMHDR)lParam)->code == NM_CLICK) {
                int iSelected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                if (iSelected != -1) {
                    ShowGraphPopup(hwnd, networks[iSelected]);
                }
            }
        }
        break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);
            EndPaint(hwnd, &ps);
        }
        break;

        case WM_DESTROY:
            ImageList_Destroy(hImageList);
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

    const wchar_t CLASS_NAME[] = L"WiFiMonitor";

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
        L"Wi-Fi Signal Strength Monitor",
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