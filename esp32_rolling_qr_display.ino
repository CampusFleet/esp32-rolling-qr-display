/**
 * CampusFleet IoT Terminal Firmware: ESP32 Dynamic Rolling Cryptographic Bus Display
 * 
 * Hardware Target: ESP32 / ESP32-S3 NodeMCU
 * Display Target: 3.5" or 2.8" SPI TFT LCD (ST7796 / ILI9341 / ST7789)
 * Time Source: GPS/GNSS Satellite Atomic Clock (u-blox NEO-6M / NEO-M8N / ATGM336H via UART2)
 * Cryptography: Hardware-accelerated HMAC-SHA256 (mbedtls) with 32-character pre-shared key
 * 
 * 100% AIR-GAPPED & FULLY OFFLINE:
 * - NO Wi-Fi, NO cellular internet, NO SIM card required.
 * - Receives high-precision UTC atomic time directly from orbiting GPS satellites.
 * - Hardware RTC on ESP32 is calibrated offline via settimeofday().
 * - Computes rolling dynamic token every 2 seconds: TimeBlock = EpochSeconds / 2
 * - Token = Truncated 16-hex HMAC-SHA256(SecretKey, "CFLEET:" + BusId + ":" + TimeBlock)
 * - Renders dynamic QR: cf://b/{BusId}?t={TimeBlock}&k={Token}
 * - Students scan with camera to mark attendance; screenshots expire in 2 seconds.
 */

#include <time.h>
#include <SPI.h>
#include <TFT_eSPI.h>         // Fast hardware-accelerated SPI graphics library
#include <qrcode.h>           // Lightweight embedded QR generator by Richard Moore
#include <TinyGPSPlus.h>      // NMEA satellite GPS parser
#include <HardwareSerial.h>   // ESP32 Hardware UART
#include "mbedtls/md.h"

// ── GPS Module Configuration (Hardware UART2) ──────────────────────────────
#define GPS_RX_PIN 16         // Connect to GPS module TX
#define GPS_TX_PIN 17         // Connect to GPS module RX (optional for read-only NMEA)
#define GPS_BAUD   9600       // Standard NMEA baud rate (NEO-6M, NEO-M8N, ATGM336H)

TinyGPSPlus gps;
HardwareSerial gpsSerial(2);   // ESP32 Hardware UART2

// ── Fleet Bus Identity & Cryptography ──────────────────────────────────────
const char* BUS_ID          = "42";               // Unique vehicle identifier
const char* BUS_NUMBER      = "Bus 42";
const char* REGISTRATION_NO = "UP-16-CF-4242";

// 32-Character Pre-Shared Master Secret Key (Shared with CampusFleet cloud backend)
const char* MASTER_KEY_32   = "CFLEET_SECURE_BUS_KEY_32CHAR_V2";

const int TIME_STEP_SEC     = 2;                  // Dynamic QR rotates every 2 seconds
const int QR_VERSION        = 3;                  // Version 3 (29x29 matrix) - fast optical camera read

// ── Globals ────────────────────────────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();
QRCode qrcode;
uint8_t qrcodeData[qrcode_getBufferSize(QR_VERSION)];

// Derived bus-specific 32-char key
char busSpecificKey[33];

// ── Derive Bus Specific Key via HMAC ────────────────────────────────────────
void deriveBusKey() {
  byte hmacResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)MASTER_KEY_32, strlen(MASTER_KEY_32));
  
  String salt = "BUS_SALT:" + String(BUS_ID);
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)salt.c_str(), salt.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);

  for (int i = 0; i < 16; i++) {
    sprintf(&busSpecificKey[i * 2], "%02x", hmacResult[i]);
  }
  busSpecificKey[32] = '\0';
}

// ── Compute 16-Hex HMAC Token for Time Block ────────────────────────────────
String computeRollingToken(long timeBlock) {
  byte hmacResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)busSpecificKey, strlen(busSpecificKey));

  String msg = "CFLEET:" + String(BUS_ID) + ":" + String(timeBlock);
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)msg.c_str(), msg.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);

  char tokenHex[17];
  for (int i = 0; i < 8; i++) {
    sprintf(&tokenHex[i * 2], "%02X", hmacResult[i]);
  }
  tokenHex[16] = '\0';
  return String(tokenHex);
}

// ── Draw Dynamic QR onto TFT Display ────────────────────────────────────────
void renderQRCode(String payload, String token, long epochSec) {
  qrcode_initText(&qrcode, qrcodeData, QR_VERSION, ECC_MEDIUM, payload.c_str());

  int scale = 6;                          // Pixel multiplier per QR module
  int qrSize = qrcode.size * scale;       // 29 * 6 = 174px
  int offsetX = (tft.width() - qrSize) / 2;
  int offsetY = 80;

  // Render White Background Box
  tft.fillRect(offsetX - 10, offsetY - 10, qrSize + 20, qrSize + 20, TFT_WHITE);

  // Render QR Modules
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        tft.fillRect(offsetX + (x * scale), offsetY + (y * scale), scale, scale, TFT_BLACK);
      }
    }
  }

  // Footer: Dynamic Token & Time Display
  tft.fillRect(0, 290, tft.width(), 30, TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("TOKEN: " + token, tft.width() / 2, 280, 2);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("EPOCH: " + String(epochSec) + "s (OFFLINE GPS ATOMIC)", tft.width() / 2, 300, 1);
}

// ── Synchronize ESP32 Internal RTC from GPS Satellite Atomic Time ──────────
bool syncTimeFromGPS(uint32_t timeoutMs = 60000) {
  uint32_t start = millis();
  int dots = 0;
  
  while (millis() - start < timeoutMs) {
    while (gpsSerial.available() > 0) {
      gps.encode(gpsSerial.read());
    }

    // Verify valid time, date, and modern year from satellites
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
        // Calibrate ESP32 internal hardware Real-Time Clock
        settimeofday(&tv, NULL);
        return true;
      }
    }

    // Update screen search feedback every 500ms
    if ((millis() - start) % 500 == 0) {
      dots = (dots + 1) % 4;
      String dotsStr = "";
      for (int i = 0; i < dots; i++) dotsStr += ".";
      tft.fillRect(0, 190, tft.width(), 30, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextDatum(TC_DATUM);
      tft.drawString("Searching satellites" + dotsStr, tft.width() / 2, 190, 2);
    }

    delay(10);
  }
  return false;
}

// ── Setup ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("[Terminal] CampusFleet Full-Offline Dynamic QR Terminal booting...");

  // Initialize TFT Display
  tft.init();
  tft.setRotation(1); // Landscape mode
  tft.fillScreen(TFT_BLACK);

  // Header Banner
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("CAMPUSFLEET BOARDING TERMINAL", tft.width() / 2, 10, 4);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(String(BUS_NUMBER) + " • " + String(REGISTRATION_NO), tft.width() / 2, 40, 2);

  // Pre-compute bus cryptographic key
  deriveBusKey();

  // Initialize GPS Hardware UART2
  Serial.println("[GPS] Initializing GPS UART2 on GPIO 16 (RX) / 17 (TX) at 9600 baud...");
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Acquiring GPS Satellite Clock...", tft.width() / 2, 150, 2);

  // Loop until first satellite atomic fix is acquired
  bool locked = false;
  while (!locked) {
    if (syncTimeFromGPS(30000)) {
      locked = true;
      Serial.println("[GPS] Satellite atomic lock acquired! RTC calibrated offline.");
      tft.fillRect(0, 120, tft.width(), 120, TFT_BLACK);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawString("GPS Atomic Lock Acquired ✓", tft.width() / 2, 60, 2);
    } else {
      Serial.println("[GPS] Still waiting for satellite fix. Check antenna sky view...");
      tft.fillRect(0, 140, tft.width(), 40, TFT_BLACK);
      tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      tft.drawString("Retrying GPS Lock (Ensure Open Sky)...", tft.width() / 2, 150, 2);
    }
  }

  // Clear startup messages before rendering QR
  delay(1000);
  tft.fillRect(0, 60, tft.width(), 260, TFT_BLACK);
}

// ── Main Loop ──────────────────────────────────────────────────────────────
long lastTimeBlock = -1;

void loop() {
  // Continuously feed GPS UART stream to maintain sub-second RTC discipline
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

  // Read disciplined RTC epoch seconds
  time_t nowSec;
  time(&nowSec);

  long currentTimeBlock = nowSec / TIME_STEP_SEC;

  // Re-generate and render when the time block increments (every 2 seconds)
  if (currentTimeBlock != lastTimeBlock) {
    lastTimeBlock = currentTimeBlock;

    String token = computeRollingToken(currentTimeBlock);
    // Compact URI format
    String qrPayload = "cf://b/" + String(BUS_ID) + "?t=" + String(currentTimeBlock) + "&k=" + token;

    Serial.print("TimeBlock: ");
    Serial.print(currentTimeBlock);
    Serial.print(" | Token: ");
    Serial.print(token);
    Serial.print(" | Sats: ");
    Serial.println(gps.satellites.value());

    renderQRCode(qrPayload, token, nowSec);
  }

  delay(100); // 100ms polling tick
}
