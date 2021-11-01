#include <sdk_macros.h>
#include <nrf_log.h>

#include "pinout.h"
#include "epaper.h"

#include "periph_pwr.h"


#define MODULE_FLAG_3V3_REG     (1 << 0)
#define MODULE_FLAG_PWR_ON      (1 << 1)

static periph_pwr_activity_flag_t m_running_activities;
static uint32_t                 m_active_modules;


/**@brief Switch on external peripheral power.
*/
static void periph_pwr_on(void)
{
	nrf_gpio_pin_set(PIN_PWR_EN);
	nrf_gpio_cfg_output(PIN_PWR_EN);

	epaper_config_gpios(true);
}

/**@brief Switch off external peripheral power.
*/
static void periph_pwr_off(void)
{
	nrf_gpio_cfg_default(PIN_PWR_EN);

	epaper_config_gpios(false);
}


/**@brief Switch on the external 3.3V regulator.
*/
static void reg_3v3_on(void)
{
	nrf_gpio_pin_set(PIN_REG_EN);
}

/**@brief Switch off the external 3.3V regulator.
*/
static void reg_3v3_off(void)
{
	nrf_gpio_pin_clear(PIN_REG_EN);
}


static uint32_t modules_required_by_activity(periph_pwr_activity_flag_t activity)
{
	switch(activity)
	{
		case PERIPH_PWR_FLAG_INIT:
			return 0;

		case PERIPH_PWR_FLAG_CONNECTED:
			return 0;

		case PERIPH_PWR_FLAG_EPAPER_UPDATE:
			return MODULE_FLAG_3V3_REG | MODULE_FLAG_PWR_ON;

		case PERIPH_PWR_FLAG_VOLTAGE_MEASUREMENT:
			return MODULE_FLAG_3V3_REG | MODULE_FLAG_PWR_ON;

		case PERIPH_PWR_FLAG_GPS:
			return MODULE_FLAG_3V3_REG | MODULE_FLAG_PWR_ON;

		case PERIPH_PWR_FLAG_LORA:
			return MODULE_FLAG_3V3_REG; // LoRa module is connected directly to the 3.3V regulator

		case PERIPH_PWR_FLAG_LEDS:
			return MODULE_FLAG_3V3_REG | MODULE_FLAG_PWR_ON; // well, it's true…
	}

	return 0;
}


void periph_pwr_init(void)
{
	m_running_activities = 0;
	m_active_modules = 0;

	// initialize the GPIOs
	nrf_gpio_pin_clear(PIN_REG_EN); // initially off
	nrf_gpio_cfg_output(PIN_REG_EN);

	nrf_gpio_cfg_default(PIN_PWR_EN); // this pin has an external pulldown
}


ret_code_t periph_pwr_start_activity(periph_pwr_activity_flag_t activity)
{
	if((m_running_activities & activity) != 0)
	{ // activity already started => no change necessary
		return NRF_SUCCESS;
	}

	uint32_t requested_modules = modules_required_by_activity(activity);
	uint32_t modules_to_power_on = requested_modules & ~m_active_modules;

	if(modules_to_power_on & MODULE_FLAG_3V3_REG)
	{
		NRF_LOG_INFO("periph_pwr: 3.3V regulator on");
		reg_3v3_on();
	}

	if(modules_to_power_on & MODULE_FLAG_PWR_ON)
	{
		NRF_LOG_INFO("periph_pwr: external peripheral power on");
		periph_pwr_on();
	}

	m_running_activities |= activity;
	m_active_modules |= requested_modules;

	return NRF_SUCCESS;
}


ret_code_t periph_pwr_stop_activity(periph_pwr_activity_flag_t activity)
{
	if((m_running_activities & activity) == 0)
	{ // activity already stopped => no change necessary
		return NRF_SUCCESS;
	}

	m_running_activities &= ~activity;

	// determine all modules requested by the remaining activities
	uint32_t remaining_modules = 0;

	for(int i = 0; i < 32; i++)
	{
		if(m_running_activities & (1 << i))
		{
			remaining_modules |= modules_required_by_activity((1 << i));
		}
	}

	uint32_t modules_to_power_off = m_active_modules & ~remaining_modules;

	if(modules_to_power_off & MODULE_FLAG_3V3_REG)
	{
		NRF_LOG_INFO("periph_pwr: 3.3V regulator off");
		reg_3v3_off();
	}

	if(modules_to_power_off & MODULE_FLAG_PWR_ON)
	{
		NRF_LOG_INFO("periph_pwr: external peripheral power off");
		periph_pwr_off();
	}

	m_active_modules = remaining_modules;

	return NRF_SUCCESS;
}
