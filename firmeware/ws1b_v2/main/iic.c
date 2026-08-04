/*******************************************************************************
  * @file       IIC BUS DRIVER APPLICATION       
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
#include "iic.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/rtc_io.h"
#include "esp_rom_sys.h"
#include "driver/i2c_master.h"

i2c_master_bus_handle_t bus_handle;

/*******************************************************************************
// delay about nus*ms
*******************************************************************************/
void ets_delay_ms(uint32_t nms)
{
  esp_rom_delay_us(1000*nms);
}

/*******************************************************************************
//init the iic bus
*******************************************************************************/
void I2C_Init(void)
{
  i2c_master_bus_config_t i2c_mst_config = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .i2c_port = I2C_MASTER_NUM,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  i2c_new_master_bus(&i2c_mst_config, &bus_handle);
}

/*******************************************************************************
//  write a byte to slave register
*******************************************************************************/
void IIC_Write_Reg(uint8_t sla_addr,uint8_t reg_addr,uint8_t val)
{
  i2c_device_config_t dev_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = sla_addr,
      .scl_speed_hz = I2C_MASTER_FREQ_HZ,
  };
  uint8_t write_buf[2] = {reg_addr, val};
  i2c_master_dev_handle_t dev_handle;
  i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
  i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  i2c_master_bus_rm_device(dev_handle);
}

/*******************************************************************************
// Read a byte from slave register
*******************************************************************************/
void IIC_Read_Reg(uint8_t sla_addr,uint8_t reg_addr,uint8_t *val)
{
  i2c_device_config_t dev_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = sla_addr,
      .scl_speed_hz = I2C_MASTER_FREQ_HZ,
  };
  i2c_master_dev_handle_t dev_handle;
  i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
  i2c_master_transmit_receive(dev_handle, &reg_addr, 1, val, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  i2c_master_bus_rm_device(dev_handle);
}

/*******************************************************************************
//  write multi byte to slave register
*******************************************************************************/
void IIC_Write_Buf(uint8_t sla_addr,uint8_t reg_addr,uint8_t *buf,uint8_t len) 
{
  i2c_device_config_t dev_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = sla_addr,
      .scl_speed_hz = I2C_MASTER_FREQ_HZ,
      .scl_wait_us = 20000,
  };
  uint8_t *write_buf=heap_caps_malloc(len+2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  memset(write_buf,0,len+2);
  write_buf[0] = reg_addr;
  mem_copy(write_buf+1,buf,len);
  i2c_master_dev_handle_t dev_handle;
  i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
  i2c_master_transmit(dev_handle, write_buf, len+1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  i2c_master_bus_rm_device(dev_handle);
  free(write_buf);
}

/*******************************************************************************
//  read multiple byte from slave register
*******************************************************************************/
void IIC_Read_Buf(uint8_t sla_addr,uint8_t reg_addr,uint8_t *buf,uint8_t len) 
{
  i2c_device_config_t dev_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = sla_addr,
      .scl_speed_hz = I2C_MASTER_FREQ_HZ,
  };
  i2c_master_dev_handle_t dev_handle;
  i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
  i2c_master_transmit_receive(dev_handle, &reg_addr, 1, buf, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  i2c_master_bus_rm_device(dev_handle);
}

/*******************************************************************************
                                      END         
*******************************************************************************/




