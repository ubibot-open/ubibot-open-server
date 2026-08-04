/*******************************************************************************
  * @file       Power Measure Application Task
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
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "PCF8563.h"
#include "MsgType.h"

#define NO_OF_SAMPLES 16  //Multisampling

const static char *TAG = "PowerMeasureTask";

/*---------------------------------------------------------------
        ADC General Macros
---------------------------------------------------------------*/
//ADC1 Channels
#define EXAMPLE_ADC1_CHAN0          ADC_CHANNEL_3
#define EXAMPLE_ADC_ATTEN           ADC_ATTEN_DB_12

static int adc_raw[2][10]={0};
static int voltage[2][10]={0};
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void example_adc_calibration_deinit(adc_cali_handle_t handle);

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) 
        {
            calibrated = true;
        }
    }
#endif
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif
    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }
    return calibrated;
}

static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    esp_err_t ret=adc_cali_delete_scheme_curve_fitting(handle);
    if(ret!=ESP_OK)
    {
        ESP_LOGE(TAG, "%d.adc_cali_delete_scheme_curve_fitting failed.", __LINE__);
    }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}

/*******************************************************************************
//power voltage value
*******************************************************************************/
float power_adcValue(void)
{
    float adc_reading = 0;

    //ADC1 Init
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t ret=adc_oneshot_new_unit(&init_config1, &adc1_handle);
    if(ret!=ESP_OK)
    {
        ESP_LOGE(TAG, "%d.adc_oneshot_new_unit failed.", __LINE__);
    }
    //ADC1 Config
    adc_oneshot_chan_cfg_t config = {
        .atten = EXAMPLE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret=adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN0, &config);
    if(ret!=ESP_OK)
    {
        ESP_LOGE(TAG, "%d.adc_oneshot_config_channel failed.", __LINE__);
    }
    //ADC1 Calibration Init
    adc_cali_handle_t adc1_cali_chan0_handle = NULL;
    bool do_calibration1_chan0 = example_adc_calibration_init(ADC_UNIT_1, EXAMPLE_ADC1_CHAN0, EXAMPLE_ADC_ATTEN, &adc1_cali_chan0_handle);
    vTaskDelay(pdMS_TO_TICKS(100));

    for (int i = 0; i < NO_OF_SAMPLES; i++)
    {
        ret=adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN0, &adc_raw[0][0]);
        if(ret!=ESP_OK)
        {
            ESP_LOGE(TAG, "%d.adc_oneshot_read failed.", __LINE__);
        }
        ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, adc_raw[0][0]);
        if (do_calibration1_chan0) 
        {
            ret=adc_cali_raw_to_voltage(adc1_cali_chan0_handle, adc_raw[0][0], &voltage[0][0]);
            if(ret!=ESP_OK)
            {
                ESP_LOGE(TAG, "%d.adc_cali_raw_to_voltage failed.", __LINE__);
            }
            ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, voltage[0][0]);
        }
        adc_reading += voltage[0][0];
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    //Tear Down
    ret=adc_oneshot_del_unit(adc1_handle);
    if(ret!=ESP_OK)
    {
        ESP_LOGE(TAG, "%d.adc_oneshot_del_unit failed.", __LINE__);
    }
    if (do_calibration1_chan0) 
    {
        example_adc_calibration_deinit(adc1_cali_chan0_handle);
    }
  adc_reading /= NO_OF_SAMPLES;
  //Convert adc_reading to voltage in mV
  return (float)adc_reading * (270.0f+750.0f) / 270.0f / 1000.0f;
}

/*******************************************************************************
                                      END         
*******************************************************************************/










