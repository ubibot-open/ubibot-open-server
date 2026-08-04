/*******************************************************************************
  * @file       HTTP Client Application Task
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
#include "esp_http_client.h"

#define CONNECT_SERVER_FAILED   -2

/*******************************************************************************
 FUNCTION PROTOTYPES
*******************************************************************************/
extern int HTTP_Get_Method(char *host,char *url,uint16_t port); //
extern int HTTP_Post_Method(char *host,char *url,uint16_t port,char *pbuf,uint32_t pbuf_len); //HTTP POST
extern int WiFi_Net_Application(uint8_t mode); //data post task//

/*******************************************************************************
                                      END         
*******************************************************************************/
