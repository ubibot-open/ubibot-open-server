/*******************************************************************************
  * @file       SPI BUS DRIVER APPLICATION       
  * @author 
  * @version
  * @date 
  * @brief
  ******************************************************************************
  * @attention
  *
  *
*******************************************************************************/
#ifndef __SPI_H__
#define __SPI_H__

/*-------------------------------- Includes ----------------------------------*/
#include "stdint.h"
#include "driver/gpio.h"
#include "pinmux.h"

#define SET_SPI1_CS_ON()    gpio_set_level(spi_cs_pin, 1); 
#define SET_SPI1_CS_OFF()   gpio_set_level(spi_cs_pin, 0);

/*******************************************************************************
 FUNCTION PROTOTYPES
*******************************************************************************/
extern void UserSpiInit(void);  //spi interface init
extern uint8_t SPI_SendReciveByte(uint8_t addr);  //spi send and recive a byte

#endif //  __SPI_H__

/*******************************************************************************
                                      END         
*******************************************************************************/




