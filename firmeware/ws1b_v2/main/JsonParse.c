/*******************************************************************************
  * @file       cJson Application Task    
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
#include "stdlib.h"
#include "osi.h"
#include "cJSON.h"
#include "esp_log.h"
#include "PCF8563.h"
#include "JsonParse.h"

#define TAG "JsonParse"

#define SUCCESS 0
#define FAILURE -1

/*******************************************************************************
// parse post response data
*******************************************************************************/
int ParseJSONData(char *ptr)
{
  if(NULL == ptr)
  {
    return FAILURE;
  }
  cJSON *pJson = cJSON_Parse(ptr);
  if(pJson ==NULL )
  { 
    return FAILURE;
  }
  cJSON *pSub = cJSON_GetObjectItem(pJson, "c");  //result
  if(NULL!=pSub)
  {
    ESP_LOGI(TAG, "\"c\":%d\r\n",pSub->valueint);
  }  

  pSub = cJSON_GetObjectItem(pJson, "t");  //time
  if(NULL!=pSub)
  { 
    ESP_LOGI(TAG, "\"t\":%d\r\n",pSub->valueint);
    Update_UnixTime(pSub->valueint);  //update time
  }
  cJSON_Delete(pJson);  //delete pJson 
  return SUCCESS;
}

/*******************************************************************************
                                      END         
*******************************************************************************/









