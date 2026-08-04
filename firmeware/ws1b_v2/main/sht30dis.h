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
#ifndef __SHT30DIS_H__
#define __SHT30DIS_H__

/*-------------------------------- Includes ----------------------------------*/
#include "stdint.h"

#define SUCCESS 0
#define FAILURE -1

#define ERROR_CODE              0xffff
#define sht30dis_addr           0x44    //7 MSB address 0x44

/*******************************************************************************
 FUNCTION PROTOTYPES
*******************************************************************************/
extern void sht30_SingleShotMeasure(float *temp,float *humi);  //read temperature humility data with locked//

#endif //  __SHT30DIS_H__

/*******************************************************************************
                                      END         
*******************************************************************************/




