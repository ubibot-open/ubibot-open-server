/*******************************************************************************
  * @file       WiFi config Application Task  
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
#ifndef __WiFi_CONFIG__
#define __WiFi_CONFIG__

#define CONNECTED_BIT (1 << 0)    //网络连接
#define WIFI_S_I_BIT (1 << 1)     //wifi是否进入初始化状态
#define WIFI_I_BIT (1 << 2)       //wifi初始化完成状态
#define WIFI_S_BIT (1 << 3)       //wifi启动状态
#define Nets_Group_ALL_BIT (CONNECTED_BIT|\
                             WIFI_S_I_BIT|\
                             WIFI_I_BIT|\
                             WIFI_S_BIT\
                            )
                            
extern void init_wifi(void);
extern void start_user_wifi(void);
extern void stop_user_wifi(void);

extern void Wlan_Disconnect_AP(void);  //wlan disconnect form the ap//
extern int WlanConnect(void); 

#endif  //__WiFi_CONFIG__


/*******************************************************************************
                                      END         
*******************************************************************************/







