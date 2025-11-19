#ifndef PRIVATE_DEFINES_H 
#define PRIVATE_DEFINES_H

// Copy this file, rename it to "private_defines.h", 
// and change the "YOURS" section to your own information.

const char* WIFI_SSID = "YOURS";
const char* WIFI_PASSWD = "YOURS";
// 데이터포털 : https://www.data.go.kr/iim/api/selectAPIAcountView.do
// 측정소별 실시간 측정정보 조회 : stationName => 금천구, serviceKey => 인증키 
// hours : 1~24 (1시간~24시간)
const char* API_URL =
"http://apis.data.go.kr/B552584/ArpltnInforInqireSvc/getMsrstnAcctoRltmMesureDnsty?returnType=json&numOfRows=1&pageNo=1&dataTerm=DAILY&ver=1.3&"
"stationName=YOURS&"
"serviceKey=YOURS";

// {
//     "response": {
//         "body": {
//             "totalCount": 23,
//                 "items" : [
//             {
//                 "pm25Grade1h": "3",
//                     "pm10Value24" : "62",
//                     "so2Value" : "0.003",
//                     "pm10Grade1h" : "2",
//                     "o3Grade" : "1",
//                     "pm10Value" : "66",
//                     "pm25Flag" : null,
//                     "khaiGrade" : "2",
//                     "pm25Value" : "49",
//                     "no2Flag" : null,
//                     "mangName" : "도시대기",
//                     "no2Value" : "0.049",
//                     "so2Grade" : "1",
//                     "coFlag" : null,
//                     "khaiValue" : "100",
//                     "coValue" : "0.7",
//                     "pm10Flag" : null,
//                     "no2Grade" : "2",
//                     "pm25Value24" : "35",
//                     "o3Flag" : null,
//                     "pm25Grade" : "2",
//                     "so2Flag" : null,
//                     "coGrade" : "1",
//                     "dataTime" : "2025-11-06 22:00",
//                     "pm10Grade" : "2",
//                     "o3Value" : "0.005"
//             }
//                 ],
//                 "pageNo": 1,
//                 "numOfRows" : 1
//         },
//             "header": {
//             "resultMsg": "NORMAL_CODE",
//             "resultCode" : "00"
//         }
//     }
// }
#endif 