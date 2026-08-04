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

#include <stdlib.h>
#include "osi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "MsgType.h"
#include "app_config.h"

#define TAG "NET_CFG"

#define DEFAULT_SCAN_LIST_SIZE 32
extern EventGroupHandle_t Nets_Group;

bool scan_flag = false;
esp_netif_t *STA_netif_t;
esp_netif_t *AP_netif_t;

esp_timer_handle_t timer_wifi_handle = NULL; //定时器句柄
void timer_wifi_cb(void *arg);
esp_timer_create_args_t timer_wifi_arg = {
    .callback = &timer_wifi_cb,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "Wifi_Timer"};
void timer_wifi_cb(void *arg)
{
    start_user_wifi();  //超时未获取到IP
}

static void event_handler(void *arg, esp_event_base_t event_base,int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) 
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d",MAC2STR(event->mac), event->aid);
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) 
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d, reason:%d",MAC2STR(event->mac), event->aid, event->reason);
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "event_base == WIFI_EVENT,event_id == WIFI_EVENT_STA_START\r\n");
        if (scan_flag == false)
        {
            esp_wifi_connect();
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI(TAG, "event_base == WIFI_EVENT,event_id == WIFI_EVENT_STA_CONNECTED\r\n");
        esp_timer_start_once(timer_wifi_handle, 30000 * 1000);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG,"event_base == WIFI_EVENT,event_id == WIFI_EVENT_STA_DISCONNECTED\r\n");
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "Wi-Fi disconnected,reason:%d", event->reason);
        xEventGroupClearBits(Nets_Group, CONNECTED_BIT);
        if (scan_flag == false)
        {
            if ((xEventGroupGetBits(Nets_Group) & WIFI_S_BIT) == WIFI_S_BIT) esp_wifi_connect();
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI(TAG,"event_base == IP_EVENT,event_id == IP_EVENT_STA_GOT_IP\r\n");
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        esp_timer_stop(timer_wifi_handle);
        xEventGroupSetBits(Nets_Group, CONNECTED_BIT);
    }
    else
    {
        ESP_LOGI(TAG, "event_base=%s,event_id=%d", event_base, event_id);
    }
}

void init_wifi(void) //
{
    xEventGroupSetBits(Nets_Group, WIFI_S_I_BIT);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    STA_netif_t = esp_netif_create_default_wifi_sta();
    AP_netif_t = esp_netif_create_default_wifi_ap();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MAX_MODEM)); //最大省电

    xEventGroupSetBits(Nets_Group, WIFI_I_BIT);
}

void stop_user_wifi(void)
{
    if ((xEventGroupGetBits(Nets_Group) & WIFI_S_BIT) == WIFI_S_BIT)
    {
        xEventGroupClearBits(Nets_Group, WIFI_S_BIT);
        esp_err_t err = esp_wifi_stop();
        if (err == ESP_ERR_WIFI_NOT_INIT)
        {
            return;
        }
        // ESP_ERROR_CHECK(err);
        ESP_LOGI(TAG, "turn off WIFI! \n");
    }
    else
    {
        ESP_LOGI(TAG, "WIFI not start! \n");
    }
}

void start_user_wifi(void)
{
    if ((xEventGroupGetBits(Nets_Group) & WIFI_S_I_BIT) != WIFI_S_I_BIT)
    {
        init_wifi();
    }

    ESP_LOGI(TAG, "%d,set_user_wifi", __LINE__);
    if ((xEventGroupGetBits(Nets_Group) & WIFI_S_BIT) == WIFI_S_BIT)
    {
        esp_err_t err = esp_wifi_stop();
        if (err == ESP_ERR_WIFI_NOT_INIT)
        {
            return;
        }
        ESP_ERROR_CHECK(err);
    }
    xEventGroupSetBits(Nets_Group, WIFI_S_BIT);

    esp_wifi_set_country_code("", 1);  
    wifi_config_t s_staconf;
    memset(&s_staconf.sta, 0, sizeof(s_staconf));
    mem_copy((char *)s_staconf.sta.ssid, USR_SSID,strlen(USR_SSID));
    mem_copy((char *)s_staconf.sta.password, USR_PASSWORD,strlen(USR_PASSWORD));
    s_staconf.sta.scan_method = 1;
    s_staconf.sta.channel = 0;
    s_staconf.sta.sort_method = 0;
    s_staconf.sta.listen_interval = 0;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &s_staconf));
    esp_wifi_set_band_mode(WIFI_BAND_MODE_AUTO);
    esp_netif_dhcpc_start(STA_netif_t);

    ESP_LOGI(TAG, "%d,start_user_wifi", __LINE__);
    ESP_ERROR_CHECK(esp_wifi_start());
}

/*******************************************************************************
//wlan disconnect form the ap
*******************************************************************************/
void Wlan_Disconnect_AP(void)
{
    esp_wifi_disconnect();
    stop_user_wifi();
}

/*******************************************************************************
//Wlan Connect To The Accesspoint
*******************************************************************************/
int WlanConnect(void)
{
    start_user_wifi();
    // 30s timeout
    if ((xEventGroupWaitBits(Nets_Group, CONNECTED_BIT, false, true, 1000*DEFAULT_WIFI_CNT_TIMEOUT / portTICK_PERIOD_MS) & CONNECTED_BIT) == CONNECTED_BIT)
    {
        return SUCCESS;
    }
    else
    {   
        return FAILURE;
    }
}

/*******************************************************************************
                                      END         
*******************************************************************************/








