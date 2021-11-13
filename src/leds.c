#include <nrf_gpio.h>
#include <sdk_macros.h>
#include <nrf_log.h>

#include "pinout.h"
#include "periph_pwr.h"

#include "leds.h"

static uint8_t m_enabled_leds = 0;

static inline uint8_t led_to_pin(led_t led)
{
	switch(led)
	{
		case LED_RED:   return PIN_LED_RED;
		case LED_GREEN: return PIN_LED_GREEN;
		case LED_BLUE:  return PIN_LED_BLUE;
	}

	NRF_LOG_ERROR("leds: used invalid LED value %d.", led);
	return PIN_LED_RED;
}


ret_code_t led_on(led_t led)
{
	m_enabled_leds |= (1 << led);

	VERIFY_SUCCESS(periph_pwr_start_activity(PERIPH_PWR_FLAG_LEDS));

	uint8_t pin = led_to_pin(led);
	nrf_gpio_pin_clear(pin);
	nrf_gpio_cfg_output(pin);

	return NRF_SUCCESS;
}


ret_code_t led_off(led_t led)
{
	uint8_t pin = led_to_pin(led);
	nrf_gpio_cfg_default(pin);

	m_enabled_leds &= ~(1 << led);

	if(m_enabled_leds == 0) {
		VERIFY_SUCCESS(periph_pwr_stop_activity(PERIPH_PWR_FLAG_LEDS));
	}

	return NRF_SUCCESS;
}
