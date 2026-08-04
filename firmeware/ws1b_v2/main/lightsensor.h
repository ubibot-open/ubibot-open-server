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
#ifndef __LIGHT_SENSOR_H__
#define __LIGHT_SENSOR_H__

/*-------------------------------- Includes ----------------------------------*/
#include "stdint.h"

#define SUCCESS 0
#define FAILURE -1
#define ERROR_CODE      0xffff

/**
 *  Address registers
 */
#define LTR308_ADDR		  0X53
#define LTR308_MAIN_CTRL		(0x00)
#define LTR308_ALS_MEAS_RATE	(0x04)
#define LTR308_ALS_GAIN			(0x05)
#define LTR308_PART_ID			(0x06)
#define LTR308_MAIN_STATUS		(0x07)
#define LTR308_ALS_DATA_0		(0x0D)
#define LTR308_ALS_DATA_1		(0x0E)
#define LTR308_ALS_DATA_2		(0x0F)
#define LTR308_INT_CFG			(0x19)
#define LTR308_INT_PST 			(0x1A)
#define LTR308_ALS_THRES_UP_0	(0x21)
#define LTR308_ALS_THRES_UP_1	(0x22)
#define LTR308_ALS_THRES_UP_2	(0x23)
#define LTR308_ALS_THRES_LOW_0	(0x24)
#define LTR308_ALS_THRES_LOW_1	(0x25)
#define LTR308_ALS_THRES_LOW_2	(0x26)

/** Default values loaded in probe function */
#define LTR308_PARTID           (0xB1)  /** part_id value */

/*******************************************************************************
 FUNCTION PROTOTYPES
*******************************************************************************/
extern int LightSensor_Init(void);  // light sensor init
extern void LightSensor_value(float *lightvalue);  //measure the light value

#endif //  __LIGHT_SENSOR_H__

/*******************************************************************************
                                      END         
*******************************************************************************/




