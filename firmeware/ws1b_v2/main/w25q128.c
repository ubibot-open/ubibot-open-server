/*******************************************************************************
  * @file       W25Q128 NOR FLASH CHIP DRIVER APPLICATION      
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
#include "user_spi.h"
#include "w25q128.h"
#include "MsgType.h"
#include "esp_log.h"

#define TAG "w25q128"

/*******************************************************************************
  w25q128 read register
*******************************************************************************/
uint8_t w25q_ReadReg(uint8_t com_val)
{
  SET_SPI1_CS_OFF();  //w25q128 spi cs enable
  SPI_SendReciveByte(com_val);  //send the read command
  uint8_t value=SPI_SendReciveByte(0x00);  //clk signal,get register value
  SET_SPI1_CS_ON();  //w25q128 spi cs disable
  return value;
}

/*******************************************************************************
// w25q128 read id
*******************************************************************************/
uint16_t w25q_ReadId(void)
{
  uint16_t w25qid=0;
  SET_SPI1_CS_OFF();  //w25q128 spi cs enable
  SPI_SendReciveByte(W25Q_DEVICE_ID);  //send the read manufacturer id
  SPI_SendReciveByte(0x00);  //recive data
  SPI_SendReciveByte(0x00);  //recive data
  SPI_SendReciveByte(0x00);  //recive data
  w25qid=SPI_SendReciveByte(0x00);  //recive data
  w25qid=w25qid<<8;
  w25qid+=SPI_SendReciveByte(0x00);  //recive data
  SET_SPI1_CS_ON();  //w25q128 spi cs disable
  ESP_LOGI(TAG, "%d,%04x", __LINE__,w25qid);
  return w25qid;
}

/*******************************************************************************
  w25q128 write register
*******************************************************************************/
void w25q_WriteCommand(uint8_t com_val)
{
  SET_SPI1_CS_OFF();  //w25q128 spi cs enable
  SPI_SendReciveByte(com_val);  //send the write command
  SET_SPI1_CS_ON();  //w25q128 spi cs disable
}

/*******************************************************************************
  waite write completed
*******************************************************************************/
static void w25q_WaitCompleted(void)
{
  uint16_t retry=0; 
  while(w25q_ReadReg(READ_STATUS_REGISTER)&0x01) //read status register,bit0=0:Ready,bit0=1:Busy
  {
    if(retry++>6000) break; //time out 1min
    osi_Sleep(10);
  }
}

/*******************************************************************************
  w25q128 write register
*******************************************************************************/
void w25q_WriteReg(uint8_t com_val,uint8_t value)
{
  w25q_WriteCommand(WRITE_ENABLE);  //write enable
  
  SET_SPI1_CS_OFF();  //w25q128 spi cs enable
  SPI_SendReciveByte(com_val);  //send the write register command
  SPI_SendReciveByte(value);     //write register value
  SET_SPI1_CS_ON();  //w25q128 spi cs disable
  
  w25q_WaitCompleted();  //waite command completed
}

/*******************************************************************************
  w25q128 erase subsector-4k
*******************************************************************************/
void w25q_EraseSubsector(uint32_t addr)
{
  w25q_WriteCommand(WRITE_ENABLE);  //write enable
  
  SET_SPI1_CS_OFF();  //w25q128 spi cs enable
  SPI_SendReciveByte(SECTOR_ERASE);  //subsector eaase code
  SPI_SendReciveByte((uint8_t)((addr)>>16));  //24bit address first 8 bit addres
  SPI_SendReciveByte((uint8_t)((addr)>>8));
  SPI_SendReciveByte((uint8_t)(addr));  //24bit address last 8 bit address
  SET_SPI1_CS_ON();  //w25q128 spi cs disable
  
  w25q_WaitCompleted();  //wait command completed
}

/*******************************************************************************
  w25q128 erase chip
*******************************************************************************/
void w25q_EraseChip(void)
{
  w25q_WriteCommand(WRITE_ENABLE);  //write enable
  w25q_WriteCommand(CHIP_ERASE);  //chip eaase code
  w25q_WaitCompleted();  //wait command completed
}

/*******************************************************************************
  w25q128 wake up
*******************************************************************************/
void w25q_WakeUp(void)
{
  w25q_WriteCommand(RELEASE_POWER_DOWN);  //release power down code
}

/*******************************************************************************
  w25q128 power down mode
*******************************************************************************/
void w25q_PowerDown(void)
{
  w25q_WriteCommand(POWER_DOWN);  //power down code
}

/*******************************************************************************
  w25q128 read data
*******************************************************************************/
void w25q_Read_Data(uint32_t addr,char *buffer,uint16_t size)
{
  SET_SPI1_CS_OFF();  //w25q128 spi cs enable
  SPI_SendReciveByte(READ_DATA);  //read operations code 
  SPI_SendReciveByte((uint8_t)((addr)>>16));   //24bit address first 8 bit address
  SPI_SendReciveByte((uint8_t)((addr)>>8));
  SPI_SendReciveByte((uint8_t)(addr));  //24bit address last 8 bit address
  for(uint16_t i=0;i<size;i++)
  {
    buffer[i]=SPI_SendReciveByte(0x00);	//read one byte
  }
  SET_SPI1_CS_ON();  //w25q128 spi cs disable
}

/*******************************************************************************
  w25q128 write data
*******************************************************************************/
static void w25q_WritePage(uint32_t addr,char *buffer,uint8_t Size)
{
  w25q_WriteCommand(WRITE_ENABLE);  //write enable
  
  SET_SPI1_CS_OFF();  //w25q128 spi cs enable
  SPI_SendReciveByte(PAGE_PROGRAM);  //page program code
  SPI_SendReciveByte((uint8_t)(addr>>16));   //24bit address first 8 bit address
  SPI_SendReciveByte((uint8_t)(addr>>8));
  SPI_SendReciveByte((uint8_t)addr);	//24bit address last 8 bit address
  for(uint16_t i=0;i<Size;i++)
  {
    SPI_SendReciveByte(buffer[i]);  //write data
  }
  SET_SPI1_CS_ON();  //w25q128 spi cs disable

  w25q_WaitCompleted();  //w25q128 wait command completed
}

/*******************************************************************************
  w25q128 write data no check
*******************************************************************************/
void w25q_Write_Data(uint32_t addr,char *buffer,uint16_t Size)
{
  uint16_t i,n_i;
  uint16_t remain;
  
  n_i=Size/256+2;
  remain=256-addr%256;  //page remain byte number
  remain=remain>Size?Size:remain;

  for(i=0;i<=n_i;i++)
  {
    if(remain>0)
    {
      w25q_WritePage(addr,buffer,remain);  //no end flag
    } 
    if(remain==Size)
    {
      break;
    }
    
    buffer+=remain;
    addr+=remain;
    Size-=remain;
    remain=Size>256?256:Size;
  }
}

/*******************************************************************************
                                      END         
*******************************************************************************/




