# 🚌 CampusFleet ESP32 Dynamic Rolling Cryptographic Bus Display

[![ESP32](https://img.shields.io/badge/Hardware-ESP32%20%2F%20ESP32--S3-blue.svg)](https://www.espressif.com/)
[![PlatformIO](https://img.shields.io/badge/IDE-PlatformIO-orange.svg)](https://platformio.org/)
[![Arduino](https://img.shields.io/badge/Framework-Arduino-teal.svg)](https://www.arduino.cc/)
[![GPS Ready](https://img.shields.io/badge/GPS-Offline%20Atomic%20Sync-green.svg)](https://github.com/CampusFleet)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Organization](https://img.shields.io/badge/Org-CampusFleet-purple.svg)](https://github.com/CampusFleet)

Production firmware for physical **ESP32** microcontrollers paired with SPI TFT LCD displays (ST7796, ILI9341, ST7789) mounted at bus boarding entry doors.

Directly integrates with the **[CampusFleet Platform](https://campusfleet.vercel.app)** to deliver **zero-proxy dynamic rolling QR boarding verification**:
1. **2-Second Cryptographic Time Block**: Generates a new rolling cryptographic authentication token every 2 seconds (`TimeBlock = EpochSeconds / 2`).
2. **Hardware-Accelerated HMAC-SHA256**: Uses ESP32 hardware crypto acceleration (`mbedtls`) with a 32-character pre-shared master key to sign `CFLEET:{BusId}:{TimeBlock}`.
3. **Zero-Proxy Anti-Tamper Security**: Screenshots, shared photos, and remote proxies expire within 2 seconds. Students must be physically standing in front of the active display to scan and board.
4. **Dual Time Synchronization**:
   - **🛰️ Offline GPS Satellite Atomic Clock**: Decodes atomic UTC timestamps directly from GPS/GNSS satellites (NEO-6M / NEO-M8N / ATGM336H via UART2). Operates **100% offline in dead zones with zero internet or Wi-Fi**.
   - **📶 Cloud NTP Fallback**: Synchronizes high-precision atomic time via onboard Wi-Fi or 4G LTE Cat-1 hotspot using Google NTP (`time.google.com`) and `pool.ntp.org`.

---

## 🔐 Cryptographic Architecture

```
┌─────────────────────────────────┐          ┌─────────────────────────┐
│         ESP32 Onboard           │          │   CampusFleet Cloud     │
│       Bus Entry Display         │          │    Next.js Backend      │
│  (Offline GPS / Online NTP)     │          │                         │
└────────────────┬────────────────┘          └────────────┬────────────┘
                 │                                        │
        Epoch / 2 = TimeBlock                             │
                 │                                        │
        HMAC-SHA256(Key, Msg)                             │
        Msg: "CFLEET:42:TBlock"                           │
                 │                                        │
        Compact QR:                                       │
        cf://b/42?t=TBlock&k=Token                        │
                 │                                        │
                 │           Student Scans with Camera    │
                 ├───────────────────────────────────────►│
                 │                                        │ Verify HMAC Token
                 │                                        │ Clock Skew Tolerance (±1 Block)
                 │                                        │
                 │           Attendance Confirmed         │ Confirm Seat & Unlock Pass
                 │◄───────────────────────────────────────┤
                 │                                        │
```

* **Token Payload**: `cf://b/{BusId}?t={TimeBlock}&k={Token}`
* **Rotation Interval**: 2 seconds
* **Clock Tolerance**: $\pm 1$ time-step ($\pm 2\text{s}$) to absorb slight network jitter while guaranteeing zero replay attacks.

---

## 📌 Hardware Wiring Diagrams

### 1. SPI TFT LCD Display Wiring (ST7796 / ILI9341 320x480)

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

### 2. GPS Module Wiring for Offline Time Synchronization (UART2)

Compatible with **u-blox NEO-6M, NEO-7M, NEO-M8N, ATGM336H, Quectel L80/L86**:

| GPS Module Pin | ESP32 Pin | Function | Description / Notes |
| :--- | :--- | :--- | :--- |
| **VCC** | **VIN (5V) / 3V3** | Power Input | Most GPS breakout boards have an onboard 3.3V LDO regulator; 5V is recommended for NEO-6M |
| **GND** | **GND** | Ground | Common ground shared with ESP32 |
| **TX** | **GPIO 16 (RX2)** | Serial Data Out | Connects to ESP32 Hardware UART2 Receive pin |
| **RX** | **GPIO 17 (TX2)** | Serial Data In | Connects to ESP32 Hardware UART2 Transmit pin (optional for read-only NMEA) |
| **PPS** | **NC (Optional)** | Pulse Per Second | Optional $\pm 1\,\mu\text{s}$ atomic pulse trigger |

```
   ┌──────────────────────┐                  ┌──────────────────────┐
   │   u-blox NEO-6M /    │                  │    ESP32 Dev Module  │
   │   NEO-M8N GPS Module │                  │                      │
   │                      │                  │                      │
   │                  VCC ├──────────────────┤ 5V / VIN             │
   │                  GND ├──────────────────┤ GND                  │
   │                  TX  ├──────────────────┤ GPIO 16 (RX2)        │
   │                  RX  ├──────────────────┤ GPIO 17 (TX2)        │
   └──────────────────────┘                  └──────────────────────┘
```

---

## 🛰️ Step-by-Step: Connecting GPS for Offline Time Synchronization

### Why Offline GPS Time is Essential in Transit
In campus and inter-city transit systems, buses frequently operate through **cellular dead zones**, underground parking decks, remote suburban corridors, and express highways where 4G LTE hotspots or campus Wi-Fi lose connectivity. 

Because CampusFleet's rolling QR code security relies on synchronized time (`Epoch / 2`), losing network connectivity would normally prevent NTP clock updates. 

**With GPS connected**:
- GNSS satellites (GPS / Galileo / BeiDou) continuously broadcast high-precision **UTC atomic clock timestamps** worldwide for free.
- The ESP32 decodes standard NMEA sentences (`$GPRMC` and `$GPGGA`) over hardware UART.
- The ESP32's internal Real-Time Clock (RTC) is calibrated offline down to the exact second via `settimeofday()`.
- **The dynamic QR display continues rolling with 100% cryptographic precision completely offline with zero internet access.**

---

### Step 1: Install the `TinyGPSPlus` Library

#### If using PlatformIO:
The library is already included in `platformio.ini`:
```ini
lib_deps =
    bodmer/TFT_eSPI @ ^2.5.43
    ricmoo/qrcode @ ^0.0.1
    mikalhart/TinyGPSPlus @ ^1.0.3
```

#### If using Arduino IDE:
1. Open Arduino IDE.
2. Go to **Sketch $\to$ Include Library $\to$ Manage Libraries...** (or press `Ctrl+Shift+I` / `Cmd+Shift+I`).
3. Search for **`TinyGPSPlus`** by Mikal Hart.
4. Click **Install**.

---

### Step 2: Wire the GPS Module to ESP32 UART2
1. Connect the GPS **TX pin** to ESP32 **GPIO 16**.
2. Connect the GPS **RX pin** to ESP32 **GPIO 17**.
3. Connect the GPS **VCC** to **5V / VIN** and **GND** to **GND**.

> [!TIP]
> Ensure the ceramic patch antenna has an unobstructed view toward the sky. Mount the antenna on the bus dashboard near the windshield or front window.

---

### Step 3: Enable Offline GPS in Firmware
Open `esp32_rolling_qr_display.ino` (or `src/main.cpp`) and verify that `USE_OFFLINE_GPS` is enabled:

```cpp
// ── Offline GPS Configuration ──────────────────────────────────────────────
#define USE_OFFLINE_GPS 1   // Set to 1 for 100% offline atomic GPS synchronization
                            // Set to 0 for Wi-Fi / Hotspot NTP only

#if USE_OFFLINE_GPS
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

#define GPS_RX_PIN 16       // Connect to GPS module TX
#define GPS_TX_PIN 17       // Connect to GPS module RX
#define GPS_BAUD   9600     // Standard NMEA baud rate

TinyGPSPlus gps;
HardwareSerial gpsSerial(2); // ESP32 Hardware UART2
#endif
```

---

### Step 4: How the Offline GPS Synchronization Works

The firmware includes a dedicated routine that reads satellite data, converts UTC date/time to Unix Epoch seconds, and sets the ESP32 internal hardware clock:

```cpp
#if USE_OFFLINE_GPS
bool syncTimeFromGPS(uint32_t timeoutMs = 30000) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (gpsSerial.available() > 0) {
      gps.encode(gpsSerial.read());
    }

    // Verify valid time, date, and modern year
    if (gps.time.isValid() && gps.date.isValid() && gps.date.year() >= 2025) {
      struct tm t = {0};
      t.tm_year = gps.date.year() - 1900;
      t.tm_mon  = gps.date.month() - 1;
      t.tm_mday = gps.date.day();
      t.tm_hour = gps.time.hour();
      t.tm_min  = gps.time.minute();
      t.tm_sec  = gps.time.second();

      time_t epoch = mktime(&t); // UTC Epoch timestamp
      if (epoch > 1700000000) {
        struct timeval tv = { 
          .tv_sec = epoch, 
          .tv_usec = (suseconds_t)(gps.time.centisecond() * 10000) 
        };
        // Calibrate ESP32 internal Real-Time Clock
        settimeofday(&tv, NULL);
        return true;
      }
    }
    delay(10);
  }
  return false;
}
#endif
```

In the main `loop()`, continuous NMEA feed maintains drift-free clock discipline:

```cpp
#if USE_OFFLINE_GPS
  // Disciplines the internal RTC in the background while the bus travels
  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      if (gps.time.isUpdated() && gps.time.isValid() && gps.date.isValid() && gps.date.year() >= 2025) {
        struct tm t = {0};
        t.tm_year = gps.date.year() - 1900;
        t.tm_mon  = gps.date.month() - 1;
        t.tm_mday = gps.date.day();
        t.tm_hour = gps.time.hour();
        t.tm_min  = gps.time.minute();
        t.tm_sec  = gps.time.second();

        time_t epoch = mktime(&t);
        if (epoch > 1700000000) {
          struct timeval tv = { 
            .tv_sec = epoch, 
            .tv_usec = (suseconds_t)(gps.time.centisecond() * 10000) 
          };
          settimeofday(&tv, NULL);
          activeSyncSource = "GPS ATOMIC";
        }
      }
    }
  }
#endif
```

---

### Step 5: Dual-Mode Operation (Seamless Fallback)

1. **Boot Priority**:
   - The device initializes UART2 and checks for GPS satellite atomic lock.
   - If satellites are locked, it calibrates the RTC immediately without touching Wi-Fi:
     $$\text{Display: } \mathbf{\text{GPS Atomic Lock Acquired (Offline) } \checkmark}$$
   - If satellites are temporarily obstructed (e.g., parked deep inside a maintenance depot), it falls back to Wi-Fi NTP:
     $$\text{Display: } \mathbf{\text{NTP Synchronized } \checkmark \text{ (Online)}}$$
2. **On the Road**:
   - As soon as the bus pulls out of the depot and acquires satellites, GPS automatically takes over as the primary atomic time discipline.
   - The footer of the display dynamically indicates the active source: `EPOCH: 1793674686s (GPS ATOMIC)` or `EPOCH: 1793674686s (NTP CLOUD)`.

---

### Step 6: Satellite Lock LED Indicator & Diagnostics

* **NEO-6M Status LED**:
  * **Solid OFF / Constant ON**: Searching for satellites (almanac download / cold start, takes 35–60 seconds).
  * **Blinking Blue LED (1 Hz)**: **3D Satellite Fix Confirmed**. Valid atomic time received.
* **Serial Monitor Diagnostics** (Baud: `115200`):
  ```text
  [GPS] Initializing GPS UART2 on GPIO 16 (RX) / 17 (TX)...
  [GPS] Satellite atomic lock acquired! RTC calibrated offline.
  TimeBlock: 89683734 | Token: 9E42BC01A7F85D31 | Source: GPS ATOMIC
  ```

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
     * Install **TinyGPSPlus** by Mikal Hart (`TinyGPSPlus`).

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
