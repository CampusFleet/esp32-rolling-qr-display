# 🚌 CampusFleet ESP32 Dynamic Rolling Cryptographic Bus Display (100% Full-Offline with DS3231 RTC)

[![ESP32](https://img.shields.io/badge/Hardware-ESP32%20%2F%20ESP32--S3-blue.svg)](https://www.espressif.com/)
[![PlatformIO](https://img.shields.io/badge/IDE-PlatformIO-orange.svg)](https://platformio.org/)
[![Arduino](https://img.shields.io/badge/Framework-Arduino-teal.svg)](https://www.arduino.cc/)
[![DS3231 Precision RTC](https://img.shields.io/badge/RTC-DS3231%20TCXO%20%C2%B12ppm-brightgreen.svg)](https://github.com/CampusFleet)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Organization](https://img.shields.io/badge/Org-CampusFleet-purple.svg)](https://github.com/CampusFleet)

Production firmware for physical **ESP32** microcontrollers paired with SPI TFT LCD displays (ST7796, ILI9341, ST7789) mounted at bus boarding entry doors.

### 🌟 100% Air-Gapped, Fully Offline & Zero-Dependency:
- **No GPS Sky View Dependency**: Replaced GPS satellite lock with a **DS3231 Temperature-Compensated Real-Time Clock (TCXO)**. Boots in **<5 milliseconds** with zero waiting for satellites.
- **Underground & Mountain Reliability**: Operates 100% reliably in covered bus depots, basement parking, mountain valleys, tunnels, and inside metal bus bodies where GPS signals are blocked.
- **No Wi-Fi Stack**: Zero `#include <WiFi.h>`, zero Wi-Fi credentials, zero network dependencies.
- **No Cellular/SIM Card**: Zero 4G LTE routers, zero monthly data SIM costs.
- **$\pm 2\,\text{ppm}$ Precision Timekeeping**: Integrated TCXO provides ultra-high precision timekeeping with less than 1 minute of drift per year across extreme cabin temperatures ($-40^\circ\text{C}$ to $+85^\circ\text{C}$).
- **CR2032 Battery Backup**: Coin cell retains live time for 5–8 years even with vehicle master battery disconnected.
- **Zero-Proxy Anti-Tamper Security**:
  1. **2-Second Cryptographic Time Block**: Generates a new rolling cryptographic token every 2 seconds ($\text{TimeBlock} = \lfloor \text{EpochSeconds} / 2 \rfloor$).
  2. **Hardware-Accelerated HMAC-SHA256**: Uses ESP32 hardware crypto acceleration (`mbedtls`) with a 32-character pre-shared master key to sign `CFLEET:{BusId}:{TimeBlock}`.
  3. **Replay & Proxy Immune**: Screenshots, shared photos, and remote proxies expire within 2 seconds. Students scan with their smartphone camera, which submits the token to the cloud for verification.

---

## 🔐 Air-Gapped Cryptographic Architecture

```
┌─────────────────────────────────┐          ┌─────────────────────────┐
│     ESP32 Onboard Terminal      │          │   CampusFleet Cloud     │
│   (100% Full-Offline DS3231)    │          │    Next.js Backend      │
│                                 │          │                         │
│  ⏱️ DS3231 TCXO RTC (±2ppm)    │          │                         │
│                 │               │          │                         │
│         Hardware I2C (400kHz)   │          │                         │
│                 │               │          │                         │
│        Epoch / 2 = TimeBlock    │          │                         │
│                 │               │          │                         │
│        HMAC-SHA256(Key, Msg)    │          │                         │
│        Msg: "CFLEET:42:TBlock"  │          │                         │
│                 │               │          │                         │
│        Dynamic 2s Rolling QR:   │          │                         │
│        cf://b/42?t=TB&k=Token   │          │                         │
└────────────────┬────────────────┘          └────────────┬────────────┘
                 │                                        │
                 │           Student Scans with Camera    │
                 │   (Student phone has cellular data)    │
                 ├───────────────────────────────────────►│
                 │                                        │ Verify HMAC Token
                 │                                        │ Clock Skew Tolerance (±1 Block)
                 │                                        │
                 │           Boarding Verified            │ Confirm Seat & Unlock Pass
                 │◄───────────────────────────────────────┤
                 │                                        │
```

* **Token Payload**: `cf://b/{BusId}?t={TimeBlock}&k={Token}`
* **Rotation Interval**: 2 seconds
* **Air-Gap Security**: The bus terminal has **zero internet exposure**, completely immune to network sniffing, Wi-Fi jamming, de-authentication attacks, or hotspot downtime.

---

## ⚖️ Why DS3231 RTC Over GPS?

| Metric | GPS Module (Old Approach) | DS3231 RTC (Current Standard) |
| :--- | :--- | :--- |
| **Cold Boot Latency** | 35–90 seconds satellite search | **< 5 milliseconds (Instant)** |
| **Indoor / Depot Coverage** | ❌ Fails under metal roofs & garages | **✅ 100% Immune to physical obstructions** |
| **Mountain / Valley Signal** | ⚠️ Drops lock in deep hill terrain | **✅ Self-contained TCXO crystal oscillator** |
| **Antenna Requirements** | Fragile external active patch antenna | **None (Compact on-board I2C module)** |
| **Power Off Retention** | None (cold start on each bus ignition) | **5–8 Years battery backup (CR2032)** |
| **Wiring Complexity** | High-speed UART + antenna cable | **Standard I2C (SDA + SCL, 2 wires)** |
| **Cabin Temperature Drift** | Requires satellite lock to discipline | **Built-in TCXO sensor ($\pm 2\,\text{ppm}$ across temp)** |

---

## 📌 Hardware Wiring Diagrams

### 1. DS3231 Precision Real-Time Clock Wiring (Hardware I2C)

Compatible with any standard **DS3231 AT24C32 RTC Breakout Board**:

| DS3231 Pin | ESP32 Pin | Function | Wiring Note |
| :--- | :--- | :--- | :--- |
| **VCC** | **3V3** | 3.3V Power | Connect to ESP32 3.3V |
| **GND** | **GND** | Common Ground | Connect to ESP32 GND |
| **SDA** | **GPIO 21** | I2C Serial Data | Hardware I2C SDA |
| **SCL** | **GPIO 22** | I2C Serial Clock | Hardware I2C SCL |
| **SQW** | **NC** | Square Wave Out | Not connected (optional 1Hz interrupt) |
| **32K** | **NC** | 32kHz TCXO Out | Not connected |

```
   ┌──────────────────────┐                  ┌──────────────────────┐
   │    DS3231 RTC Module │                  │    ESP32 Dev Module  │
   │                      │                  │                      │
   │                  VCC ├──────────────────┤ 3V3                  │
   │                  GND ├──────────────────┤ GND                  │
   │                  SDA ├──────────────────┤ GPIO 21 (SDA)        │
   │                  SCL ├──────────────────┤ GPIO 22 (SCL)        │
   └──────────────────────┘                  └──────────────────────┘
```

> [!TIP]
> **Battery Notice**: If using standard non-rechargeable **CR2032** coin cells, ensure the diode on the module's trickle-charge circuit is desoldered or disabled. If using rechargeable **LIR2032** coin cells, the trickle-charge circuit can remain active.

---

### 2. SPI TFT LCD Display Wiring (ST7796 / ILI9341 320x480)

| Display Pin | ESP32 Pin | Function | Wiring Note |
| :--- | :--- | :--- | :--- |
| **VCC** | **3V3** | 3.3V Power | Connect to ESP32 3.3V pin |
| **GND** | **GND** | Common Ground | Connect to ESP32 Ground |
| **CS** | **GPIO 15** | SPI Chip Select | Configurable via `User_Setup.h` |
| **RESET / RST** | **GPIO 4** | Hardware Reset | Connect to GPIO 4 |
| **DC / RS** | **GPIO 2** | Data / Command | Connect to GPIO 2 |
| **SDI (MOSI)** | **GPIO 23** | SPI Master Out | Hardware SPI MOSI |
| **SCK / SCL** | **GPIO 18** | SPI Clock | Hardware SPI SCK |
| **SDO (MISO)** | **GPIO 19** | SPI Master In | Hardware SPI MISO (optional) |
| **LED / BL** | **3V3** | Backlight Power | Connect to 3.3V for maximum brightness |

---

## 🛠️ USB Serial Calibration Tool

When deploying new devices or replacing the CR2032 backup battery, technicians can inspect or synchronize the DS3231 clock over USB Serial (`115200` baud):

### Commands:
1. **Calibrate Clock to Epoch Seconds**:
   ```
   SYNC:1793700000
   ```
   *Response*:
   ```
   >> [SUCCESS] DS3231 RTC calibrated to Epoch: 1793700000 (2026-11-03 19:20:00 UTC)
   ```

2. **Inspect Hardware Diagnostics**:
   ```
   STATUS
   ```
   *Response*:
   ```
   ───────────────────────────────────────────────────
   CampusFleet ESP32 Dynamic Rolling Bus Display
   Vehicle: Bus 42 (UP-16-CF-4242) | ID: 42
   RTC Time: 2026-11-03 19:20:00 UTC
   Unix Epoch: 1793700000 | TimeStep: 2s
   DS3231 Temp: 24.50 C | Lost Power: NO (OK)
   Commands: SYNC:<epoch> (Set Time), STATUS (Info)
   ───────────────────────────────────────────────────
   ```

---

## 🚀 Building & Flashing

### Using PlatformIO (Recommended):
```bash
git clone https://github.com/CampusFleet/esp32-rolling-qr-display.git
cd esp32-rolling-qr-display

# Build firmware
pio run

# Flash to connected ESP32
pio run --target upload

# Open serial monitor
pio device monitor
```

### Using Arduino IDE:
1. Install **ESP32** board support in the Arduino Boards Manager.
2. Install required libraries from Library Manager:
   - `RTClib` (by Adafruit)
   - `TFT_eSPI` (by Bodmer)
   - `qrcode` (by Richard Moore)
3. Open `esp32_rolling_qr_display.ino`.
4. Select `ESP32 Dev Module` under Tools → Board and click **Upload**.

---

## ⚡ Cloud Verification Endpoint

When a student scans the dynamic rolling QR using the CampusFleet portal camera scanner:

```http
POST /api/boarding/scan HTTP/1.1
Host: campusfleet.vercel.app
Content-Type: application/json

{
  "studentId": "stud_12345",
  "busId": "42",
  "timeBlock": 89685000,
  "token": "A7B4F92E1C3D8402"
}
```

* **Backend Action**: The server recomputes the HMAC-SHA256 signature for `timeBlock` (accepting $t$ and $t \pm 1$ windows to tolerate network transit latency).
* **Response**:
  ```json
  {
    "success": true,
    "status": "BOARDED",
    "busNumber": "Bus 42",
    "timestamp": "2026-11-03T19:20:02.000Z"
  }
  ```

---

## 📄 License
MIT License. Developed for the CampusFleet University Transit System.
