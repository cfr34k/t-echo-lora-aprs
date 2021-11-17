#ifndef BSP_CONFIG_H
#define BSP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

#include "pinout.h"

// LEDs definitions for T-Echo
#define LEDS_NUMBER    1

#define LED_1          PIN_LED_BLUE   // indicates bluetooth status
//#define LED_2          PIN_LED_RED
//#define LED_3          PIN_LED_GREEN

#define LED_START      LED_1
#define LED_STOP       LED_1

#define LEDS_ACTIVE_STATE 0

#define LEDS_LIST { LED_1 }

#define LEDS_INV_MASK  LEDS_MASK

#define BSP_LED_0      LED_1
//#define BSP_LED_1      LED_2
//#define BSP_LED_2      LED_3

#define BUTTONS_NUMBER 0

#define BUTTONS_ACTIVE_STATE 0

#if 0
#define BUTTON_1       PIN_BUTTON_1
#define BUTTON_PULL    NRF_GPIO_PIN_NOPULL

#define BUTTONS_LIST { BUTTON_1 }

#define BSP_BUTTON_0   BUTTON_1

//#define RX_PIN_NUMBER  8
//#define TX_PIN_NUMBER  3
//#define CTS_PIN_NUMBER 7
//#define RTS_PIN_NUMBER 5
//#define HWFC           true
#endif

#ifdef __cplusplus
}
#endif

#endif // BSP_CONFIG_H
