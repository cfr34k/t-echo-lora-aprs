/*
 * vim: noexpandtab
 *
 * Copyright (c) 2021-2022 Thomas Kolb <cfr34k-git@tkolb.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <nrf_gpio.h>
#include <sdk_macros.h>
#include <nrf_log.h>

#include <app_pwm.h>

#include "pinout.h"
#include "periph_pwr.h"

#include "leds.h"

APP_PWM_INSTANCE(m_pwm, 3);

static uint8_t m_enabled_leds = 0;

static inline uint8_t led_to_pin(led_t led)
{
	switch(led)
	{
		case LED_RED:              return PIN_LED_RED;
		case LED_GREEN:            return PIN_LED_GREEN;
		case LED_BLUE:             return PIN_LED_BLUE;
		case LED_EPAPER_BACKLIGHT: return PIN_EPD_BL;
	}

	NRF_LOG_ERROR("leds: used invalid LED value %d.", led);
	return PIN_LED_RED;
}


ret_code_t led_on(led_t led)
{
	m_enabled_leds |= (1 << led);

	VERIFY_SUCCESS(periph_pwr_start_activity(PERIPH_PWR_FLAG_LEDS));

	if(led == LED_EPAPER_BACKLIGHT) {
		// Special handling for ePaper backlight: PWM.
		// Simply switching on causes the 3.3V regulator to overheat and is way
		// too bright.
		app_pwm_enable(&m_pwm);
		app_pwm_channel_duty_set(&m_pwm, 0, 5);
	} else {
		// normal LEDs
		uint8_t pin = led_to_pin(led);
		nrf_gpio_pin_clear(pin);
		nrf_gpio_cfg_output(pin);
	}

	return NRF_SUCCESS;
}


ret_code_t led_off(led_t led)
{
	if(led == LED_EPAPER_BACKLIGHT) {
		// Special handling for ePaper backlight: PWM.
		app_pwm_disable(&m_pwm);
	} else {
		// normal LEDs
		uint8_t pin = led_to_pin(led);
		nrf_gpio_cfg_default(pin);
	}

	m_enabled_leds &= ~(1 << led);

	if(m_enabled_leds == 0) {
		VERIFY_SUCCESS(periph_pwr_stop_activity(PERIPH_PWR_FLAG_LEDS));
	}

	return NRF_SUCCESS;
}


ret_code_t leds_init(void)
{
	ret_code_t err_code;

	app_pwm_config_t pwm_cfg = APP_PWM_DEFAULT_CONFIG_1CH(1000L, PIN_EPD_BL);

	pwm_cfg.pin_polarity[0] = APP_PWM_POLARITY_ACTIVE_HIGH;

	err_code = app_pwm_init(&m_pwm, &pwm_cfg, NULL);
	VERIFY_SUCCESS(err_code);

	return NRF_SUCCESS;
}
