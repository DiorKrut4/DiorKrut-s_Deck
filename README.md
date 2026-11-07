# DiorKrut's_Deck 🚀

**DiorKrut's_Deck** is a portable AI terminal project based on the **ESP32-S3** microcontroller. The device is designed to interact with the Google Gemini AI directly via Wi-Fi, providing mobility and a sleek interface for on-the-go AI assistance.

## ✨ Features
- **Direct Gemini API Integration:** Communicate with AI without the need for intermediate servers.
- **Portability:** Powered by a Li-Po battery with an integrated charging and protection system.
- **Color Interface:** Data visualization on a 2.8" ILI9341 TFT display.
- **Physical Input:** External keyboard support via ESP32-S3 Native USB (USB OTG).

## 🛠 Hardware Components
- **Microcontroller:** ESP32-S3 DevKit (Dual Type-C ports).
- **Display:** 2.8" TFT LCD (ILI9341 controller, 240x320 resolution, SPI interface).
- **Power System:** - 3.7V 1000 mAh Li-Po battery.
  - TP4056 charging module with over-discharge protection.
  - Mechanical orange rocker switch for power control.
- **Peripherals:** M5 Stack CardKB.

## 💻 Software Stack
- **Language:** C++
- **IDE:** Arduino IDE
- **Core Libraries:**
  - `TFT_eSPI` – Optimized graphics library for the display.
  - `ArduinoJson` – For parsing Gemini API JSON responses.
  - `WiFi.h` & `HTTPClient.h` – For network connectivity.

## 🔧 Wiring Diagram (Current Progress)
The power circuit is currently assembled as follows:
1. **Battery (+/-)** -> **TP4056 (B+/B-)**.
2. **TP4056 (OUT+)** -> **Rocker Switch** -> **ESP32-S3 (5V pin)**.
3. **TP4056 (OUT-)** -> **ESP32-S3 (GND pin)**.

*Note: In the current prototype, the red wire is used for the GND line, and the brown wire is used for the 5V line.*

## 🚀 Getting Started
1. Obtain your API key from [Google AI Studio](https://aistudio.google.com/).
2. Configure `User_Setup.h` in the `TFT_eSPI` library to match your ESP32-S3 pinout.
3. Flash the firmware using Arduino IDE.

## 📄 License
This project is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for details.

---
*Developed by DiorKrut*
