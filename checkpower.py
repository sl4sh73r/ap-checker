import subprocess
import time
import math
import tkinter as tk
from tkinter import ttk

# Известные значения для калибровки
KNOWN_DISTANCE = 1.0  # Известное расстояние в метрах
KNOWN_RSSI = -50  # Известный уровень сигнала в дБм на известном расстоянии

def get_wifi_networks_info():
    try:
        result = subprocess.run(['netsh', 'wlan', 'show', 'networks', 'mode=bssid'], capture_output=True, text=True, check=True, encoding='cp866', timeout=10)
        return result.stdout
    except subprocess.TimeoutExpired:
        return "Command timed out"
    except subprocess.CalledProcessError as e:
        return f"An error occurred: {e}\nOutput: {e.output}"
    except Exception as e:
        return f"An unexpected error occurred: {e}"

def parse_signal_strength(info):
    networks = []
    current_network = {}
    for line in info.splitlines():
        if "SSID" in line and "BSSID" not in line:
            if current_network:
                networks.append(current_network)
                current_network = {}
            current_network['SSID'] = line.split(":")[1].strip()
        elif "BSSID" in line:
            current_network['BSSID'] = line.split(":")[1].strip()
        elif "Сигнал" in line:
            signal_percent = line.split(":")[1].strip().replace('%', '')
            current_network['Signal'] = convert_signal_to_dbm(int(signal_percent))
    if current_network:
        networks.append(current_network)
    return networks

def convert_signal_to_dbm(signal_percent):
    # Примерная конвертация из процентов в дБм
    return -100 + (signal_percent / 2)

def calculate_distance(signal, known_distance=KNOWN_DISTANCE, known_rssi=KNOWN_RSSI, n=2.7):
    try:
        rssi = int(signal)
        if rssi > 0:
            rssi = -rssi  # Уровень сигнала должен быть отрицательным
        # Используем калибровку на основе известного расстояния и уровня сигнала
        return known_distance * (10 ** ((known_rssi - rssi) / (10 * n)))
    except ValueError:
        return 'Unknown'

def update_networks(tree):
    for i in tree.get_children():
        tree.delete(i)
    
    info = get_wifi_networks_info()
    if "Command timed out" in info or "An error occurred" in info or "An unexpected error occurred" in info:
        print(info)
    else:
        networks = parse_signal_strength(info)
        for network in networks:
            ssid = network.get('SSID', 'Unknown')
            bssid = network.get('BSSID', 'Unknown')
            signal = network.get('Signal', 'Unknown')
            distance = calculate_distance(signal) if signal != 'Unknown' else 'Unknown'
            tree.insert("", "end", values=(ssid, bssid, f"{signal} dBm", f"{distance:.2f} meters"))
    
    tree.after(10000, update_networks, tree)  # Обновление каждые 10 секунд

def main():
    root = tk.Tk()
    root.title("Wi-Fi Signal Strength Monitor")

    tree = ttk.Treeview(root, columns=("SSID", "BSSID", "Signal", "Distance"), show="headings")
    tree.heading("SSID", text="SSID")
    tree.heading("BSSID", text="BSSID")
    tree.heading("Signal", text="Signal")
    tree.heading("Distance", text="Distance")
    tree.pack(fill=tk.BOTH, expand=True)

    update_networks(tree)

    root.mainloop()

if __name__ == "__main__":
    main()