#include <Arduino.h>
#include <Adafruit_GFX.h>     
#include <Adafruit_ST7789.h>  
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <iostream>
#include <time.h>
#include "esp_sntp.h"

#include <thread>
#include <chrono>
#include <memory>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "private_defines.h" 

// -----------------------------------------------------------------------------
// pins
// ST7789     ESP32-C3 Super Mini
// ------     -------------------
// GND        GND
// VCC        3V3
// SCL        2
// SDA        4
// RES        0
// DC         1
// CS         5
// BL         3V3
#define TFT_RST 0
#define TFT_DC 1
#define TFT_SCLK 2  // SCL
#define TFT_MOSI 4  // SDA 
#define TFT_CS 5
// -----------------------------------------------------------------------------

#define TFT_WIDTH 240
#define TFT_HEIGHT 240
#define ONE_MINUTE 60000
#define FIVE_MINUTES (ONE_MINUTE * 5)

#ifdef DEBUG_LOGGING
#define  LOG_WHERE "("<<__FILE__<<"-"<<__FUNCTION__<<"-"<<__LINE__<<") "
#define  DLOG(x)  std::cout<<LOG_WHERE << x << "\n"
#else
#define  LOG_WHERE 
#define  DLOG(x)  
#endif
#define  ELOG(x)  std::cerr<<LOG_WHERE << x << "\n"

void init_mem();
void display_data();
void connect_wifi();
bool build_dust_data();
String http_get();
void init_sntp();
void get_current_time();
int get_measured_hour();
void increase_reserved_hour();
void clock_task(void* parameter);

const char* g_ntp_server = "pool.ntp.org";
const char* g_timezone = "KST-9"; // UTC+9 , south korea
char g_measure_dt[20];
int  g_pm10 = 0;
int  g_pm25 = 0;
int g_min_ago = -1; // 측정된 시간과 현재 시간과의 차이(분)
int g_current_hour = -1;
int g_current_minute = -1;
int g_current_sec = -1;
int g_reserved_hour = -1; // API 호출 후, 다음 호출할 시간대
char g_current_mon_day[10]; // month-day 저장용
char g_current_hour_min[10]; // hour-minute 저장용

// -----------------------------------------------------------------------------
// XXX task 에서 g_tft 객체를 사용하고 있으므로, 접근시 mutex 로 보호 필요함 
SemaphoreHandle_t g_display_mutex;
Adafruit_ST7789 g_tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// -----------------------------------------------------------------------------
void setup(void) {

    g_display_mutex = xSemaphoreCreateMutex(); // Create the mutex semaphore

    Serial.begin(9600);

    g_tft.init(TFT_WIDTH, TFT_HEIGHT);
    g_tft.fillScreen(ST77XX_BLACK);
    g_tft.setTextWrap(false);

    g_tft.setTextSize(3);
    g_tft.setRotation(0);
    // g_tft.setRotation(3); // landscape (wide) mode :
    // https://learn.adafruit.com/adafruit-gfx-graphics-library/rotating-the-g_tft
    g_tft.setCursor(0, 0);

    init_mem();
    connect_wifi();
    init_sntp();

    // onboard led off
    gpio_reset_pin(GPIO_NUM_8);
    gpio_set_direction(GPIO_NUM_8, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_8, 1);

    if (build_dust_data()) { // blocking call 
        get_current_time(); // 위 함수 호출에서 blocking 된 경우, 현재 hour가 변경될수 있다.
        DLOG("(setup) API called success : " << g_measure_dt);
        if (get_measured_hour() == g_current_hour) {
            // 실제 측정 시간이 현재 시간인 경우 
            // -> 현 시간의 측정값을 가져온 경우에만 다음 예약 시간을 갱신한다.
            // API 호출시 매시 정각에 측정 데이터가 즉시 갱신되지 않으므로 체크 필요
            increase_reserved_hour();
        }
        // 현재 시간 데이터가 아니더라도 최초 한번은 반드시 화면에 표시한다. 
        display_data();
    }

    if (g_display_mutex != NULL) {
        xTaskCreatePinnedToCore(
            clock_task,       // Task function
            "clock_task",     // Name of the task
            10000,        // Stack size in words (adjust as needed)
            NULL,         // Parameter to pass to the task
            1,            // Priority of the task (0 is lowest)
            NULL,         // Task handle (can be NULL if not needed)
            0             // Core to run the task on (0 or 1)
        );
    } else {
        ELOG("Failed to create mutex semaphore");
    }

    delay(FIVE_MINUTES); // 데이터 출력 완료 상태. loop 처리 전,5분 대기
}

// ----------------------------------------------------------------------------- clock task
// 현재 시간 정보를 표시한다. 
void clock_task(void* parameter) {

    // DLOG("task : core id --> " << xPortGetCoreID()); //0
    uint16_t gray_color = g_tft.color565(128, 128, 128);
    char temp_buf[10];
    bool is_first = true;

    while (true) {
        if (xSemaphoreTake(g_display_mutex, portMAX_DELAY) == pdTRUE) {
            get_current_time();
            //----------------------------- 매초 빈번하게 갱신 안되게 처리한다
            bool need_update = false;
            if (g_current_sec >= 0 && g_current_sec <= 3) {
                need_update = true;
            }
            if (is_first) {
                is_first = false;
                need_update = true;
            }

            if (need_update) {
                g_tft.fillRect(0, 0, g_tft.width(), 15, ST77XX_BLACK); // 시간 표시 ROW 영역 클리어
                g_tft.setCursor(2, 0);
                g_tft.setTextSize(2);
                g_tft.setTextColor(ST77XX_MAGENTA);
                g_tft.print(g_current_mon_day);
                g_tft.print(" ");
                g_tft.println(g_current_hour_min);
            }

            // g_tft.fillRect(130, 0, 100, 15, ST77XX_YELLOW); // minute 영역만 클리어
            // 너무 오래 지연되는 경우, delayed minute 표시
            if (g_min_ago >= 80) {
                g_tft.fillRoundRect(150, 0, 85, 15, 2, ST77XX_RED); // red box 표시 

                g_tft.setCursor(50, 16);
                g_tft.setTextColor(ST77XX_RED);
                snprintf(temp_buf, sizeof(temp_buf), "%4d", g_min_ago);
                g_tft.print(temp_buf);
                //----------------------------- 
                g_tft.setCursor(100, 16);
                g_tft.println(" min delay");
            } else {
                g_tft.fillRoundRect(150, 0, 85, 15, 2, ST77XX_BLACK); // red box 지움
                g_tft.fillRect(0, 16, g_tft.width(), 15, ST77XX_BLACK); // delay min 표시 ROW 영역 클리어
            }

            xSemaphoreGive(g_display_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// -----------------------------------------------------------------------------
// api 호출 빈도를 최소화 한다. 
// 현재 시간 데이터를 가져왔으면 다음 시간까지 호출없게 처리 
void loop() {
    // DLOG("loop : core id --> " << xPortGetCoreID()); //0
    get_current_time();
    DLOG("-- loop current time : " << g_current_hour << ":" << g_current_minute
        << " / reserved_hour : " << g_reserved_hour);

    if (g_reserved_hour == 0 && g_current_hour == 23) {
        // 현재 23시에 0시 예약된 경우 매 5분마다 호출 안되게 처리
        // 이미 23시 데이터는 가져온 상태이므로, 더이상 호출 안함
        delay(FIVE_MINUTES);
        return;
    } else if (g_reserved_hour <= g_current_hour) {
        // 한시간동안 api 호출해도 데이터 얻지 못한경우 고려해서 <= 조건 사용 
        if (build_dust_data()) { // blocking call 
            get_current_time(); // 위 함수 호출에서 blocking 된 경우, 현재 hour가 변경될수 있다.
            DLOG("API called success : " << g_measure_dt);
            if (get_measured_hour() == g_current_hour) {
                display_data(); // 화면 갱신
                increase_reserved_hour();
            }
        }
    }
    // 하루에 500 번만 api 호출가능, 1시간에 20번. 
    delay(FIVE_MINUTES);
}

// -----------------------------------------------------------------------------
// 현재 시간 기준, 1 시간 이후로 api 호출 예약
void increase_reserved_hour() {
    if (g_current_hour == 23) {
        g_reserved_hour = 0;
    } else {
        g_reserved_hour = g_current_hour + 1;
    }
    DLOG("   +++ increased reserved_hour:" << g_reserved_hour);
}

// -----------------------------------------------------------------------------
int get_measured_hour() {
    char temp_hour[10];
    memset(temp_hour, 0x00, sizeof(temp_hour));
    strncpy(temp_hour, g_measure_dt + 8, 2); // 202511131800
    int meas_hour = atoi(temp_hour); // 이값은 정확하게 매시간 마다 갱신되지 않는다 
    if (meas_hour == 24) {
        // api 호출 시, 측정된 시간값은 1-24 hour 값으로 반환된다.
        // 실제 시간은 0-23 hour 이므로
        // 0으로 변경해서 시간 비교할수 있도록 한다.
        meas_hour = 0;
    }
    // DLOG("meas_hour:" << meas_hour);
    return meas_hour;
}

// -----------------------------------------------------------------------------
void init_sntp() {

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Disconnected");
        connect_wifi();
    }

    //configTime(gmt_offset_sec, daylight_offset_sec, ntp_server); // 사용하지 않는다. return 값 체크 어려움.
    if (esp_netif_init() != ESP_OK) {
        // XXX 위 함수에 이런 comments 가 있다. - -;
        // --> This function should be called exactly once from application code, 
        // when the application starts up.
        ELOG("esp_netif_init() failed");
        while (true) {
            //XXX 이 부분은 명확하지 않다. error발생시 어떻게 처리할 것인가?
            Serial.print(".");
        }
    }
    if (sntp_enabled()) {
        sntp_stop();
    }
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, (char*)g_ntp_server);
    sntp_init();
    setenv("TZ", g_timezone, 1);
    // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
    // POSIX format : UTC offset 과 부호가 반대임.
    // UTC+9는 ==> UTC-9 를 사용해야 한다.

    tzset();
    DLOG("Time synchronized");
}

// -----------------------------------------------------------------------------
void get_current_time() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    // g_current_month = timeinfo.tm_mon + 1;
    // g_current_day = timeinfo.tm_mday;
    g_current_hour = timeinfo.tm_hour;
    g_current_minute = timeinfo.tm_min;
    g_current_sec = timeinfo.tm_sec;
    if (g_current_hour == 0 && get_measured_hour() == 23) {
        // 자정이 넘어간 경우, 측정된 시간이 23시인 경우 고려
        g_min_ago = atoi(g_measure_dt + 10) - g_current_minute;
    } else {
        g_min_ago = (g_current_hour * 60 + g_current_minute) - (get_measured_hour() * 60 + atoi(g_measure_dt + 10));
    }
    // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
    snprintf(g_current_mon_day, sizeof(g_current_mon_day), "%02d-%02d", timeinfo.tm_mon + 1, timeinfo.tm_mday);
    snprintf(g_current_hour_min, sizeof(g_current_hour_min), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    // DLOG("-- current time : " << g_current_mon_day << " " << g_current_hour_min << " / min_ago:" << g_min_ago);
}

// -----------------------------------------------------------------------------
void init_mem() {
    memset(g_measure_dt, 0x00, sizeof(g_measure_dt));
}

// -----------------------------------------------------------------------------
// api 호출로 얻은 날자 정보는 자정이 넘어가면 어제 24:00 형식으로 온다.
// ex) 현재 시간 2025-11-15 00:00 ==> api는 2025-11-14 24:00 형식으로 옴.
void display_data() {
    uint16_t gray_color = g_tft.color565(128, 128, 128);
    if (xSemaphoreTake(g_display_mutex, portMAX_DELAY) == pdTRUE) {
        DLOG("---- display data");
        char imsiBuf[10];

        // g_tft.fillScreen(ST77XX_BLACK);
        g_tft.fillRect(0, 30, g_tft.width(), 210, ST77XX_BLACK);

        //------------------------------------------------------ line
        // g_tft.drawFastHLine(0, 80, g_tft.width(), gray_color);

        //------------------------------------------------------ pm10
        // 좋음0~30
        // 보통31~80
        // 나쁨81~150
        // 매우나쁨150 초과

        // label display
        uint16_t pm10_color;
        g_tft.setCursor(10, 40);
        if (g_pm10 <= 30) {
            pm10_color = ST77XX_BLUE;
            g_tft.setTextColor(ST77XX_WHITE);
        } else if (g_pm10 <= 80) {
            pm10_color = ST77XX_GREEN; // normal
            g_tft.setTextColor(ST77XX_BLACK);
        } else if (g_pm10 <= 150) {
            pm10_color = ST77XX_ORANGE;
            g_tft.setTextColor(ST77XX_BLACK);
        } else {
            pm10_color = ST77XX_RED;
            g_tft.setTextColor(g_tft.color565(255, 224, 143));
        }
        g_tft.fillRoundRect(0, 32, g_tft.width(), 100, 8, pm10_color); // color box

        g_tft.setCursor(10, 42);
        g_tft.setTextSize(3);
        g_tft.print("PM 10");

        // value display
        g_tft.setCursor(110, 68);
        g_tft.setTextSize(7);
        // if (g_pm10 <= 30) {
        //     g_tft.setTextColor(ST77XX_BLUE);
        // } else if (g_pm10 <= 80) {
        //     g_tft.setTextColor(gray_color);
        // } else if (g_pm10 <= 150) {
        //     g_tft.setTextColor(ST77XX_ORANGE);
        // } else {
        //     g_tft.setTextColor(ST77XX_RED);
        // }
        snprintf(imsiBuf, sizeof(imsiBuf), "%3d", g_pm10);
        g_tft.println(imsiBuf);

        //------------------------------------------------------ line
        // g_tft.drawFastHLine(0, 160, g_tft.width(), gray_color);

        //------------------------------------------------------ pm25
        //  0 좋음0~15
        //  1 보통16~35
        //  2 나쁨36~75
        //  3 매우나쁨75 초과

        // label display
        uint16_t pm25_color;
        if (g_pm25 <= 15) {
            pm25_color = ST77XX_BLUE;
            g_tft.setTextColor(ST77XX_WHITE);
        } else if (g_pm25 <= 35) {
            pm25_color = ST77XX_GREEN; // normal
            g_tft.setTextColor(ST77XX_BLACK);
        } else if (g_pm25 <= 75) {
            pm25_color = ST77XX_ORANGE;
            g_tft.setTextColor(ST77XX_BLACK);
        } else {
            pm25_color = ST77XX_RED;
            g_tft.setTextColor(g_tft.color565(255, 224, 143));
        }
        // g_tft.fillRoundRect(1, 170, 85, 42, 5, pm25_color);
        g_tft.fillRoundRect(0, 136, g_tft.width(), 100, 8, pm25_color); //color box
        g_tft.setCursor(10, 146);
        g_tft.setTextSize(3);
        g_tft.print("PM 2.5");

        // value display
        g_tft.setCursor(110, 172);
        g_tft.setTextSize(7);
        // if (g_pm25 <= 15) {
        //     g_tft.setTextColor(ST77XX_BLUE);
        // } else if (g_pm25 <= 35) {
        //     g_tft.setTextColor(gray_color);
        // } else if (g_pm25 <= 75) {
        //     g_tft.setTextColor(ST77XX_ORANGE);
        // } else {
        //     g_tft.setTextColor(ST77XX_RED);
        // }
        snprintf(imsiBuf, sizeof(imsiBuf), "%3d", g_pm25);
        g_tft.println(imsiBuf);

        //------------------------------------------------------ line
        // g_tft.drawFastHLine(0, 239, g_tft.width(), gray_color); //XXX

        xSemaphoreGive(g_display_mutex);
    }
}

// -----------------------------------------------------------------------------
void connect_wifi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);

    Serial.println("Connecting ");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());
}

// -----------------------------------------------------------------------------
bool build_dust_data() {

    //----------------------- XXX TEST
    // g_pm10 = 50;
    // g_pm25 = 30;
    // init_mem();
    // snprintf(g_measure_dt, sizeof(g_measure_dt), "%s", "202511180000");
    // return true;
    //-----------------------

    String jsonData = http_get(); //blocking call

    // DLOG(jsonData); 
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonData);

    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        ELOG(error.c_str());

        return false;
    }

    // const char* rslt_code = doc["response"]["header"]["resultCode"];
    const char* measure_dt = doc["response"]["body"]["items"][0]["dataTime"]; // "2025-11-06 22:00"
    g_pm10 = doc["response"]["body"]["items"][0]["pm10Value"];
    g_pm25 = doc["response"]["body"]["items"][0]["pm25Value"];

    init_mem();
    snprintf(g_measure_dt, sizeof(g_measure_dt), "%s", measure_dt);
    for (int src = 0, dst = 0; src < sizeof(g_measure_dt); src++) {
        if (g_measure_dt[src] != ':' && g_measure_dt[src] != '-' && g_measure_dt[src] != ' ') {
            g_measure_dt[dst++] = g_measure_dt[src];
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
// api 호출시 에러 발생하는 경우 계속 재시도한다. 결과를 얻을때까지 blocking 된다.
String http_get() {

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Disconnected");
        connect_wifi();
    }

    int retryCnt = 0;
    String payload = "{}";
    while (true) {
        DLOG("************ HTTP Request ************");
        WiFiClient client;
        HTTPClient http;
        http.setConnectTimeout(20000);
        http.setTimeout(20000);  // milliseconds
        if (!http.begin(client, API_URL)) {
            http.end();

            DLOG("http err(begin) / retry : cnt->" << retryCnt);
            retryCnt++;
            delay(FIVE_MINUTES);
            continue;
        };

        int httpResponseCode = http.GET();

        if (httpResponseCode > 0) {
            if (httpResponseCode == HTTP_CODE_OK) {
                Serial.print("HTTP Response code: ");
                Serial.println(httpResponseCode);
                payload = http.getString();
                http.end();
                // Serial.println(payload); //debug
                return payload;
            } else {

                if (HTTP_CODE_TOO_MANY_REQUESTS == httpResponseCode) {
                    Serial.printf("(%d)http err:Too Many Requests\n", retryCnt);
                    DLOG("API rate limit exceeded)");
                } else {
                    Serial.printf("(%d)http err:%s\n",
                        retryCnt, http.errorToString(httpResponseCode).c_str());
                    Serial.println(httpResponseCode);
                }
                http.end();
            }
        } else {
            Serial.printf("(%d)-http err:%s\n",
                retryCnt, http.errorToString(httpResponseCode).c_str());
            Serial.println(httpResponseCode);

            http.end();
        }
        DLOG("http retry : cnt->" << retryCnt);
        retryCnt++;

        delay(FIVE_MINUTES);
    }
}
