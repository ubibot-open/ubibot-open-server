/*******************************************************************************
  * @file       SPI BUS DRIVER APPLICATION       
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
#include <string.h>
#include "user_spi.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

static spi_device_handle_t g_spi=NULL;

//SPI int
int VprocHALInit(void)
{
  /*if the customer platform requires any init
    * then implement such init here.
    * Otherwise the implementation of this function is complete
    */
  esp_err_t ret = ESP_OK;

  spi_bus_config_t buscfg = {
      .miso_io_num = spi_miso_pin,
      .mosi_io_num = spi_mosi_pin,
      .sclk_io_num = spi_sck_pin,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      // .max_transfer_sz = 4092  //Defaults to 4092
    };

  spi_device_interface_config_t devcfg = {
      .clock_speed_hz = 10 * 1000 * 1000, // Clock out at 10 MHz
      .mode = 0,                          // SPI mode 0
      .spics_io_num = -1,                 // CS pin
      .queue_size = 7,                    //queue 7 transactions at a time
  };
  //Initialize the SPI bus
  if (g_spi)
  {
    return ret;
  }
  ret = spi_bus_initialize(SPI2_HOST, &buscfg, 0);
  assert(ret == ESP_OK);

  ret = spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi);
  assert(ret == ESP_OK);
  
  return ret;
}

/*******************************************************************************
//spi interface init
*******************************************************************************/
void UserSpiInit(void)
{  
  VprocHALInit();
  SET_SPI1_CS_ON();
}

/*******************************************************************************
//spi send and recive a byte
*******************************************************************************/
uint8_t SPI_SendReciveByte(uint8_t write_val)
{
  /*Note: Implement this as per your platform*/
  esp_err_t ret;
  spi_transaction_t tr;
  uint8_t read_val = 0xFF;
  
  memset(&tr, 0, sizeof(tr));       //Zero out the transaction
  tr.length = sizeof(uint8_t) * 8; //Len is in bytes, transaction length is in bits.
  tr.rxlength = sizeof(uint8_t) * 8;

  tr.tx_buffer = &write_val;
  tr.rx_buffer = &read_val;

  ret = spi_device_transmit(g_spi, &tr); //Transmit
   assert(ret == ESP_OK);

  return read_val;
}

/*******************************************************************************
                                      END         
*******************************************************************************/






