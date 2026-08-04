/*******************************************************************************
  * @file       PCF8563 Driver  
  * @author 
  * @version
  * @date 
  * @brief
  ******************************************************************************
  * @attention
  *
  *
*******************************************************************************/
#ifndef __PCF8563_H__
#define __PCF8563_H__

/*-------------------------------- Includes ----------------------------------*/
#include "stdint.h"
#include "stdbool.h"
#include "time.h"

#define PCF8563_ADDR            0x51

#define Control_status_1        0x00
#define Control_status_2        0x01

#define VL_seconds              0x02
#define Minutes                 0x03
#define Hours                   0x04
#define Days                    0x05
#define Weekdays                0x06
#define Century_months          0x07
#define Years                   0x08

#define Minute_alarm            0x09
#define Hour_alarm              0x0a
#define Day_alarm               0x0b
#define Weekday_alarm           0x0c

#define CLKOUT_control          0x0d
#define Timer_control           0x0e
#define Timer_value             0x0f

typedef struct tm* tm_struct;

/*******************************************************************************
// FUNCTION PROTOTYPES
*******************************************************************************/
extern void Timer_IC_Init(void);  //PCF8563 init
extern void Timer_IC_Reset_Time(void);  //PCF8563 Reset Time
extern unsigned long Read_UnixTime(void);  //PCF8563 read Unix time
extern void Update_UnixTime(uint32_t unix_tm);
extern void Read_UTCtime(char *buffer,uint8_t buf_size);

#endif //  __PCF8563_H__

/*******************************************************************************
                                      END         
*******************************************************************************/




