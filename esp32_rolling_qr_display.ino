/**
 * CampusFleet IoT Terminal Firmware: ESP32 Dynamic Rolling Cryptographic Bus Display
 * 
 * Hardware Target: ESP32-S3 / ESP32 NodeMCU
 * Display Target: 3.5" or 2.8" SPI TFT LCD (ST7796 / ILI9341 / ST7789)
 * Time Source: GPS Atomic Clock (NEO-6M/M8N via UART2) or NTP via 4G LTE Cat-1 / Wi-Fi
 * Cryptography: Hardware-accelerated HMAC-SHA256 (mbedtls) with 32-character pre-shared key
 * 
 * Security Standard:
 * - Computes rolling token every 2 seconds: TimeBlock = EpochSeconds / 2
 * - Token = Truncated 16-hex HMAC-SHA256(SecretKey, "CFLEET:" + BusId + ":" + TimeBlock)
 * - Renders dynamic QR: cf://b/{BusId}?t={TimeBlock}&k={Token}
 * - Students scan with camera to mark attendance without physical RFID cards
 * - Zero proxy attendance: screenshots and photos expire in 2 seconds.
 * - OFFLINE OPERATION: Uses GPS satellite atomic clock to keep time in dead zones.
 */

#include <WiFi.h>
#include <time.h>
#include <SPI.h>
#include <TFT_eSPI.h>       // Fast hardware-accelerated SPI graphics library
#include <qrcode.h>         // Lightweight embedded QR generator by Richard Moore
#include "mbedtls/md.h"

// ── Offline GPS Configuration ──────────────────────────────────────────────
#define USE_OFFLINE_GPS 1   // 1 = Enable GPS module via UART2 for 100% offline atomic clock sync
                            // 0 = Use Wi-Fi / LTE Hotspot NTP only

#if USE_OFFLINE_GPS
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

#define GPS_RX_PIN 16       // Connect to GPS module TX
#define GPS_TX_PIN 17       // Connect to GPS module RX (optional)
#define GPS_BAUD   9600     // Standard NMEA baud rate (NEO-6M / NEO-M8N / ATGM336H)

TinyGPSPlus gps;
HardwareSerial gpsSerial(2); // ESP32 Hardware UART2
#endif

// ── Fleet & Network Configuration ──────────────────────────────────────────
const char* WIFI_SSID       = "Bus_Onboard_WiFi";
const char* WIFI_PASSWORD   = "TransitPass123";

const char* BUS_ID          = "42";               // Unique vehicle identifier
const char* BUS_NUMBER      = "Bus 42";
const char* REGISTRATION_NO = "UP-16-CF-4242";

// 32-Character Pre-Shared Master Secret Key (Shared with Next.js web server)
const char* MASTER_KEY_32   = "CFLEET_SECURE_BUS_KEY_32CHAR_V2";

const int TIME_STEP_SEC     = 2;                  // QR code rotates every 2 seconds
const int QR_VERSION        = 3;                  // Version 3 (29x29 matrix) - fast optical read

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
void renderQRCode(String payload, String token, long epochSec, const char* syncSource) {
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

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("EPOCH: " + String(epochSec) + "s (" + String(syncSource) + ")", tft.width() / 2, 300, 1);
}

#if USE_OFFLINE_GPS
// ── Synchronize ESP32 Internal RTC from GPS Satellite Atomic Time ──────────
bool syncTimeFromGPS(uint32_t timeoutMs = 30000) {
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
        struct timeval tv = { .tv_sec = epoch, .tv_usec = (suseconds_t)(gps.time.centisecond() * 10000) };
        settimeofday(&tv, NULL);
        return true;
      }
    }
    delay(10);
  }
  return false;
}
#endif

// Active time source label
const char* activeSyncSource = "STANDBY";

// ── Setup ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1); // Landscape mode
  tft.fillScreen(TFT_BLACK);

  // Header Banner
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("CAMPUSFLEET BOARDING TERMINAL", tft.width() / 2, 10, 4);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(String(BUS_NUMBER) + " • " + String(REGISTRATION_NO), tft.width() / 2, 40, 2);

  deriveBusKey();

  bool timeSynced = false;

#if USE_OFFLINE_GPS
  Serial.println("[GPS] Initializing GPS UART2 on GPIO 16 (RX) / 17 (TX)...");
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Acquiring GPS Satellite Clock...", tft.width() / 2, 160, 2);

  // Attempt to acquire satellite atomic fix
  if (syncTimeFromGPS(35000)) {
    timeSynced = true;
    activeSyncSource = "GPS ATOMIC";
    Serial.println("[GPS] Satellite atomic lock acquired! RTC calibrated offline.");
    tft.fillRect(0, 70, tft.width(), 240, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("GPS Atomic Lock Acquired (Offline) ✓", tft.width() / 2, 60, 2);
  } else {
    Serial.println("[GPS] No satellite lock yet, checking Wi-Fi / LTE fallback...");
  }
#endif

  if (!timeSynced) {
    tft.fillRect(0, 150, tft.width(), 40, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Connecting to Wi-Fi NTP...", tft.width() / 2, 160, 2);

    // Connect to Wi-Fi / LTE Cat-1 Hotspot
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
      delay(500);
      retry++;
    }

    // Sync atomic time via NTP servers
    configTime(19800, 0, "pool.ntp.org", "time.google.com"); // 19800s offset for IST (+5:30)
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10000)) {
      timeSynced = true;
      activeSyncSource = "NTP CLOUD";
      tft.fillRect(0, 70, tft.width(), 240, TFT_BLACK);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawString("NTP Synchronized ✓ (Online)", tft.width() / 2, 60, 2);
    }
  }

  if (!timeSynced) {
    tft.fillRect(0, 70, tft.width(), 240, TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Time Sync Failed - Check GPS / Wi-Fi", tft.width() / 2, 160, 2);
  }
}

// ── Main Loop ──────────────────────────────────────────────────────────────
long lastTimeBlock = -1;

void loop() {
#if USE_OFFLINE_GPS
  // Keep feeding GPS UART stream in background to maintain continuous atomic clock discipline
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
          struct timeval tv = { .tv_sec = epoch, .tv_usec = (suseconds_t)(gps.time.centisecond() * 10000) };
          settimeofday(&tv, NULL);
          activeSyncSource = "GPS ATOMIC";
        }
      }
    }
  }
#endif

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
    Serial.print(" | Source: ");
    Serial.println(activeSyncSource);

    renderQRCode(qrPayload, token, nowSec, activeSyncSource);
  }

  delay(100); // 100ms polling tick
}
