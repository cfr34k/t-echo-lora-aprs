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

// enable pin of the 3.3V regulator.
#define PIN_REG_EN     NRF_GPIO_PIN_MAP(0, 13)

#define PIN_LED_RED    NRF_GPIO_PIN_MAP(1,  3)
#define PIN_LED_GREEN  NRF_GPIO_PIN_MAP(1,  1)
#define PIN_LED_BLUE   NRF_GPIO_PIN_MAP(0, 14)

#define PIN_BUTTON_1   NRF_GPIO_PIN_MAP(1, 10)
#define PIN_BUTTON_2   NRF_GPIO_PIN_MAP(0, 18)   // beware: this is the reset pin!
#define PIN_BUTTON_3   NRF_GPIO_PIN_MAP(0, 11)   // touch button

#define SAADC_INPUT_VBAT  NRF_SAADC_INPUT_AIN2

#define PIN_GPS_RESET_N    NRF_GPIO_PIN_MAP(1,  5)
#define PIN_GPS_WAKEUP     NRF_GPIO_PIN_MAP(1,  2)
#define PIN_GPS_PPS        NRF_GPIO_PIN_MAP(1,  4)
#define PIN_GPS_RX         NRF_GPIO_PIN_MAP(1,  9)
#define PIN_GPS_TX         NRF_GPIO_PIN_MAP(1,  8)

#define PIN_LORA_CS     NRF_GPIO_PIN_MAP(0, 24)
#define PIN_LORA_SCK    NRF_GPIO_PIN_MAP(0, 19)
#define PIN_LORA_MOSI   NRF_GPIO_PIN_MAP(0, 22)
#define PIN_LORA_MISO   NRF_GPIO_PIN_MAP(0, 23)
#define PIN_LORA_RST    NRF_GPIO_PIN_MAP(0, 25)
#define PIN_LORA_BUSY   NRF_GPIO_PIN_MAP(0, 17)
#define PIN_LORA_DIO1   NRF_GPIO_PIN_MAP(0, 20)
#define PIN_LORA_DIO3   NRF_GPIO_PIN_MAP(0, 21)

#endif // PINOUT_H
