#pragma once

#include <vector>
#include <string>
#include <map>
#include <windows.h> // Для BYTE и DWORD

struct Network {
    std::wstring SSID;
    std::wstring BSSID;
    int Signal; // Signal strength in dBm
    double Distance; // Calculated distance
    double X; // X coordinate
    double Y; // Y coordinate
    bool isCoordinateSet = false; // Flag to check if coordinates are already set
};

std::wstring convert_ssid(const BYTE* ssid, DWORD length);
double calculate_distance(double rssi, double frequency);
bool check_wifi_adapter();
std::vector<Network> get_wifi_networks();
void calculate_coordinates(std::vector<Network>& networks, std::map<std::wstring, std::pair<double, double>>& savedCoordinates);
void correct_coordinates(std::vector<Network>& networks, const std::map<std::wstring, std::pair<double, double>>& customCoordinates);
std::pair<double, double> triangulate_position(const std::pair<double, double>& A, const std::pair<double, double>& B, double alpha, double beta);