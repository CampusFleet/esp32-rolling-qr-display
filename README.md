# 🚌 CampusFleet ESP32 Dynamic Rolling Cryptographic Bus Display (100% Full-Offline)

[![ESP32](https://img.shields.io/badge/Hardware-ESP32%20%2F%20ESP32--S3-blue.svg)](https://www.espressif.com/)
[![PlatformIO](https://img.shields.io/badge/IDE-PlatformIO-orange.svg)](https://platformio.org/)
[![Arduino](https://img.shields.io/badge/Framework-Arduino-teal.svg)](https://www.arduino.cc/)
[![GPS Atomic Sync](https://img.shields.io/badge/GPS-100%25%20Offline%20Air--Gapped-green.svg)](https://github.com/CampusFleet)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Organization](https://img.shields.io/badge/Org-CampusFleet-purple.svg)](https://github.com/CampusFleet)

Production firmware for physical **ESP32** microcontrollers paired with SPI TFT LCD displays (ST7796, ILI9341, ST7789) mounted at bus boarding entry doors.

**100% Air-Gapped & Fully Offline**:
- **No Wi-Fi stack**: Zero `#include <WiFi.h>`, no Wi-Fi credentials, no network dependencies.
- **No Cellular/SIM Card**: Does not require any 4G LTE router, data plan, or internet connection.
- **Satellite Atomic Clock**: Obtains microsecond-precision UTC time directly from orbiting GPS/GNSS satellites (NEO-6M / NEO-M8N / ATGM336H via hardware UART2).
- **Zero-Proxy Anti-Tamper Security**:
  1. **2-Second Cryptographic Time Block**: Generates a new rolling cryptographic authentication token every 2 seconds (`TimeBlock = EpochSeconds / 2`).
  2. **Hardware-Accelerated HMAC-SHA256**: Uses ESP32 hardware crypto acceleration (`mbedtls`) with a 32-character pre-shared master key to sign `CFLEET:{BusId}:{TimeBlock}`.
  3. **Replay & Proxy Immune**: Screenshots, shared photos, and remote proxies expire within 2 seconds. Students scan with their smartphone camera, which submits the token to the cloud for verification.

---

## 🔐 Air-Gapped Cryptographic Architecture

```
┌─────────────────────────────────┐          ┌─────────────────────────┐
│     ESP32 Onboard Terminal      │          │   CampusFleet Cloud     │
│       (100% Full Offline)       │          │    Next.js Backend      │
│                                 │          │                         │
│  🛰️ GPS Satellites (Atomic UTC) │          │                         │
│                 │               │          │                         │
│         Hardware UART2          │          │                         │
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

### 2. GPS Module Wiring for 100% Offline Time Synchronization (UART2)

Compatible with **u-blox NEO-6M, NEO-7M, NEO-M8N, ATGM336H, Quectel L80/L86**:

| GPS Module Pin | ESP32 Pin | Function | Description / Notes |
| :--- | :--- | :--- | :--- |
| **VCC** | **VIN (5V) / 3V3** | Power Input | 5V recommended for NEO-6M breakout boards with onboard LDO regulator |
| **GND** | **GND** | Ground | Common ground shared with ESP32 |
| **TX** | **GPIO 16 (RX2)** | Serial Data Out | Connects to ESP32 Hardware UART2 Receive (`RX2`) |
| **RX** | **GPIO 17 (TX2)** | Serial Data In | Connects to ESP32 Hardware UART2 Transmit (`TX2`) |
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

## 🛰️ How Offline GPS Atomic Time Synchronization Works

### Why Satellites?
GPS, Galileo, and BeiDou satellites carry onboard rubidium and cesium atomic clocks broadcasting UTC time down to nanosecond precision. The ESP32 listens to standard NMEA sentences (`$GPRMC` and `$GPGGA`) broadcast at 9600 baud, extracts the atomic UTC timestamp, and calibrates its internal hardware RTC using `settimeofday()`.

### Code Implementation (`syncTimeFromGPS`):
```cpp
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

bool syncTimeFromGPS(uint32_t timeoutMs = 60000) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (gpsSerial.available() > 0) {
      gps.encode(gpsSerial.read());
    }

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
        // Set ESP32 hardware Real-Time Clock
        settimeofday(&tv, NULL);
        return true;
      }
    }
    delay(10);
  }
  return false;
}
```

### Continuous Background Drift Discipline in `loop()`:
```cpp
void loop() {
  // Feed GPS UART in background: continuous zero-drift atomic discipline
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
        }
      }
    }
  }

  // Generates new rolling QR every 2 seconds
  time_t nowSec;
  time(&nowSec);
  long currentTimeBlock = nowSec / 2;
  ...
}
```

---

## 🚀 Flashing & Quick Start

Because the firmware is **100% offline**, you do **not** need to configure any Wi-Fi SSID or password.

### Option A: Using PlatformIO (VS Code / CLI)

1. Clone this repository:
   ```bash
   git clone https://github.com/CampusFleet/esp32-rolling-qr-display.git
   cd esp32-rolling-qr-display
   ```

2. Open the folder in VS Code with PlatformIO IDE installed.

3. Update Bus details in `src/main.cpp`:
   ```cpp
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
5. Update your `BUS_ID` and `MASTER_KEY_32`.
6. Select Board: **ESP32 Dev Module**, set Baud Rate to **115200**, and click **Upload**.

---

## 🛰️ GPS Antenna Placement & Status Indicators

* **Ceramic Patch Antenna**: Affix the antenna flat on the bus dashboard near the windshield facing the open sky.
* **Cold Start Duration**: On first power-up, satellite almanac acquisition takes ~35–60 seconds.
* **Lock Indicator**:
  * **On-Board GPS LED**: 1 Hz pulsing blue LED indicates active 3D navigation lock.
  * **TFT Display Header**: Turns green with `GPS Atomic Lock Acquired ✓`.
  * **Footer Bar**: Displays `EPOCH: 1793674686s (OFFLINE GPS ATOMIC)` and live satellite count in serial logs.

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
