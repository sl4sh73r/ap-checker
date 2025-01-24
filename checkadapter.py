import subprocess

def get_wifi_adapter_info():
    try:
        result = subprocess.run(['netsh', 'wlan', 'show', 'drivers'], capture_output=True, text=True, check=True, encoding='cp866')
        return result.stdout
    except subprocess.CalledProcessError as e:
        return f"An error occurred: {e}"

def check_rssi_support(info):
    return "RSSI" in info

def check_signal_support(info):
    return "Signal" in info or "Уровень сигнала" in info

def main():
    info = get_wifi_adapter_info()
    print(info)
    if check_rssi_support(info):
        print("RSSI поддерживается")
    elif check_signal_support(info):
        print("Поддерживается уровень сигнала")
    else:
        print("RSSI и уровень сигнала не поддерживаются")

if __name__ == "__main__":
    main()