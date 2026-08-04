/*******************************************************************************
  * @file       ub_dt_p1 Temperature Sensor Application      
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
#include "ub_dt_p1.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "pinmux.h"
#include "crc_8_check.h"
#include "MsgType.h"

extern void ets_delay_us(uint32_t us);

/*******************************************************************************
//SET IO HIGH
*******************************************************************************/
void DATA_IO_ON(void)
{
  gpio_set_level(ub_dt_p1_pin1, 1);
}

/*******************************************************************************
//SET IO LOW
*******************************************************************************/
void DATA_IO_OFF(void)
{
  gpio_set_level(ub_dt_p1_pin1, 0);
}

/*******************************************************************************
//Configure PIN for GPIO Input mode
*******************************************************************************/
void ub_dt_p1_io_in(void)
{
  gpio_set_direction(ub_dt_p1_pin1, GPIO_MODE_INPUT);
}

/*******************************************************************************
//Configure PIN for GPIO Output mode
*******************************************************************************/
void ub_dt_p1_io_out(void)
{
  gpio_set_direction(ub_dt_p1_pin1, GPIO_MODE_OUTPUT);
}

/*******************************************************************************
//SET IO HIGH
*******************************************************************************/
void DATA_IO2_ON(void)
{
  gpio_set_level(ub_dt_p1_pin2, 1);
}

/*******************************************************************************
//SET IO LOW
*******************************************************************************/
void DATA_IO2_OFF(void)
{
  gpio_set_level(ub_dt_p1_pin2, 0);
}

/*******************************************************************************
//Configure PIN for GPIO Input mode
*******************************************************************************/
void ub_dt_p1_io2_in(void)
{
  gpio_set_direction(ub_dt_p1_pin2, GPIO_MODE_INPUT);
}

/*******************************************************************************
//Configure PIN for GPIO Output mode
*******************************************************************************/
void ub_dt_p1_io2_out(void)
{
  gpio_set_direction(ub_dt_p1_pin2, GPIO_MODE_OUTPUT);
}

/*******************************************************************************
//ub_dt_p1 reset,return:0 reset success,return:-1,failured
*******************************************************************************/
short ub_dt_p1_reset(void)
{
  uint8_t retry=0;
  
  ub_dt_p1_io_out();
  DATA_IO_OFF();
  ets_delay_us(750);
  
  ub_dt_p1_io_in();
  ets_delay_us(30);

  while(gpio_get_level(ub_dt_p1_pin1))  //waite ub_dt_p1 respon
  {
    if(retry++>100)
    {
      return FAILURE;
    }
    ets_delay_us(3);
  }

  ets_delay_us(480);
  ub_dt_p1_io_out();  //data pin out mode;

  return SUCCESS;
}

/*******************************************************************************
//ub_dt_p1 2 reset,return:0 reset success,return:-1,failured
*******************************************************************************/
static short ub_dt_p1_2_reset(void)
{
  uint8_t retry=0;
  
  ub_dt_p1_io2_out();
  DATA_IO2_OFF();
  ets_delay_us(750);
  
  ub_dt_p1_io2_in();
  ets_delay_us(30);
  
  while(gpio_get_level(ub_dt_p1_pin2))  //waite ub_dt_p1 respon
  {
    if(retry++>100)
    {
      return FAILURE;
    }
    ets_delay_us(3);
  }

  ets_delay_us(480);
  ub_dt_p1_io2_out();  //data pin out mode

  return SUCCESS;
}

/*******************************************************************************
//ub_dt_p1 read a bit,return 0/1 
*******************************************************************************/
static uint8_t ub_dt_p1_read_bit(void)
{
  uint8_t data;
  
  ub_dt_p1_io_out();
  DATA_IO_OFF();
  ets_delay_us(3);
  
  ub_dt_p1_io_in();
  ets_delay_us(10);
  
  if(gpio_get_level(ub_dt_p1_pin1))
  {
    data=1;
  }
  else
  {
    data=0;
  }
  
  ets_delay_us(60);
  
  return data;
}

/*******************************************************************************
//ub_dt_p1 2 read a bit,return 0/1 
*******************************************************************************/
static uint8_t ub_dt_p1_2_read_bit(void)
{
  uint8_t data;
  
  ub_dt_p1_io2_out();
  DATA_IO2_OFF();
  ets_delay_us(3);

  ub_dt_p1_io2_in();
  ets_delay_us(10);
  
  if(gpio_get_level(ub_dt_p1_pin2))
  {
    data=1;
  }
  else
  {
    data=0;
  }

  ets_delay_us(60);
  
  return data;
}

/*******************************************************************************
//ub_dt_p1 read a byte
*******************************************************************************/
uint8_t ub_dt_p1_read_byte(void)
{
  uint8_t i,j,data=0;
  
  for(i=0;i<8;i++)
  {
    j=ub_dt_p1_read_bit();
    
    data=(j<<7)|(data>>1);
  }
  
  return data;
}

/*******************************************************************************
//ub_dt_p1 2 read a byte
*******************************************************************************/
static uint8_t ub_dt_p1_2_read_byte(void)
{
  uint8_t i,j,data=0;
  
  for(i=0;i<8;i++)
  {
    j=ub_dt_p1_2_read_bit();
    
    data=(j<<7)|(data>>1);
  }
  
  return data;
}

/*******************************************************************************
//ub_dt_p1 write a byte
*******************************************************************************/
void ub_dt_p1_write_byte(uint8_t data)
{
  uint8_t i,data_bit;
  
  ub_dt_p1_io_out();  //data pin out mode
  
  for(i=0;i<8;i++)
  {
    data_bit=data&0x01;
    
    if(data_bit)
    {
      DATA_IO_OFF();
      ets_delay_us(5);
      
      DATA_IO_ON();
      ets_delay_us(75);
    }
    else
    {
      DATA_IO_OFF();
      ets_delay_us(75);
      
      DATA_IO_ON();
      ets_delay_us(5);
    }
    data=data>>1;
  }
}

/*******************************************************************************
//ub_dt_p1 2 write a byte
*******************************************************************************/
static void ub_dt_p1_2_write_byte(uint8_t data)
{
  uint8_t i,data_bit;
  
  ub_dt_p1_io2_out();  //data pin out mode
  
  for(i=0;i<8;i++)
  {
    data_bit=data&0x01;
    
    if(data_bit)
    {
      DATA_IO2_OFF();
      ets_delay_us(5);
      
      DATA_IO2_ON();
      ets_delay_us(75);
    }
    else
    {
      DATA_IO2_OFF();
      ets_delay_us(75);
      
      DATA_IO2_ON();
      ets_delay_us(5);
    }
    data=data>>1;
  }
}

/*******************************************************************************
//ub_dt_p1 start convert
*******************************************************************************/
static uint8_t ub_dt_p1_start(void)
{
  uint8_t resp_val=0;
  
  ets_delay_us(5000);
  if(ub_dt_p1_reset()==0)
  {
    resp_val |= 0x01;
    ub_dt_p1_write_byte(0xcc);   //skip rom
    ub_dt_p1_write_byte(0x44);   //start convert
    DATA_IO_ON();
  }
  if(ub_dt_p1_2_reset()==0)
  {
    resp_val |= 0x02;
    ub_dt_p1_2_write_byte(0xcc);   //skip rom
    ub_dt_p1_2_write_byte(0x44);   //start convert
    DATA_IO2_ON();
  }

  if(resp_val) osi_Sleep(750);  //
  return resp_val;
}

/*******************************************************************************
//ub_dt_p1 get temperature value
*******************************************************************************/
void ub_dt_p1_get_temp(float *temp_value1,float *temp_value2)
{
  uint8_t j;
  uint8_t data_buf[10] = {0};
  short temp;
  uint8_t data_h,data_l;
  short res_val=-1;
  *temp_value1 = ERROR_CODE;
  *temp_value2 = ERROR_CODE;
  uint8_t ext_status = ub_dt_p1_start();  //ub_dt_p1 start convert
  ets_delay_us(5000);
  if(ext_status&0x01)
  {
    res_val=ub_dt_p1_reset();
    if(res_val==0)
    {
      ub_dt_p1_write_byte(0xcc);   //skip rom
      ub_dt_p1_write_byte(0xbe);   //read data
      for(j=0;j<9;j++)
      {
        data_buf[j] = ub_dt_p1_read_byte();  //
      }
      if(calcrc_bytes(data_buf,9)==0)
      {
        data_l = data_buf[0];
        data_h = data_buf[1];
        if(data_h>7)  //temperature value<0
        {
          data_l=~data_l;
          data_h=~data_h;
          
          temp=data_h;
          temp<<=8;
          temp+=data_l;
          *temp_value1=(float)-temp*0.0625;
        }
        else //temperature value>=0
        {
          temp=data_h;
          temp<<=8;
          temp+=data_l;
          *temp_value1=(float)temp*0.0625;
        }
      }
    }
  }  
  if(ext_status&0x02)
  {
    res_val=ub_dt_p1_2_reset();
    if(res_val==0)
    {
      ub_dt_p1_2_write_byte(0xcc);   //skip rom
      ub_dt_p1_2_write_byte(0xbe);   //read data
      for(j=0;j<9;j++)
      {
        data_buf[j] = ub_dt_p1_2_read_byte();  //
      }
      if(calcrc_bytes(data_buf,9)==0)
      {
        data_l = data_buf[0];
        data_h = data_buf[1];
        if(data_h>7)  //temperature value<0
        {
          data_l=~data_l;
          data_h=~data_h;
          
          temp=data_h;
          temp<<=8;
          temp+=data_l;
          *temp_value2=(float)-temp*0.0625;
        }
        else //temperature value>=0
        {
          temp=data_h;
          temp<<=8;
          temp+=data_l;
          *temp_value2=(float)temp*0.0625;
        }
      }
    }
  }
}

/*******************************************************************************
                                      END         
*******************************************************************************/





