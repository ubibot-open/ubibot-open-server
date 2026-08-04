/*******************************************************************************
  * @file       at24c32 EEPROM CHIP DRIVER APPLICATION     
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
#include "stdlib.h"
#include "osi.h" 
#include "iic.h"
#include "at24c32.h"
#include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#define TAG "at24c32"

extern i2c_master_bus_handle_t bus_handle;

void at24c32_write_buf(uint16_t addr, uint8_t *buf, uint8_t size);

/*******************************************************************************
// FM24CL64B write multi bytewhit multiple try
*******************************************************************************/
void MulTry_FM24CL64B_Write(uint8_t sla_addr,uint16_t reg_addr,uint8_t *buf,uint8_t len) 
{
  i2c_device_config_t dev_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = sla_addr,
      .scl_speed_hz = I2C_MASTER_FREQ_HZ,
      .scl_wait_us = 20000,
  };
  uint8_t *write_buf = heap_caps_malloc(len+3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  memset(write_buf,0,len+3);
  write_buf[0] = reg_addr/256;
  write_buf[1] = reg_addr%256;
  mem_copy(write_buf+2,buf,len);
  i2c_master_dev_handle_t dev_handle;
  i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
  i2c_master_transmit(dev_handle, write_buf, len+2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  i2c_master_bus_rm_device(dev_handle);
  free(write_buf);
  osi_Sleep(20);
}

/*******************************************************************************
// FM24CL64B read multiple byte whit multiple try
*******************************************************************************/
void MulTry_FM24CL64B_Read(uint8_t sla_addr,uint16_t reg_addr,uint8_t *buf,uint8_t len) 
{
    uint8_t write_buf[2] = {reg_addr/256,reg_addr%256};
    i2c_device_config_t dev_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = sla_addr,
      .scl_speed_hz = I2C_MASTER_FREQ_HZ,
      .scl_wait_us = 20000,
  };
  i2c_master_dev_handle_t dev_handle;
  i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
  i2c_master_transmit_receive(dev_handle, write_buf, 2, buf, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  i2c_master_bus_rm_device(dev_handle);
}

/******************************************************************************
//at24c32 write page,addr:0-1023,Size:1-16
******************************************************************************/
static void at24c32_write_Page(uint16_t reg_addr, uint8_t *buffer, uint8_t buf_len)
{
  gpio_set_level(eepromwp_pin, 0);
  MulTry_FM24CL64B_Write(AT24C32_ADDR,reg_addr,buffer,buf_len);  //
  gpio_set_level(eepromwp_pin, 1); 
}

/******************************************************************************
//at24c32 write data
//addr:0-1023,*buffer:write data,Size:1-256
******************************************************************************/
void at24c32_write_buf(uint16_t addr, uint8_t *buf, uint8_t size)
{
  uint8_t i,add=0;
  uint8_t remain;
  if(size)
  {
    remain = 16 - addr % 16;
    if (remain)
    {
      remain = size > remain ? remain : size;
      at24c32_write_Page(addr, buf, remain);
      addr += remain;
      add += remain;
      size -= remain;
    }

    remain = size / 16;
    for (i = 0; i < remain; i++)
    {
      at24c32_write_Page(addr, buf+add, 16);
      addr += 16;
      add += 16;
    }

    remain = size % 16;
    if (remain)
    {
      at24c32_write_Page(addr, buf+add, remain);
      addr += remain;
    }
  }
}

/******************************************************************************
//at24c32 read data
*******************************************************************************/
void at24c32_read_buf(uint16_t reg_addr,uint8_t *buf,uint8_t size)
{
  MulTry_FM24CL64B_Read(AT24C32_ADDR, reg_addr, buf, size);
}

/*******************************************************************************
                                      END         
*******************************************************************************/
