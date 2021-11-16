#ifndef LEDS_H
#define LEDS_H

#include <sdk_errors.h>

typedef enum
{
	LED_RED,
	LED_GREEN,
	LED_BLUE,
	LED_EPAPER_BACKLIGHT
} led_t;

ret_code_t leds_init(void);

ret_code_t led_on(led_t led);
ret_code_t led_off(led_t led);

#endif // LEDS_H
