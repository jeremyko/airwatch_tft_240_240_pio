#include <Arduino.h>
#include <Adafruit_GFX.h>     
#include <Adafruit_ST7789.h>  
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>

#include "private_defines.h"

void initBuf();
void displayData();
void connectWiFi();
bool getDustData();
String httpGETRequest();

unsigned long lastTimeDust = 0;
unsigned long lastTempHumTime = 0;
unsigned long timerDelayDust = 120000;    // 2 min
unsigned long timerDelayTempHum = 20000;  // 20 sec
char oldMsrdtBuf[20];
char msrdtBuf[20];
char pm10Buf[20];
char pm25Buf[20];
char tempBuf[20];
char humiBuf[20];
int  pm10Val = 0;
int  pm25Val = 0;
#define TFT_WIDTH 240
#define TFT_HEIGHT 240

#define TFT_CS 5
#define TFT_RST 0
#define TFT_DC 1
#define TFT_MOSI 4  // SDA 
#define TFT_SCLK 2  // SCL

// ST7789     ESP32-C3 Super Mini
// GND        GND
// VCC        3V3
// SCL        2
// SDA        4
// RES        0
// DC         1
// CS         5
// BL         3V3

Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// -----------------------------------------------------------------------------
void setup(void) {
    Serial.begin(9600);

    memset(oldMsrdtBuf, 0x00, sizeof(oldMsrdtBuf));

    display.init(TFT_WIDTH, TFT_HEIGHT);
    display.fillScreen(ST77XX_BLACK);
    display.setTextWrap(false);

    display.setTextSize(3);
    display.setRotation(3);
    // landscape (wide) mode :
    // https://learn.adafruit.com/adafruit-gfx-graphics-library/rotating-the-display
    display.setCursor(0, 0);

    initBuf();
    connectWiFi();

    if (getDustData()) {
        displayData();
    }
}

// -----------------------------------------------------------------------------
void loop() {
    bool isApiCallOk = false;
    if ((millis() - lastTimeDust) > timerDelayDust) {
        if (WiFi.status() == WL_CONNECTED) {
            isApiCallOk = getDustData();
        } else {
            Serial.println("WiFi Disconnected");
            connectWiFi();
        }
        lastTimeDust = millis();
    }

    if (isApiCallOk) {
        displayData();
    }

    delay(5000);
}

// -----------------------------------------------------------------------------
void initBuf() {
    memset(msrdtBuf, 0x00, sizeof(msrdtBuf));
    memset(pm10Buf, 0x00, sizeof(pm10Buf));
    memset(pm25Buf, 0x00, sizeof(pm25Buf));
}

// -----------------------------------------------------------------------------
void displayData() {
    // Serial.print(oldMsrdtBuf); //degub
    // Serial.print("/"); //degub
    // Serial.println(msrdtBuf); // debug
    if (strncmp(oldMsrdtBuf, msrdtBuf, sizeof(msrdtBuf)) != 0) {
        display.fillScreen(ST77XX_BLACK);
        display.setCursor(0, 0);

        display.setTextSize(2);
        display.drawFastHLine(0, 0, display.width(), ST77XX_CYAN); //XXX
        display.println("");

        // date ---------------------------------------------------
        display.setTextSize(3);
        display.setTextColor(ST77XX_BLACK);
        char imsiBuf[10];
        memset(imsiBuf, 0x00, sizeof(imsiBuf));
        strncpy(imsiBuf, msrdtBuf + 4, 2);
        display.print("        ");
        display.setTextColor(ST77XX_MAGENTA);
        display.print(imsiBuf);
        display.print("-");
        memset(imsiBuf, 0x00, sizeof(imsiBuf));
        strncpy(imsiBuf, msrdtBuf + 6, 2);
        display.println(imsiBuf);

        // minuite ---------------------------------------------------
        display.setTextSize(5);
        display.setTextColor(ST77XX_ORANGE);
        memset(imsiBuf, 0x00, sizeof(imsiBuf));
        strncpy(imsiBuf, msrdtBuf + 8, 2);
        display.print(imsiBuf);
        display.print(":");
        memset(imsiBuf, 0x00, sizeof(imsiBuf));
        strncpy(imsiBuf, msrdtBuf + 10, 2);
        display.println(imsiBuf);

        display.setTextSize(3);
        display.drawFastHLine(0, 80, display.width(), ST77XX_CYAN); //XXX
        display.println("");

        // pm10 ------------------------------------------------------
        // 좋음0~30
        // 보통31~80
        // 나쁨81~150
        // 매우나쁨150 초과
        display.fillRoundRect(1, 98, 100, 35, 5, display.color565(128, 128, 128));
        display.setTextSize(3);
        display.setTextColor(ST77XX_BLACK);
        display.print(" PM10");
        display.print("  ");
        display.setTextSize(7);
        if (pm10Val <= 30) {
            display.setTextColor(ST77XX_BLUE);
        } else if (pm10Val <= 80) {
            display.setTextColor(display.color565(192, 192, 192)); //grey tone
        } else if (pm10Val <= 150) {
            display.setTextColor(ST77XX_ORANGE);
        } else {
            display.setTextColor(ST77XX_RED);
        }
        display.println(pm10Buf);

        display.setTextSize(3);
        display.drawFastHLine(0, 160, display.width(), ST77XX_CYAN);
        display.println("");

        // pm25 -------------------------------------------------------
        // 초미세 등급지수범위 
        //  0 좋음0~15
        //  1 보통16~35
        //  2 나쁨36~75
        //  3 매우나쁨75 초과
        display.fillRoundRect(1, 176, 100, 35, 5, display.color565(128, 128, 128));
        display.setTextSize(3);
        display.setTextColor(ST77XX_BLACK);
        display.print(" PM25");
        display.print("  ");
        display.setTextSize(7);
        if (pm25Val <= 15) {
            display.setTextColor(ST77XX_BLUE);
        } else if (pm25Val <= 35) {
            display.setTextColor(display.color565(192, 192, 192));
        } else if (pm25Val <= 75) {
            display.setTextColor(ST77XX_ORANGE);
        } else {
            display.setTextColor(ST77XX_RED);
        }
        display.println(pm25Buf);

        display.setTextSize(3);
        display.drawFastHLine(0, 239, display.width(), ST77XX_CYAN); //XXX
        display.println("");

        snprintf(oldMsrdtBuf, sizeof(oldMsrdtBuf), "%s",
            msrdtBuf);  // save old value
    } else {
        // display.fillScreen(ST77XX_BLACK);
        // display.setCursor(0, 0);
        // display.println("http error");
    }
}

// -----------------------------------------------------------------------------
void connectWiFi() {
    // WiFi.setAutoReconnect(true);  //XXX test !!!
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);

    Serial.println("Connecting ");
    display.println("Connecting ");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        display.print(".");
    }
    Serial.println("");
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());

    display.fillScreen(ST77XX_BLACK);
    display.setCursor(0, 0);
    display.println("Connected");
}

// -----------------------------------------------------------------------------
bool getDustData() {
    // Serial.println("invoke getDustData()");
    String jsonData = httpGETRequest();
    // Serial.println(jsonData); //debug
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonData);

    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());

        display.fillScreen(ST77XX_BLACK);
        display.setCursor(0, 0);
        display.setTextSize(2);
        display.print("Json failed:");
        display.println(error.f_str());
        return false;
    }

    //===========================================================
    const char* rsltCode = doc["response"]["header"]["resultCode"];
    const char* msrdt = doc["response"]["body"]["items"][0]["dataTime"]; // "2025-11-06 22:00"
    int pm10 = doc["response"]["body"]["items"][0]["pm10Value"];
    int pm25 = doc["response"]["body"]["items"][0]["pm25Value"];

    initBuf();
    snprintf(msrdtBuf, sizeof(msrdtBuf), "%s", msrdt);
    for (int src = 0, dst = 0; src < sizeof(msrdtBuf); src++) {
        if (msrdtBuf[src] != ':' && msrdtBuf[src] != '-' && msrdtBuf[src] != ' ') {
            msrdtBuf[dst++] = msrdtBuf[src];
        }
    }
    pm10Val = pm10;
    pm25Val = pm25;
    snprintf(pm10Buf, sizeof(pm10Buf), "%d", pm10);
    snprintf(pm25Buf, sizeof(pm25Buf), "%d", pm25);
    // snprintf(pm25Buf, sizeof(pm25Buf), "%.1f", pm25);
    return true;
}

// -----------------------------------------------------------------------------
String httpGETRequest() {
    Serial.println("http request...");
    int retryCnt = 0;
    while (true) {
        WiFiClient client;
        HTTPClient http;
        http.setConnectTimeout(10000);
        http.setTimeout(10000);  // milliseconds
        http.begin(client, API_URL);

        int httpResponseCode = http.GET();
        String payload = "{}";

        if (httpResponseCode > 0) {
            if (httpResponseCode == HTTP_CODE_OK) {
                Serial.print("HTTP Response code: ");
                Serial.println(httpResponseCode);
                payload = http.getString();
                http.end();  // Free resources
                // Serial.println(oldMsrdtBuf); //debug
                // Serial.println(payload); //debug
                return payload;
            } else {
                Serial.printf("(%d) http failed : %s\n",
                    retryCnt, http.errorToString(httpResponseCode).c_str());
                Serial.println(http.errorToString(httpResponseCode).c_str());

                display.fillScreen(ST77XX_BLACK);
                display.setCursor(0, 0);
                display.setTextSize(2);
                display.println("HTTP GET failed:");
                display.println(httpResponseCode);
                display.println(http.errorToString(httpResponseCode).c_str());
                http.end();  // Free resources
                memset(oldMsrdtBuf, 0x00, sizeof(oldMsrdtBuf));
            }
        } else {
            Serial.printf("(%d) - http failed: %s\n",
                retryCnt, http.errorToString(httpResponseCode).c_str());
            Serial.println(http.errorToString(httpResponseCode).c_str());

            display.fillScreen(ST77XX_BLACK);
            display.setCursor(0, 0);
            display.setTextSize(2);
            display.println("HTTP GET failed:");
            display.println(httpResponseCode);
            display.println(http.errorToString(httpResponseCode).c_str());
            http.end();  // Free resources
            memset(oldMsrdtBuf, 0x00, sizeof(oldMsrdtBuf));
        }
        delay(3000);
        Serial.println("Retrying...");
        display.println("HTTP retrying...");
        continue; //retry
    } //for 
    // Serial.println("give up !");
    // return "error";
}
