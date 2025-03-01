#define UNICODE
#define _UNICODE

#include <iostream>
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <gdiplus.h>
#include "window.h"

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    std::wcout << L"Starting wifi-checker..." << std::endl;

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

    std::wcout << L"Exiting wifi-checker..." << std::endl;
    return 0;
}

#ifdef UNICODE
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
#endif