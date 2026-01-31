# 🧅 OnionController - ESP32 BLE HID Firmware

Transform your ESP32 into a wireless, touch-sensitive keyboard controller using the power of BLE (Bluetooth Low Energy) and... onions! This firmware allows you to use up to 16 capacitive touch sensors (or any conductive objects like vegetables) as custom HID keyboard inputs.

## 🚀 Features

- **BLE HID Support**: Works as a standard Bluetooth keyboard. No extra drivers needed on Windows/macOS/Linux.
- **16-Channel Support**: Utilizes an analog multiplexer (CD74HC4067) to expand touch capabilities.
- **Real-time Configuration**: Adjust sensitivity (thresholds) and key mappings on the fly via a dedicated PC application.
- **Fast Response**: Optimized task scheduling (FreeRTOS) and ADC sampling for low-latency performance.
- **Non-Volatile Storage (NVS)**: Your custom key mappings and thresholds are saved permanently on the ESP32 memory.

## 🛠 Hardware Requirements

- **ESP32** (S3, C3, or Classic with BLE support).
- **CD74HC4067** 16-channel analog multiplexer.
- Conductive pads (or onions!) connected to the multiplexer inputs.
- **Status LED** for connection feedback.

## 💻 Tech Stack

- **Framework**: ESP-IDF
- **BLE Stack**: NimBLE (lightweight and efficient)
- **RTOS**: FreeRTOS for multi-threaded task management

## 📡 Protocol & Communication

The firmware communicates with the [OnionConfigurator PC App](https://github.com/AdrianMatenka/OnionConfigurator-PC) via Serial/UART:
- `CONNECT` / `DISCONNECT`: Handshake for telemetry.
- `SET:ch,thr,key`: Update sensor parameters.
- `RAW:v0,v1...`: Real-time ADC data streaming.

## 🔧 Installation & Build

1. Setup ESP-IDF environment.
2. Clone this repository:
   ```bash
   git clone [https://github.com/AdrianMatenka/OnionController-ESP32.git](https://github.com/AdrianMatenka/OnionController-ESP32.git)

🤝 Companion App
For the best experience, use the OnionConfigurator (Raylib-based PC application) to visualize your sensor data and tune your controller in real-time.