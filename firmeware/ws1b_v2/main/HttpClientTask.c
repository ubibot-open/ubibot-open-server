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
#include "stdlib.h"
#include "osi.h"
#include "stdbool.h"
#include "math.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "MsgType.h"
#include "JsonParse.h"
#include "app_config.h"
#include "esp_http_client.h"
#include "HttpClientTask.h"
#include "PCF8563.h"
#include "at24c32.h"
#include "cJSON.h"

#define TAG "HttpClient"

extern OsiSyncObj_t Timer_Binary;  //For Timer interrupt task
extern OsiMsgQ_t      Data_Queue;        //Used for cjson and memory save

// extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
// extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
  switch(evt->event_id) 
  {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
        if (err != 0) {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        esp_http_client_set_header(evt->client, "From", "user@example.com");
        esp_http_client_set_header(evt->client, "Accept", "text/html");
        esp_http_client_set_redirection(evt->client);
        break;
  }
  return ESP_OK;
}

/*******************************************************************************
//This function read respose from server and dump on console
*******************************************************************************/
static int readResponse(esp_http_client_handle_t httpClient)
{
  char *http_rx_buf;
  long lRetVal = -1;

  http_rx_buf = heap_caps_malloc(HTTP_RX_BUFF_LEN, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if(http_rx_buf==NULL) 
  {
    return lRetVal;
  }
  memset(http_rx_buf,0,HTTP_RX_BUFF_LEN);

  int content_length = esp_http_client_fetch_headers(httpClient);
  if (content_length >= 0)
  {
    ESP_LOGI(TAG, "content_length=%d",content_length);
    int data_read = esp_http_client_read_response(httpClient, http_rx_buf, HTTP_RX_BUFF_LEN);
    if (data_read >= 0)
    {
      lRetVal = esp_http_client_get_status_code(httpClient);
      ESP_LOGI(TAG, "HTTP Status = %ld, data_read = %d,GET Request READ:\n%s",lRetVal,data_read,http_rx_buf);
      switch (lRetVal)
      {
        case 200:
        {
          lRetVal = ParseJSONData(http_rx_buf); //Sensor Data
          break;
        }
        case 400:
        case 401:
        case 429:
          ParseJSONData(http_rx_buf); //Sensor Data
          break;
        default:
          break;
      }
    }
    else
    {
      ESP_LOGI(TAG, "Failed to read response, data_read = %d",data_read);
    }
  }
  else
  {
    ESP_LOGI(TAG, "HTTP client fetch headers failed.content_length=%d",content_length);
  }
  free(http_rx_buf);
  return lRetVal;
}


/*******************************************************************************
  HTTP GET METHOD
*******************************************************************************/
int HTTP_Get_Method(char *host,char *url,uint16_t port)
{
  int lRetVal = -1;
  esp_http_client_config_t config = {
    .transport_type = HTTP_TRANSPORT_OVER_TCP,
    .host = host,
    .port = port,
    .path = "/",
    .timeout_ms = 30000,
    .event_handler = _http_event_handler,
  };

  esp_http_client_handle_t httpClient = esp_http_client_init(&config);
  if(httpClient==NULL) 
  {
    return FAILURE;
  }

  esp_http_client_set_url(httpClient, url);
  esp_http_client_set_method(httpClient, HTTP_METHOD_GET);
  lRetVal = esp_http_client_open(httpClient, 0);
  if (lRetVal != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_http_client_open FAIL:%s", esp_err_to_name(lRetVal));
    if(lRetVal==ESP_ERR_HTTP_CONNECT)
    {
      ESP_LOGI(TAG, "%d,ESP_ERR_HTTP_CONNECT.", __LINE__);
    } 
    esp_http_client_cleanup(httpClient);
    return CONNECT_SERVER_FAILED;
  }
  else
  {
    ESP_LOGI(TAG, "Connection to server successfully\r\n");
  }
  lRetVal = readResponse(httpClient);
  esp_http_client_close(httpClient);
  esp_http_client_cleanup(httpClient);
  return lRetVal;
}

/*******************************************************************************
  HTTP POST Demonstration
*******************************************************************************/
int HTTP_Post_Method(char *host,char *url,uint16_t port,char *pbuf,uint32_t pbuf_len)
{
  long lRetVal = 0;
  esp_http_client_config_t config = {
    .transport_type = HTTP_TRANSPORT_OVER_TCP,
    .host = host,
    .port = port,
    .path = "/",
    .timeout_ms = 30000,
    .event_handler = _http_event_handler,
  };

  ESP_LOGI(TAG, "pbuf_len=%d,pbuf: %s",pbuf_len,pbuf);
  esp_http_client_handle_t httpClient = esp_http_client_init(&config);
  if(httpClient==NULL) 
  {
    return FAILURE;
  }

  esp_http_client_set_url(httpClient, url);
  esp_http_client_set_method(httpClient, HTTP_METHOD_POST);
  esp_http_client_set_header(httpClient, "Content-Type", "application/json");

  lRetVal = esp_http_client_open(httpClient, pbuf_len);  //
  if (lRetVal != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(lRetVal));
    if(lRetVal==ESP_ERR_HTTP_CONNECT)
    {
      ESP_LOGI(TAG, "%d,ESP_ERR_HTTP_CONNECT.", __LINE__);
    } 
    esp_http_client_cleanup(httpClient);
    return CONNECT_SERVER_FAILED;
  }

  lRetVal = esp_http_client_write(httpClient, pbuf, pbuf_len); //
  if (lRetVal < 0)
  {
    ESP_LOGE(TAG, "http Write failed %d", __LINE__);
  }
  lRetVal = readResponse(httpClient); //read respose from server
  esp_http_client_close(httpClient);
  esp_http_client_cleanup(httpClient);
  return lRetVal;
}

/*******************************************************************************
// data post task
*******************************************************************************/
int WiFi_Net_Application(uint8_t mode)
{
  int lRetVal = -1;
  wifi_ap_record_t wifidata_t;
  SensorMessage sMsg={0};
  
  ESP_LOGI(TAG, "%d.wifi http start.", __LINE__);
  ESP_LOGI(TAG, "heap: free=%u min_free=%u internal_free=%u spiram_free=%u",
            esp_get_free_heap_size(), esp_get_minimum_free_heap_size(),heap_caps_get_free_size(MALLOC_CAP_INTERNAL),heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  lRetVal = WlanConnect(); //Wlan Connect To The Accesspoint 10s timeout
  if(lRetVal<0)
  {
    ESP_LOGI(TAG, "%d", __LINE__);
    Wlan_Disconnect_AP(); //wlan disconnect form the ap
    return lRetVal;
  }
  else
  {
    esp_wifi_sta_get_ap_info(&wifidata_t);
    ESP_LOGI(TAG, "%d,wifidata_t.rssi=%d.", __LINE__,wifidata_t.rssi);
    sMsg.ts = Read_UnixTime();
    sMsg.sensornum=RSSI_NUM;         //Message Number
    sMsg.sensorval=wifidata_t.rssi;        //Message Value
    osi_MsgQWrite(&Data_Queue,&sMsg,OSI_SAVE_WAIT);   //Send Light Data Message
  }

  char *post_data;
  post_data = heap_caps_malloc(HTTP_TX_BUFF_LEN, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if(post_data==NULL)
  {
    Wlan_Disconnect_AP(); //wlan disconnect form the ap
    return lRetVal;
  }

  if(mode == UPDATE_TIME_MODE)
  {
    ESP_LOGI(TAG, "%d,Device UPDATE_TIME_MODE.", __LINE__);
    char *out_buf;
    cJSON *pJsonRoot;
    pJsonRoot=cJSON_CreateObject();
    cJSON_AddStringToObject(pJsonRoot,"pid",USR_PID);
    cJSON_AddStringToObject(pJsonRoot,"sn",USR_SN);
    out_buf = cJSON_PrintUnformatted(pJsonRoot);  //cJSON_Print(Root)
    memset(post_data,0,HTTP_TX_BUFF_LEN);
    mem_copy(post_data,out_buf,strlen(out_buf)); 
    free(out_buf);
    cJSON_Delete(pJsonRoot);  //delete cjson root

    ESP_LOGI(TAG, "heap: free=%u min_free=%u internal_free=%u spiram_free=%u",
            esp_get_free_heap_size(), esp_get_minimum_free_heap_size(),heap_caps_get_free_size(MALLOC_CAP_INTERNAL),heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    lRetVal = HTTP_Post_Method(USR_HTTP_HOST,USR_HTTP_TIME_URL,USR_HTTP_PORT,post_data,strlen(post_data)); 
    if(lRetVal == SUCCESS)
    {
      ESP_LOGI(TAG, "%d,HTTP_Post_Method success.", __LINE__);
    }
    else
    {
      ESP_LOGI(TAG, "%d,HTTP_Post_Method failed,code=%d.", __LINE__,lRetVal);
    }
  }
  if(mode == DATA_POST_MODE)
  {
    ESP_LOGI(TAG, "%d,Data post.", __LINE__);
    char field[9]={0};
    sMsg.ts = Read_UnixTime();
    char *out_buf;
    cJSON *pJsonRoot;
    OsiReturnVal_e msg_result = OSI_FAILURE;
    pJsonRoot=cJSON_CreateObject();
    cJSON_AddStringToObject(pJsonRoot,"pid",USR_PID);
    cJSON_AddStringToObject(pJsonRoot,"sn",USR_SN);
    cJSON_AddNumberToObject(pJsonRoot,"ts",sMsg.ts);
    cJSON *json_arry,*json_arrys = cJSON_CreateArray();
    cJSON_AddItemToObject(pJsonRoot,"payloads",json_arrys);
    for(uint8_t j=0;j<USR_POST_DATA_SUM;j++)
    {
      msg_result = osi_MsgQRead(&Data_Queue,&sMsg,OSI_NO_WAIT);  //Wait Sensor Value Message
      if(msg_result != OSI_OK) break;
      mem_set(field,0,sizeof(field));
      snprintf(field,sizeof(field),"field%d",sMsg.sensornum);  //fields number
      json_arry = cJSON_CreateObject();
      cJSON_AddItemToArray(json_arrys,json_arry);
      cJSON_AddNumberToObject(json_arry,"ts",sMsg.ts);  //
      cJSON_AddNumberToObject(json_arry,field,sMsg.sensorval);
    }
    out_buf = cJSON_PrintUnformatted(pJsonRoot);  //cJSON_Print(Root)
    memset(post_data,0,HTTP_TX_BUFF_LEN);
    mem_copy(post_data,out_buf,strlen(out_buf)); 
    free(out_buf);
    cJSON_Delete(pJsonRoot);  //delete cjson root   

    ESP_LOGI(TAG, "heap: free=%u min_free=%u internal_free=%u spiram_free=%u",
          esp_get_free_heap_size(), esp_get_minimum_free_heap_size(),heap_caps_get_free_size(MALLOC_CAP_INTERNAL),heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    lRetVal = HTTP_Post_Method(USR_HTTP_HOST,USR_HTTP_DATA_URL,USR_HTTP_PORT,post_data,strlen(post_data));
    if (lRetVal == SUCCESS)
    {
      ESP_LOGI(TAG, "%d,WiFi http post successed.", __LINE__);
    }
    else
    {
      ESP_LOGI(TAG, "%d,HTTP Post failed,code=%d.\r\n", __LINE__,lRetVal);
    }
  }
  Wlan_Disconnect_AP(); //wlan disconnect form the ap
  free(post_data);
  return lRetVal;
}

/*******************************************************************************
                                      END         
*******************************************************************************/
