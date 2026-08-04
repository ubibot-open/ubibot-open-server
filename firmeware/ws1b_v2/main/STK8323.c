/*******************************************************************************
  * @file       STK8323 Sensor Application      
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
#include "stdio.h"
#include "STK8323.h"
#include "iic.h"
#include "math.h"
#include "MsgType.h"

uint8_t stk8323_chipid = 0;

/**
  * @brief  Read generic device register
  *
  * @param  reg   register to read
  * @param  data  pointer to buffer that store the data read(ptr)
  * @param  len   number of consecutive register to read
  *
  */
void stk8323_read_reg(uint8_t reg,uint8_t *data,uint8_t len)
{
  IIC_Read_Buf(STK8323_ADDR,reg,data,len);
}

/**
  * @brief  Write generic device register
  *
  * @param  reg   register to write
  * @param  data  pointer to data to write in register reg(ptr)
  * @param  len   number of consecutive register to write
  *
  */
void stk8323_write_reg(uint8_t reg,uint8_t *data,uint8_t len)
{
  IIC_Write_Buf(STK8323_ADDR,reg,data,len);
}

/**
  * @brief  DeviceWhoamI .[get]
  * @param  buff     buffer that stores data read
  *
  */
void stk8323_device_id_get(uint8_t *buff)
{
  stk8323_read_reg(STK832X_REG_CHIPID,buff,1);
}

void stk8323_reset(void) 
{                                      /*复位*/
	//reset STEP_COUNTER_OUT 0000H
	uint8_t reg_val;
  
	reg_val = STK832X_SWRST_VAL;
	stk8323_write_reg(STK832X_REG_SWRST,&reg_val,1); //
	ets_delay_ms(50);
}

void stk8323_uinit(void) //FOR test     res:failed 
{
	uint8_t reg_val;
	stk8323_reset(); /* soft-reset */  
                            /* set power mode */
	reg_val = STK832X_PWMD_SUSPEND;	// low-power mode
	stk8323_write_reg(STK832X_REG_POWMODE,&reg_val,1); //
}

void stk8323_init(void)
{
	uint8_t reg_val;
	stk8323_read_reg(STK832X_REG_CHIPID,&stk8323_chipid,1);
  
	if(stk8323_chipid == STK8323_ID)
	{
		stk8323_reset(); /* soft-reset */       

		/* set range, resolution */
		reg_val = STK832X_RANGESEL_4G;                  /*  4G   */
		stk8323_write_reg(STK832X_REG_RANGESEL,&reg_val,1); 

	 	/* set bandwidth */
		reg_val = STK832X_BWSEL_BW_125;  /*  125 Hz  计步需要*/ 
		stk8323_write_reg(STK832X_REG_BWSEL,&reg_val,1); 
     
		/* set Low_Power mode */
		reg_val = STK832X_PWMD_LOWPOWER;	        //Low_Power, sleep duration = 0ms EDM模式
		reg_val = 0x7A;                                 // Low_Power,sleep duration = 100ms  ESM 模式
		stk8323_write_reg(STK832X_REG_POWMODE,&reg_val,1); //
                
		/* set i2c watch dog */
		reg_val =  STK832X_INTFCFG_I2C_WDT_EN;          // enable watch dog  Watchdog timer period 1ms
		stk8323_write_reg(STK832X_REG_INTFCFG,&reg_val,1); //
                                            
		 reg_val = 0x40;                                /* set : Disable the data protection function.  Data output filtered */
   		stk8323_write_reg(STK832X_REG_DATASETUP,&reg_val,1); //  
                                                                  
		/* set the number of samples needed in slope detection */
		reg_val = 0x00;                         /*斜坡 判定 时间*/
		stk8323_write_reg(STK832X_REG_SLOPEDLY,&reg_val,1); //
   		
		reg_val = 0x14;                         /* 斜坡 判定 阈值 */
		stk8323_write_reg(STK832X_REG_SLOPETHD,&reg_val,1); //
                               
		reg_val = STK832X_STEPCNT2_STEP_CNT_EN; /*使能    计步计数器  */
		stk8323_write_reg(STK832X_REG_STEPCNT2,&reg_val,1); 
                
		/* INT1寄存器配置 */ 
		reg_val = 0x07;                      /*  set  0 SLP_EN_X,Y,Z any-motion (slope) interrupt*/
		stk8323_write_reg(STK832X_REG_INTEN1,&reg_val,1); //
        
		/* INT1 int config */
		reg_val = 0x01;                         //Int PIN 1/Int PIN2   Active high
		stk8323_write_reg(STK832X_REG_INTCFG1,&reg_val,1); //
                                   
//		reg_val = 0x00;                         /* non -latched  */
		reg_val = 0x03;                       /*temporary, 1s */
//		reg_val = 0x07;                        /* latched  */
		stk8323_write_reg(STK832X_REG_INTCFG2,&reg_val,1); //
                          
	 	reg_val = 0x04;               /* ANY_MOT_EN：Enable any-motion */
		//reg_val = 0x02;                 /*Enable significant motion*/
	 	stk8323_write_reg(STK832X_REG_SIGMOT2,&reg_val,1); //

	 	 /* 将任意运动使能 INT1 pin */
	 	reg_val = 0x0F; 
	 	stk8323_write_reg(STK832X_REG_INTMAP1,&reg_val,1); 

		/*INT pin  */
		reg_val = 0x00; 
		//reg_val = 0x04;                                         //enable Map FIFO full interrupt to INT1
		stk8323_write_reg(STK832X_REG_INTMAP2,&reg_val,1); 
	}
	else
	{
#ifdef DEBUG_MODE
		printf("stk8323 CHIP_ID ERR");
#endif 
	}
}

int stk8323_get_step(void) {                                   /*读取 计步计数*/
	int counter;
	uint8_t cnt_l,cnt_h;
             
	 stk8323_read_reg(STK832X_REG_STEPOUT1,&cnt_l,1);
	 stk8323_read_reg(STK832X_REG_STEPOUT2,&cnt_h,1);
	counter = cnt_h*256 + cnt_l;
#ifdef STK8323_DEBUG
	printf("counter= %d  cnt_l=%d  cnt_h=%d ",counter,cnt_l,cnt_h);
#endif
	return counter;
}

void stk8323_Steps_Clear(void) {                                /*清空 计步计数*/
	
	  uint8_t reg_val;

	   //reg_val = 0xB6;
	  //stk8323_write_reg(STK832X_REG_STEPCNT1,&reg_val,1); //

	   reg_val = 0x0C;
	  stk8323_write_reg(STK832X_REG_STEPCNT2,&reg_val,1); //
}

void stk832x_getdata(short int *X_DataOut, short int *Y_DataOut, short int *Z_DataOut)
{
	unsigned char  RegReadValue[6]={0};
	//RegAddr      = 0x3F;		
  	stk8323_read_reg(STK832X_REG_XOUT1, (uint8_t*)RegReadValue, 6);
	*X_DataOut = (short int)((((int)((char)RegReadValue[1])) << 8) | (RegReadValue[0] & 0xF0)) >> 4;  //resolution = 12 bit
	*Y_DataOut = (short int)((((int)((char)RegReadValue[3])) << 8) | (RegReadValue[2] & 0xF0)) >> 4;  //resolution = 12 bit
	*Z_DataOut = (short int)((((int)((char)RegReadValue[5])) << 8) | (RegReadValue[4] & 0xF0)) >> 4;  //resolution = 12 bit
}

uint8_t stk8323_intstatus(void)
{                                      /*读取  中断状态 */
   uint8_t reg_val;
   stk8323_read_reg(STK832X_REG_INTSTS1,&reg_val,1);//读取寄存器配置 
#ifdef STK8323_DEBUG
   printf("REG_INTSTS1= %x\n  REG_INTSTS2=%x\r\n  ",REG_INTSTS1,REG_INTSTS2);
#endif  
   return reg_val;
}

/*******************************************************************************
                                      END         
*******************************************************************************/



