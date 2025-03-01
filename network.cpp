#include "network.h"
#include <windows.h>
#include <wlanapi.h>
#include <cmath>
#include <iostream>
#include <random> // Для std::random_device, std::mt19937 и std::uniform_real_distribution

#pragma comment(lib, "wlanapi.lib")

const double FREQUENCY = 2.4; // Frequency in GHz

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

bool check_wifi_adapter() {
    HANDLE hClient = NULL;
    DWORD dwMaxClient = 2;
    DWORD dwCurVersion = 0;

    if (WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient) != ERROR_SUCCESS) {
        std::wcerr << L"Failed to open WLAN handle." << std::endl;
        return false;
    }

    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    if (WlanEnumInterfaces(hClient, NULL, &pIfList) != ERROR_SUCCESS) {
        std::wcerr << L"Failed to enumerate WLAN interfaces." << std::endl;
        WlanCloseHandle(hClient, NULL);
        return false;
    }

    bool hasAdapter = pIfList->dwNumberOfItems > 0;
    WlanFreeMemory(pIfList);
    WlanCloseHandle(hClient, NULL);
    return hasAdapter;
}

std::vector<Network> get_wifi_networks() {
    std::vector<Network> networks;
    if (!check_wifi_adapter()) {
        return networks;
    }

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

void correct_coordinates(std::vector<Network>& networks, const std::map<std::wstring, std::pair<double, double>>& customCoordinates) {
    for (auto& network : networks) {
        auto it = customCoordinates.find(network.SSID);
        if (it != customCoordinates.end()) {
            network.X = it->second.first;
            network.Y = it->second.second;
        }
    }
}

std::pair<double, double> triangulate_position(const std::pair<double, double>& A, const std::pair<double, double>& B, double alpha, double beta) {
    double xA = A.first;
    double yA = A.second;
    double xB = B.first;
    double yB = B.second;

    double dAB = std::sqrt((xB - xA) * (xB - xA) + (yB - yA) * (yB - yA));
    double gamma = M_PI - alpha - beta;

    double dAC = dAB * std::sin(beta) / std::sin(gamma);
    double dBC = dAB * std::sin(alpha) / std::sin(gamma);

    double xC = xA + dAC * std::cos(alpha);
    double yC = yA + dAC * std::sin(alpha);

    return {xC, yC};
}