#pragma once

#include <vector>
#include <string>
#include <map>
#include <windows.h>
#include <wlanapi.h>

struct Network {
    std::wstring SSID;
    std::wstring BSSID;
    double Signal;
    double Distance;
    double X;
    double Y;
};

std::wstring convert_ssid(const BYTE* ssid, DWORD length);
double calculate_distance(double rssi, double frequency);
bool check_wifi_adapter();
std::vector<WLAN_INTERFACE_INFO> get_wifi_adapters();
std::vector<Network> get_wifi_networks(const GUID& adapterGuid);

void calculate_coordinates(std::vector<Network>& networks, std::map<std::wstring, std::pair<double, double>>& savedCoordinates, std::map<std::wstring, double>& savedAngles);
void correct_coordinates(std::vector<Network>& networks, const std::map<std::wstring, std::pair<double, double>>& customCoordinates);
std::pair<double, double> triangulate_position(const std::pair<double, double>& A, const std::pair<double, double>& B, double alpha, double beta);