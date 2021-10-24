#ifndef BSP_CONFIG_H
#define BSP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

// LEDs definitions for T-Echo
#define LEDS_NUMBER    3

#define LED_1          NRF_GPIO_PIN_MAP(1, 3)
#define LED_2          NRF_GPIO_PIN_MAP(1, 1)
#define LED_3          NRF_GPIO_PIN_MAP(0,14)

#define LED_START      LED_1
#define LED_STOP       LED_4

#define LEDS_ACTIVE_STATE 0

#define LEDS_LIST { LED_1, LED_2, LED_3 }

#define LEDS_INV_MASK  LEDS_MASK

#define BSP_LED_0      LED_1
#define BSP_LED_1      LED_2
#define BSP_LED_2      LED_3

#define BUTTONS_NUMBER 2

#define BUTTON_1       (32+10)
#define BUTTON_2       (11)
#define BUTTON_PULL    NRF_GPIO_PIN_NOPULL

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST { BUTTON_1 }

#define BSP_BUTTON_0   BUTTON_1

//#define RX_PIN_NUMBER  8
//#define TX_PIN_NUMBER  3
//#define CTS_PIN_NUMBER 7
//#define RTS_PIN_NUMBER 5
//#define HWFC           true

#ifdef __cplusplus
}
#endif

#endif // BSP_CONFIG_H
