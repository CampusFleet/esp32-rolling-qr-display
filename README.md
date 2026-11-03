# 🚌 CampusFleet ESP32 Dynamic Rolling Cryptographic Bus Display

[![ESP32](https://img.shields.io/badge/Hardware-ESP32%20%2F%20ESP32--S3-blue.svg)](https://www.espressif.com/)
[![PlatformIO](https://img.shields.io/badge/IDE-PlatformIO-orange.svg)](https://platformio.org/)
[![Arduino](https://img.shields.io/badge/Framework-Arduino-teal.svg)](https://www.arduino.cc/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Organization](https://img.shields.io/badge/Org-CampusFleet-purple.svg)](https://github.com/CampusFleet)

Production firmware for physical **ESP32** microcontrollers paired with SPI TFT LCD displays (ST7796, ILI9341, ST7789) mounted at bus boarding entry doors.

Directly integrates with the **[CampusFleet Platform](https://campusfleet.vercel.app)** to deliver **zero-proxy dynamic rolling QR boarding verification**:
1. **2-Second Cryptographic Time Block**: Generates a new rolling cryptographic authentication token every 2 seconds (`TimeBlock = EpochSeconds / 2`).
2. **Hardware-Accelerated HMAC-SHA256**: Uses ESP32 hardware crypto acceleration (`mbedtls`) with a 32-character pre-shared master key to sign `CFLEET:{BusId}:{TimeBlock}`.
3. **Zero-Proxy Anti-Tamper Security**: Screenshots, shared photos, and remote proxies expire within 2 seconds. Students must be physically standing in front of the active display to scan and board.
4. **NTP Atomic Clock Synchronization**: Synchronizes high-precision atomic time via onboard Wi-Fi / 4G LTE Cat-1 hotspot using Google NTP and pool.ntp.org.

---

## 🔐 Cryptographic Architecture

```
┌─────────────────────────┐          ┌─────────────────────────┐
│     ESP32 Onboard       │          │   CampusFleet Cloud     │
│   Bus Entry Display     │          │    Next.js Backend      │
└────────────┬────────────┘          └────────────┬────────────┘
             │                                    │
    Epoch / 2 = TimeBlock                         │
             │                                    │
    HMAC-SHA256(Key, Msg)                         │
    Msg: "CFLEET:42:TBlock"                       │
             │                                    │
    Compact QR:                                   │
    cf://b/42?t=TBlock&k=Token                    │
             │                                    │
             │       Student Scans with Camera    │
             ├───────────────────────────────────►│
             │                                    │ Verify HMAC Token
             │                                    │ Clock Skew Tolerance (±1 Block)
             │                                    │
             │       Attendance Confirmed         │ Confirm Seat & Unlock Pass
             │◄───────────────────────────────────┤
             │                                    │
```

* **Token Payload**: `cf://b/{BusId}?t={TimeBlock}&k={Token}`
* **Rotation Interval**: 2 seconds
* **Clock Tolerance**: $\pm 1$ time-step ($\pm 2\text{s}$) to absorb slight network jitter while guaranteeing zero replay attacks.

---

## 📌 Wiring Diagram (ESP32 $\leftrightarrow$ SPI TFT LCD Display)

Recommended display: **3.5" or 2.8" SPI TFT LCD (ST7796 or ILI9341)**

| Display Pin | ESP32 Pin | Function | Wiring Note |
| :--- | :--- | :--- | :--- |
| **VCC** | **3V3** | 3.3V Power | Connect to ESP32 3.3V output |
| **GND** | **GND** | Common Ground | Connect to ESP32 ground |
| **CS** | **GPIO 15** | SPI Chip Select | Configurable via User_Setup.h |
| **RESET / RST** | **GPIO 4** | Hardware Reset | Connect to GPIO 4 |
| **DC / RS** | **GPIO 2** | Data / Command | Connect to GPIO 2 |
| **SDI (MOSI)** | **GPIO 23** | SPI Master Out | Hardware SPI MOSI |
| **SCK / SCL** | **GPIO 18** | SPI Clock | Hardware SPI SCK |
| **SDO (MISO)** | **GPIO 19** | SPI Master In | Hardware SPI MISO (optional) |
| **LED / BL** | **3V3** | Backlight Power | Connect to 3.3V for max brightness |

---

## 🚀 Flashing & Quick Start

You can build and flash using either **PlatformIO** (recommended) or the **Arduino IDE**.

### Option A: Using PlatformIO (VS Code / CLI)

1. Clone this repository:
   ```bash
   git clone https://github.com/CampusFleet/esp32-rolling-qr-display.git
   cd esp32-rolling-qr-display
   ```

2. Open the folder in VS Code with PlatformIO IDE installed.

3. Update Wi-Fi and Bus details in `src/main.cpp`:
   ```cpp
   const char* WIFI_SSID       = "YOUR_BUS_WIFI_HOTSPOT";
   const char* WIFI_PASSWORD   = "YOUR_HOTSPOT_PASSWORD";
   const char* BUS_ID          = "42";               // Target Bus ID
   const char* BUS_NUMBER      = "Bus 42";
   const char* REGISTRATION_NO = "UP-16-CF-4242";
   const char* MASTER_KEY_32   = "CFLEET_SECURE_BUS_KEY_32CHAR_V2";
   ```

4. Connect your ESP32 board via USB.
5. Compile and flash:
   ```bash
   pio run -t upload
   pio device monitor
   ```

---

### Option B: Using Arduino IDE

1. **Install ESP32 Board Package**:
   * Go to **Settings $\to$ Additional Board Manager URLs** and add:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   * In **Tools $\to$ Board $\to$ Boards Manager**, search for `esp32` by Espressif Systems and install.

2. **Install Required Libraries**:
   * Open **Library Manager** (`Ctrl+Shift+I` or `Cmd+Shift+I`):
     * Install **TFT_eSPI** by Bodmer.
     * Install **qrcode** by Richard Moore (`ricmoo/qrcode`).

3. **Configure TFT_eSPI**:
   * In your Arduino libraries folder (`libraries/TFT_eSPI/User_Setup.h`), configure the pins matching your display driver (e.g. `#define ST7796_DRIVER` or `#define ILI9341_DRIVER`).

4. Open `esp32_rolling_qr_display.ino`.
5. Select Board: **ESP32 Dev Module**, set Baud Rate to **115200**, and click **Upload**.

---

## ⚡ Cloud Verification Endpoint

When a student scans the dynamic rolling QR from the student portal camera scanner:

```http
POST /api/boarding/scan HTTP/1.1
Host: campusfleet.vercel.app
Content-Type: application/json

{
  "studentId": "stud_12345",
  "busId": "42",
  "timeBlock": 89451230,
  "token": "A7B4F92E1C3D8402"
}
```

* **Backend Action**: The server recomputes the HMAC-SHA256 signature for `timeBlock` (accepting $t$ and $t \pm 1$).
* **Response**:
  ```json
  {
    "success": true,
    "status": "BOARDED",
    "busNumber": "Bus 42",
    "timestamp": "2026-11-03T03:55:00.000Z"
  }
  ```

---

## 🔗 Related Repositories

* **[CampusFleet Core Platform](https://github.com/CampusFleet/platform)**: Next.js fullstack platform, automated zone allocation, telematics, and student portal.
* **[CampusFleet ESP32 RFID Terminal](https://github.com/CampusFleet/esp32-rfid-terminal)**: Firmware for physical MFRC522 RFID card tap readers.

---

## 📄 License

MIT License. Developed for the CampusFleet University Transit System.
