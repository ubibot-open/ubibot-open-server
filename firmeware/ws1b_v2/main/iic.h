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
#ifndef __IIC_H__
#define __IIC_H__

/*-------------------------------- Includes ----------------------------------*/
#include "stdint.h"
#include "pinmux.h"

#define SUCCESS 0
#define FAILURE -1

#define I2C_MASTER_NUM    I2C_NUM_0 /*!< I2C port number for master dev */
#define I2C_MASTER_SCL_IO iic_scl_pin     /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO iic_sda_pin     /*!< gpio number for I2C master data  */

#define I2C_MASTER_FREQ_HZ        100000   /*!< I2C master clock frequency */
#define I2C_MASTER_TIMEOUT_MS     1000  

/*******************************************************************************
// FUNCTION PROTOTYPES
*******************************************************************************/
extern void ets_delay_ms(uint32_t ms);
extern void I2C_Init(void);  //init the iic bus
extern void IIC_Write_Reg(uint8_t sla_addr,uint8_t reg_addr,uint8_t val);  //
extern void IIC_Read_Reg(uint8_t sla_addr,uint8_t reg_addr,uint8_t *val);  //
extern void IIC_Write_Buf(uint8_t sla_addr,uint8_t reg_addr,uint8_t *buf,uint8_t len); //
extern void IIC_Read_Buf(uint8_t sla_addr,uint8_t reg_addr,uint8_t *buf,uint8_t len);  //

#endif //  __IIC_H__

/*******************************************************************************
                                      END         
*******************************************************************************/















