/*******************************************************************************
  * @file           
  * @author 
  * @version
  * @date 
  * @brief
  ******************************************************************************
  * @attention
  *
  *
*******************************************************************************/

/*-------------------------------- Includes ----------------------------------*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#ifndef __MSG_TYPE_H__
#define __MSG_TYPE_H__

#define SUCCESS 0
#define FAILURE -1

#define USR_POST_DATA_SUM 20

#define USR_PID           "ubibot-ws1b"
#define USR_SN            "RV41554WS1B"
#define USR_SSID          "CF"
#define USR_PASSWORD      "cf86686675"
#define USR_HTTP_HOST     "192.168.2.71"
#define USR_HTTP_PORT     8080
#define USR_HTTP_TIME_URL "/api/v1/auth/time"
#define USR_HTTP_DATA_URL "/api/v1/data/report"
#define UPDATE_TIME_MODE  0x01
#define DATA_POST_MODE    0x02
#define HTTP_TX_BUFF_LEN  4096
#define HTTP_RX_BUFF_LEN  4096

#define TEMP_NUM        1
#define HUMI_NUM        2
#define LIGHT_NUM       3
#define BAT_NUM         4
#define RSSI_NUM        5
#define EXT1_TEMP_NUM   6
#define EXT2_TEMP_NUM   7

#define DEFAULT_FN            300
#define DEFAULT_WIFI_BAND     0
#define DEFAULT_WIFI_CNT_TIMEOUT  30

typedef struct
{
  unsigned long ts;
  uint8_t sensornum;    //sensor field
  float   sensorval;    //sensor value
} SensorMessage;

typedef struct
{
  unsigned long  fn_th;         //Temp&Humi sensor frequence
  unsigned long  fn_th_t;       //Temp&Humi sensor frequence
  unsigned long  fn_light;      //Light sensor frequence
  unsigned long  fn_light_t;    //Light sensor frequence
  unsigned long  fn_ext;        // Temp sensor Measure frequence
  unsigned long  fn_ext_t;      // Temp sensor Measure frequence
  unsigned long  fn_battery;    //Power Measure frequence
  unsigned long  fn_battery_t;  //Power Measure frequence
  unsigned long  fn_acc;
  unsigned long  fn_acc_t;
  unsigned long  fn_dp;         //data post frequence
  unsigned long  fn_dp_t;       //data post frequence
}Metadata_Struct;

#endif

/*******************************************************************************
                                      END         
*******************************************************************************/