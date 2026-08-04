/*******************************************************************************
  * @file       MAIN FUNCTION PROTOTYPES      
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
#include "string.h"
#include "math.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "stdlib.h"
#include "osi.h"                        //Free-RTOS includes

#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_sleep.h"

#include "cJSON.h"                      //JSON Parser
#include "JsonParse.h"

#include "iic.h"                        //user driver header
#include "user_spi.h"
#include "sht30dis.h"
#include "lightsensor.h"
#include "ub_dt_p1.h"
#include "at24c32.h"
#include "w25q128.h"
#include "PCF8563.h"
#include "MsgType.h"
#include "HttpClientTask.h"
#include "power_adc.h"
#include "app_config.h"
#include "STK8323.h"

#define TAG "MAIN"

/*******************************************************************************
//Global Variables
*******************************************************************************/
EventGroupHandle_t Nets_Group;
OsiSyncObj_t SW1_Binary;  //For Button2 interrupt task
OsiMsgQ_t Data_Queue;  //Used for cjson and memory save

bool sw_wakeup=0;
bool dev_power_on = 0;  //device power on

void SET_GREEN_LED_ON(void)      
{
  gpio_set_level(led1_pin, 1);
}
void SET_GREEN_LED_OFF(void)     
{
  gpio_set_level(led1_pin, 0);
}
void SET_RED_LED_ON(void)        
{
  gpio_set_level(led2_pin, 1);
}
void SET_RED_LED_OFF(void)       
{
  gpio_set_level(led2_pin, 0);
}

/*******************************************************************************
//GREEN LED Flashed when Post Task 
*******************************************************************************/
void G_Led_Task(void *pvParameters)
{
  for(;;)
  {
    SET_GREEN_LED_ON();
    osi_Sleep(500);  //delay 500ms
      
    SET_GREEN_LED_OFF();
    osi_Sleep(500);  //delay 500ms
  }
}

/*******************************************************************************
//Button1 Interrupt Application Task
*******************************************************************************/
void SW1_Task( void *pvParameters )
{
  for(;;)
  {
    osi_SyncObjWait(&SW1_Binary,OSI_WAIT_FOREVER);  //Waite Button GPIO Interrupt Message 

    osi_Sleep(10);
    if((gpio_get_level(sw1int_pin))||(sw_wakeup==1))
    {
      sw_wakeup=0;
      buzzer_makeSound(200);
    }
    osi_SyncObjClear(&SW1_Binary);  //clear task message
  }
}

/*******************************************************************************
//GPIO or Timer Wake Up Process
*******************************************************************************/
void WakeUp_Process(void)
{
  uint64_t wakeup_pin_mask;
  unsigned long ret_val = esp_sleep_get_wakeup_cause();
  switch(ret_val)
  {
    case ESP_SLEEP_WAKEUP_EXT1:
    {
      wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
      if(((wakeup_pin_mask>>sw1int_pin)&0x01)&&(dev_power_on==0))
      {
        ESP_LOGI(TAG, "%d,sw1 int", __LINE__);
        sw_wakeup = 1;
        osi_SyncObjSignalFromISR(&SW1_Binary);
      }
    }
    break;
    default:
    break;
  }
}

/*******************************************************************************
//Enter_Sleep
*******************************************************************************/
void Enter_Sleep(unsigned long slp_time)
{
  ESP_LOGI(TAG,"slp_time=%ld", slp_time);
  esp_sleep_enable_timer_wakeup(slp_time * 1000000);  //
  const int ext_wakeup_pin_1 = sw1int_pin;
  const uint64_t ext_wakeup_pin_1_mask = 1ULL << ext_wakeup_pin_1;
  esp_sleep_enable_ext1_wakeup(ext_wakeup_pin_1_mask, ESP_EXT1_WAKEUP_ANY_HIGH);
  ESP_LOGI(TAG, "SET BUTTON1 wakeup");
	esp_deep_sleep_start();
}

/*******************************************************************************
//Wireless net work tasks
*******************************************************************************/
void Main_Task(void *pvParameters)
{
  short net_resp=-1; 
  float temp_value=0,humi_value=0;
  float lightvalue=0;
  float p_value=0;
  float temp_val_1=0,temp_val_2=0;
  SensorMessage sMsg={0};
  for(;;)
  {
    ESP_LOGI(TAG, "%d,Main_Task.", __LINE__);

    sMsg.ts = Read_UnixTime();
    if(sMsg.ts<1767196800)  //2026-1-1 
    {
      net_resp = WiFi_Net_Application(UPDATE_TIME_MODE);
      if(net_resp == SUCCESS)
      {
        ESP_LOGI(TAG, "%d,update time success.", __LINE__);
      }
      else
      {
        ESP_LOGI(TAG, "%d,update time failed,code=%d.", __LINE__,net_resp);
      }
      sMsg.ts = Read_UnixTime();
    }
    sht30_SingleShotMeasure(&temp_value,&humi_value);  //read temperature humility data
    ESP_LOGI(TAG, "%d,temp=%.4f,humi=%.4f", __LINE__,temp_value,humi_value);
    if((temp_value!=ERROR_CODE)&&(humi_value!=ERROR_CODE))
    {
      sMsg.sensornum=TEMP_NUM;  //Message Number
      sMsg.sensorval=temp_value;  //Message Value
      osi_MsgQWrite(&Data_Queue,&sMsg,OSI_SAVE_WAIT);  //Send Humility Data Message
      sMsg.sensornum=HUMI_NUM;  //Message Number
      sMsg.sensorval=humi_value;  //Message Value
      osi_MsgQWrite(&Data_Queue,&sMsg,OSI_SAVE_WAIT);  //Send Humility Data Message
    }

    LightSensor_value(&lightvalue);  //Read Light Value
    ESP_LOGI(TAG, "%d,light=%.4f", __LINE__,lightvalue);
    if(lightvalue!=ERROR_CODE)
    {
      sMsg.sensornum=LIGHT_NUM;         //Message Number
      sMsg.sensorval=lightvalue;        //Message Value
      osi_MsgQWrite(&Data_Queue,&sMsg,OSI_SAVE_WAIT);   //Send Light Data Message
    }

    p_value=power_adcValue();  //Read Noise Value
    if(p_value!=ERROR_CODE)
    {
      sMsg.sensornum=BAT_NUM;  //Message Number
      sMsg.sensorval=p_value;  //Message Value
      osi_MsgQWrite(&Data_Queue,&sMsg,OSI_SAVE_WAIT);  //send power data message
    }

    ub_dt_p1_get_temp(&temp_val_1,&temp_val_2);  //measure the temperature
    if(temp_val_1 != ERROR_CODE)
    {
      sMsg.sensornum=EXT1_TEMP_NUM;  //Message Number
      sMsg.sensorval=temp_val_1;  //Message Value
      osi_MsgQWrite(&Data_Queue,&sMsg,OSI_SAVE_WAIT);   //send noise data message
    }
    if(temp_val_2 != ERROR_CODE)
    {
      sMsg.sensornum=EXT2_TEMP_NUM;  //Message Number
      sMsg.sensorval=temp_val_2;  //Message Value
      osi_MsgQWrite(&Data_Queue,&sMsg,OSI_SAVE_WAIT);   //send noise data message
    }

    net_resp = WiFi_Net_Application(DATA_POST_MODE);
    if(net_resp == SUCCESS)
    {
      ESP_LOGI(TAG, "%d,data post success.", __LINE__);
    }
    else
    {
      ESP_LOGI(TAG, "%d,data post failed,code=%d.", __LINE__,net_resp);
    }

    Enter_Sleep(DEFAULT_FN);
  }
}

/*******************************************************************************
// MAIN FUNCTION                                
*******************************************************************************/
void app_main(void)
{
  esp_err_t ret = nvs_flash_init();  //Initialize NVS
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
  {
    ret = nvs_flash_erase();
    if(ret!=ESP_OK)
    {
      ESP_LOGI(TAG, "%dv,nvs_flash_erase failed.", __LINE__);
    }
    ret = nvs_flash_init();
  }
  if(ret!=ESP_OK)
  {
    ESP_LOGI(TAG, "%dv,nvs_flash_init failed.", __LINE__);
  }

  Nets_Group = xEventGroupCreate();
  xEventGroupClearBits(Nets_Group, Nets_Group_ALL_BIT);
  osi_SyncObjCreate(&SW1_Binary);  //Button1 Interrupt Task //xBinary1
  osi_MsgQCreate(&Data_Queue,"Data_Queue",sizeof(SensorMessage),USR_POST_DATA_SUM);   //create queue used for sensor value save

  PinMuxConfig();  //Configure The Peripherals
  I2C_Init();  //Configuring IIC Bus

  unsigned long ulResetCause = esp_reset_reason();
  ESP_LOGI(TAG, "%d,%d", __LINE__,(int)ulResetCause);
  if (ulResetCause == ESP_RST_POWERON) //power on wakeup
  {
    dev_power_on = 1;
    SET_GREEN_LED_ON();
    buzzer_makeSound(200);
    SET_GREEN_LED_OFF();  //Green led off

    Timer_IC_Init();  //PCF8563 init 
    Timer_IC_Reset_Time();  //PCF8563 Reset Time 2018-01-01 00:00:00
    LightSensor_Init();  // light sensor init 
  }
  WakeUp_Process();

  osi_TaskCreate(SW1_Task, NULL,1024,NULL, 9, NULL);  //Create Button1 Interrupt_Task 
  osi_TaskCreate(G_Led_Task, NULL,1024, NULL, 7,NULL);  //Create GREEN LED Blink Task When Internet Application
  osi_TaskCreate(Main_Task, NULL,8192,NULL, 5, NULL);  //Create net Task
}

/*******************************************************************************
                                      END         
*******************************************************************************/




