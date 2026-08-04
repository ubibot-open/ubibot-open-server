# Embed the server root certificate into the final binary
#
# (If this was a component, we would set COMPONENT_EMBED_TXTFILES here.)
set(requires esp-tls nvs_flash esp_event esp_netif esp_http_client driver json esp_adc bt esp_wifi app_update)
idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    list(APPEND requires esp_stubs protocol_examples_common)
endif()
idf_component_register(SRCS "HttpClientTask.c" "pinmux.c" "main.c" "osi.c" "crc_8_check.c" "ub_dt_p1.c" 
                    "iic.c" "at24c32.c" "PCF8563.c" "sht30dis.c" "lightsensor.c"  
                    "user_spi.c" "w25q128.c" "JsonParse.c" 
                    "power_adc.c" "app_config.c" "STK8323.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES ${requires}
                    EMBED_TXTFILES howsmyssl_com_root_cert.pem)



                    