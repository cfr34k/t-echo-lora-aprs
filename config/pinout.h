#ifndef PINOUT_H
#define PINOUT_H

#include <nrf_gpio.h>

#define PIN_EPD_CS     NRF_GPIO_PIN_MAP(0, 30)
#define PIN_EPD_SCK    NRF_GPIO_PIN_MAP(0, 31)
#define PIN_EPD_MOSI   NRF_GPIO_PIN_MAP(0, 29)
#define PIN_EPD_MISO   NRF_GPIO_PIN_MAP(1,  6)
#define PIN_EPD_DC     NRF_GPIO_PIN_MAP(0, 28)
#define PIN_EPD_RST    NRF_GPIO_PIN_MAP(0,  2)
#define PIN_EPD_BUSY   NRF_GPIO_PIN_MAP(0,  3)
#define PIN_EPD_BL     NRF_GPIO_PIN_MAP(1, 11)

// peripheral power enable. Must be set to enable LoRa, GPS, LEDs and Flash
#define PIN_PWR_EN     NRF_GPIO_PIN_MAP(0, 12)

#endif // PINOUT_H
