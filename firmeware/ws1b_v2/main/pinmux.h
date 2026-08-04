/*******************************************************************************
  * @file       Pin config DRIVER APPLICATION      
  * @author 
  * @version
  * @date 
  * @brief
  ******************************************************************************
  * @attention
  *
  *
*******************************************************************************/

#ifndef __PINMUX_H__
#define __PINMUX_H__
#include "stdint.h"

#define usbint_pin      2
#define acce_int1       3
#define batvol_pin      4
#define sw1int_pin      5
#define pwr_mode_pin    6

#define led1_pin        7
#define led2_pin        8
#define iic_scl_pin     9      //SCL GPIO PIN
#define iic_sda_pin     10      //SDA GPIO PIN
#define ub_dt_p1_pin1    13
#define ub_dt_p1_pin2    14

#define spi_sck_pin     24
#define spi_mosi_pin    23
#define spi_miso_pin    25
#define spi_cs_pin      28

#define buzzer_pin      26
#define eepromwp_pin    27

#define uart0tx_pin     (UART_PIN_NO_CHANGE)
#define uart0rx_pin     (UART_PIN_NO_CHANGE)
#define uart0_rts_pin   (UART_PIN_NO_CHANGE)
#define uart0_cts_pin   (UART_PIN_NO_CHANGE)

extern void PinMuxConfig(void);
extern void buzzer_makeSound(uint32_t n_msec); //buzzer make sound//

#endif //  __PINMUX_H__




/*******************************************************************************
                                      END         
*******************************************************************************/