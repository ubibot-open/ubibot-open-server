/*******************************************************************************

  * @file       Temperature and Humility Sensor Application Driver     
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
#include "stdlib.h"
#include "osi.h"
#include "sht30dis.h"
#include "iic.h"
#include "crc_8_check.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#define TAG "sht32dis"

extern i2c_master_bus_handle_t bus_handle;

/*******************************************************************************
// SHT30DIS Single Shot Measure
// Command:0x2400-Repeatability:high,clock stretching:disable
*******************************************************************************/
void sht30_SingleShotMeasure(float *temp,float *humi)
{
  uint8_t recive[7]={0};
  uint16_t tempval=ERROR_CODE,humival=ERROR_CODE;
  i2c_device_config_t dev_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = sht30dis_addr,
      .scl_speed_hz = I2C_MASTER_FREQ_HZ,
  };
  uint8_t write_buf[2] = {0x24,0x00};
  i2c_master_dev_handle_t dev_handle;
  i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
  i2c_master_transmit(dev_handle, write_buf, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  osi_Sleep(100);

  i2c_master_transmit_receive(dev_handle, write_buf, 2, recive, 6, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  i2c_master_bus_rm_device(dev_handle);
  if(Data_Crc_Check(&recive[0],3)==0)
  {
    tempval=recive[0];
    tempval=tempval<<8;
    tempval+=recive[1];
    *temp=175*(float)tempval/65535-45;
  }
  if(Data_Crc_Check(&recive[3],3)==0)
  {
    humival=recive[3];
    humival=humival<<8;
    humival+=recive[4];
    *humi=100*(float)humival/65535; 
  }
  if((*temp==0)&&(*humi==0))
  {
    *temp=ERROR_CODE;
    *humi=ERROR_CODE;
  }
  // ESP_LOGI(TAG, "%d,%.4f,%.4f", __LINE__,*temp,*humi);
}

/*******************************************************************************
                                      END         
*******************************************************************************/


