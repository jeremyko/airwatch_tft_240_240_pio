## What is this

This crude hobby project combines the esp32-c3 super mini board and the inexpensive ST7789 display to show the current fine dust pollution situation (particulate matter) in Korea, using following public API: 

https://www.data.go.kr/data/15073861/openapi.do
http://apis.data.go.kr/B552584/ArpltnInforInqireSvc/getMsrstnAcctoRltmMesureDnsty

## Screenshot
![001](/screenshot/esp32-project_001.jpg)

It's a mess of soldering.

![002](/screenshot/esp32-project_002.jpg)

#### Instead of soldering directly to the board, I used a PCB Female Pin Header.
![003](/screenshot/esp32-project_003.jpg)
![004](/screenshot/esp32-project_004.jpg)
![01](/screenshot/esp32-project_01.jpg)
![02](/screenshot/esp32-project_02.jpg)

#### I built the enclosure with cheap foamboard and a hot-melt glue gun. Yeah, it's not very pretty.
![03](/screenshot/esp32-project_03.jpg)

There was no blueprint or anything from the beginning. It was a kind of 'vibe crafting'.

![04](/screenshot/esp32-project_04.jpg)

The current time is displayed, followed by the measurement values ​​for PM10 and PM2.5. 
Rather than displaying the exact two measurements numerically, I decided it was more important to design the UI so that they could be immediately identified by color. So, I created two large boxes with corresponding colors for each measurement, making them easily identifiable.

If there is a delay in retrieving measurements by calling the API, a red warning rectangle and message will be displayed next to the current time.

![delay01](/screenshot/delay_01.jpg)
![delay02](/screenshot/delay_02.jpg)

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

## ESP32-C3 Super Mini 
[ESP32-C3 Super Mini Board](https://ko.aliexpress.com/item/1005008133819792.html?spm=a2g0o.order_list.order_list_main.92.4752140fsnwYLm&gatewayAdapt=glo2kor)

## Display 
[1.54" Color TFT Display Module 240x240 SPI Interface ST7789](https://ko.aliexpress.com/item/1005008625277253.html?spm=a2g0o.order_list.order_list_main.22.4abc140fw6y0zY&gatewayAdapt=glo2kor)

## Perfboard
[Prototype Paper PCB Universal Experiment Matrix Circuit Board](https://ko.aliexpress.com/item/1005003370482974.html?spm=a2g0o.order_list.order_list_main.22.4752140fsnwYLm&gatewayAdapt=glo2kor)
## How to set up

You need to apply for an API and receive an authentication key. And for the call to actually succeed, **you have to wait for a monthly server synchronization. It seems like it's usually reflected at the beginning of each month.** Yes, that's right. This is a really inconvenient process.

Users must define **Wi-Fi connection information and authentication key values ​​for API** calls in a separate file. This header file is not included in source control.

- First, copy the `private_defines template.h` template file from the include folder and rename it to `private_defines.h`.

- Then, change the `YOURS` part of the file contents to your own information.
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
