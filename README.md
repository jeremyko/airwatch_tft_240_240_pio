## What is this
This PlatformIO project combines the esp32-c3 super mini board and the inexpensive ST7789 display to show the current fine dust pollution situation (particulate matter) in Korea,
using following public API: https://www.data.go.kr/data/15073861/openapi.do

http://apis.data.go.kr/B552584/ArpltnInforInqireSvc/getMsrstnAcctoRltmMesureDnsty

## Screenshot
![01](/screenshot/esp32-project_01.jpg)

![02](/screenshot/esp32-project_02.jpg)

The current time and the elapsed time (in minutes) since the measurement values ​​were obtained are displayed, followed by the measurement values ​​for PM10 and PM2.5.

## Wiring for ESP32-C3 Super Mini
 
| ST7789 |   ESP32-C3 Super Mini |
|-|-|
| GND    |    GND |
| VCC    |    3V3 |
| SCL    |    2   |
| SDA    |    4   |
| RES    |    0   |
| DC     |    1   |
| CS     |    5   |
| BL     |    3V3 |


## Display 
[1.54" Color TFT Display Module 240x240 SPI Interface ST7789](https://ko.aliexpress.com/item/1005008625277253.html?spm=a2g0o.order_list.order_list_main.22.4abc140fw6y0zY&gatewayAdapt=glo2kor)

## How to set up

You need to apply for an API and receive an authentication key. And for the call to actually succeed, **you have to wait for a monthly server synchronization. It seems like it's usually reflected at the beginning of each month.** Yes, that's right. This is a really inconvenient process.

Users must define **Wi-Fi connection information and authentication key values ​​for API** calls in a separate file. This header file is not included in source control.

- First, **create the following file**:
    ```cpp
    include\private_defines.h
    ```

- Then, add the following to the file and change it to your own information
    ```cpp
    #ifndef PRIVATE_DEFINES_H 
    #define PRIVATE_DEFINES_H

    // XXX NOTE : Only 2.4 GHz wifi is supported !
    const char* WIFI_SSID = "YOURS";
    const char* WIFI_PASSWD = "YOURS";
    const char* API_URL =
        "http://apis.data.go.kr/B552584/ArpltnInforInqireSvc/getMsrstnAcctoRltmMesureDnsty?returnType=json&numOfRows=1&pageNo=1&dataTerm=DAILY&ver=1.3&"
        "stationName=YOURS&"
        "serviceKey=YOURS";

    // stationName => Example) "관악구" (URL must be encoded)
    // eg) stationName=%EA%B4%80%EC%95%85%EA%B5%AC
    // serviceKey => Authentication key received when requesting API

    #endif 
    ```
