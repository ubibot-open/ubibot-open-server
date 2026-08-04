/*******************************************************************************
  * @file       Light Sensor DRIVER APPLICATION      
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
#include <stdlib.h>
#include "osi.h"
#include "lightsensor.h"
#include "iic.h"
#include "math.h"
#include "esp_log.h"

#define TAG "light sensor"

/*******************************************************************************
  write ltr308 light sensor register
*******************************************************************************/
static void LTR308_WriteReg(uint8_t regaddr,uint8_t val)
{
  IIC_Write_Reg(LTR308_ADDR,regaddr,val);     //write data
}

/*******************************************************************************
  read write ltr308 light sensor register
*******************************************************************************/
static uint8_t LTR308_ReadReg(uint8_t regaddr)
{
  uint8_t regdata;
  IIC_Read_Reg(LTR308_ADDR,regaddr,&regdata); //read data
  return regdata; 
}

/*******************************************************************************
  opt3001 light sensor init
*******************************************************************************/
int LightSensor_Init(void)
{
  if(LTR308_ReadReg(LTR308_PART_ID)==LTR308_PARTID)
  {
    ESP_LOGI(TAG, "%d,LTR308 PARTID=%02x\r\n", __LINE__,LTR308_PARTID);
    LTR308_WriteReg(LTR308_MAIN_CTRL,0x00);  //ALS standby
    return SUCCESS;
  }
  return FAILURE;	   					
}

/*******************************************************************************
//measure the light value
*******************************************************************************/
void LightSensor_value(float *lightvalue)
{
  uint16_t retry;

  *lightvalue = ERROR_CODE;
  if(LTR308_ReadReg(LTR308_PART_ID)==LTR308_PARTID)
  {
    ESP_LOGI(TAG, "%d,LTR308 PARTID=%02x\r\n", __LINE__,LTR308_PARTID);

    uint8_t reg_val[3]={0};
    LTR308_WriteReg(LTR308_MAIN_CTRL,0x02);  //ALS active
    LTR308_WriteReg(LTR308_ALS_MEAS_RATE,0x05);  //20 Bit, Conversion time = 400ms,1000ms
    LTR308_WriteReg(LTR308_ALS_GAIN,0x00);  //Gain Range: 1
    for(retry=0;retry<500;retry++)
    {
      if(LTR308_ReadReg(LTR308_MAIN_STATUS)&0x08)
      {
        IIC_Read_Buf(LTR308_ADDR,LTR308_ALS_DATA_0,reg_val,3); //read data
        ESP_LOGI(TAG, "%d,%02x%02x%02x\r\n", __LINE__,reg_val[0],reg_val[1],reg_val[2]);
        *lightvalue = 0.6*(256*256*(reg_val[2]&0x0f) + 256*reg_val[1] + reg_val[0])/(1*4);
        break;
      }
      osi_Sleep(10);  //
    }
    LTR308_WriteReg(LTR308_MAIN_CTRL,0x00);  //ALS standby
  }
  else
  {
    *lightvalue = ERROR_CODE;
  }
}

/*******************************************************************************
                                      END         
*******************************************************************************/




