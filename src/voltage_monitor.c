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

#include "nrf_error.h"
#include "nrf_saadc.h"
#include "nrfx_saadc.h"
#include "app_timer.h"

#define NRF_LOG_MODULE_NAME voltage_monitor
#include <nrf_log.h>
NRF_LOG_MODULE_REGISTER();

#include "sdk_macros.h"

#include "periph_pwr.h"

#include "voltage_monitor.h"

#define REFRESH_TIMER_INTERVAL_SEC  60
#define REFRESH_TIMER_TICKS APP_TIMER_TICKS(1000 * REFRESH_TIMER_INTERVAL_SEC)

static nrf_saadc_value_t m_adc_result[VOLTAGE_MONITOR_CHANNEL_COUNT];

static voltage_monitor_callback_t m_callback;

static bool m_voltage_monitor_active;

static uint8_t m_tracked_bat_percent;

static uint8_t m_power_state;

static uint32_t m_interval;

APP_TIMER_DEF(m_refresh_timer_id);
APP_TIMER_DEF(m_sampling_delay_timer_id);


typedef struct
{
	int16_t voltage;
	int8_t  percent;
} lut_entry_t;

/* LUTs need to be sorted by increasing voltage and the percentage values
 * should range from 0 to 100. */
static const lut_entry_t LUT_LIPO[9] = {
	{3000,   0},
	{3200,   3},
	{3400,   5},
	{3500,  10},
	{3600,  30},
	{3700,  50},
	{3800,  70},
	{4000,  90},
	{4100, 100}};


#define NUM_PWR_STATE_FLAGS 1

/* Power state threshold voltages (in millivolt). The corresponding modules
 * will be flagged as unavailable when battery is below the voltage given here.
 */

static const int16_t POWER_STATE_THRESHOLDS_LIPO[NUM_PWR_STATE_FLAGS] = {
	3100, // VOLTAGE_MONITOR_STATE_IDX_ALLOW_WAKEUP
};


static uint8_t vbat_lookup(int16_t vbat, const lut_entry_t *lut, uint8_t lut_size)
{
	// shortcut
	if(vbat <= lut[0].voltage)
	{
		return lut[0].percent;
	}

	// search LUT and interpolate
	for(uint8_t i = 1; i < lut_size; i++)
	{
		int32_t voltage_u = lut[i].voltage;
		if(voltage_u > vbat)
		{
			int32_t voltage_l = lut[i-1].voltage;

			int32_t percent_l = lut[i-1].percent;
			int32_t percent_u = lut[i].percent;

			uint8_t result = percent_l + (percent_u - percent_l) * (vbat - voltage_l) / (voltage_u - voltage_l);

			return result;
		}
	}

	// value not found in the LUT
	return lut[lut_size-1].percent;
}


static uint8_t vbat_to_percent(int16_t vbat_millivolt)
{
	const uint8_t lut_size = sizeof(LUT_LIPO) / sizeof(LUT_LIPO[0]);

	uint8_t power_state_before = m_power_state;

	/* power state flag handling. Each flag will be cleared once the battery
	 * voltage is below the given threshold. It will not be restored even if
	 * the battery “recovers”. */
	for(int i = 0; i < NUM_PWR_STATE_FLAGS; i++)
	{
		if(vbat_millivolt < POWER_STATE_THRESHOLDS_LIPO[i])
		{
			m_power_state &= ~(1 << i);
		}
	}

	// FIXME: just for debugging
	if(power_state_before != m_power_state)
	{
		NRF_LOG_INFO("Power state changed! 0x%02x => 0x%02x", power_state_before, m_power_state);
	}

	return vbat_lookup(vbat_millivolt, LUT_LIPO, lut_size);
}


/**@brief SAADC event handler.
 */
static void cb_saadc_handler(nrfx_saadc_evt_t const *p_evt)
{
	int16_t meas_millivolt[VOLTAGE_MONITOR_CHANNEL_COUNT];
	uint8_t bat_percent;

	ret_code_t err_code;

	switch(p_evt->type)
	{
		case NRFX_SAADC_EVT_DONE: // Sampling done
			m_voltage_monitor_active = false;

			// uninitialize the SAADC to save power
			nrfx_saadc_uninit();

			// shut down the external voltage switch
			periph_pwr_stop_activity(PERIPH_PWR_FLAG_VOLTAGE_MEASUREMENT);

			// convert measured values to millivolt
			meas_millivolt[VOLTAGE_MONITOR_VBAT_RESULT_INDEX] =
				m_adc_result[VOLTAGE_MONITOR_VBAT_RESULT_INDEX]
				* 600L * 2 * 4 / 4096L;

			bat_percent = vbat_to_percent(meas_millivolt[VOLTAGE_MONITOR_VBAT_RESULT_INDEX]);

			if(bat_percent >= 100)
			{ // battery is being charged
				m_tracked_bat_percent = 100;
			}

			// track the battery percentage. The value can only go downwards
			// (upwards is only possible if the battery is actively charged,
			// see above).  This prevents "bouncing" in high-current
			// situations.
			if(bat_percent < m_tracked_bat_percent)
			{
				m_tracked_bat_percent = bat_percent;
			}

			m_callback(meas_millivolt, m_tracked_bat_percent);
			break;

		case NRFX_SAADC_EVT_CALIBRATEDONE:
			err_code = app_timer_start(m_sampling_delay_timer_id, APP_TIMER_TICKS(10), NULL);
			APP_ERROR_CHECK(err_code);
			break;

		default:
			break;
	}
}


/**@brief Function for initializing the SAADC.
*/
static ret_code_t saadc_init(void)
{
	ret_code_t err_code;

	// basic initialization

	nrfx_saadc_config_t config = NRFX_SAADC_DEFAULT_CONFIG;

	err_code = nrfx_saadc_init(&config, cb_saadc_handler);
	VERIFY_SUCCESS(err_code);

	// channel setup: all channels are measured with the internal reference (0.6 V), Gain 1/4, 40 μs sampling time

	// VBAT
	nrf_saadc_channel_config_t ch_config = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(VOLTAGE_MONITOR_VBAT_SAADC_PORT);
	ch_config.gain = NRF_SAADC_GAIN1_4;
	ch_config.acq_time = NRF_SAADC_ACQTIME_40US; // default 10 μs

	err_code = nrfx_saadc_channel_init(VOLTAGE_MONITOR_VBAT_RESULT_INDEX, &ch_config);
	VERIFY_SUCCESS(err_code);

	// calibrate the ADC

	err_code = nrfx_saadc_calibrate_offset();
	return err_code;
}


/**@brief Start sampling the monitored voltages.
 * @details
 * This functions starts the sampling procedure and returns immediately.
 * Sampling is done asynchronously in the background. cb_saadc_handler() will
 * be called when sampling is done.
 *
 * @retval                 The result of the sampling start.
 */
static ret_code_t start_sampling(void)
{
	if(m_voltage_monitor_active)
	{ // sampling was already started
		return NRF_SUCCESS;
	}

	periph_pwr_start_activity(PERIPH_PWR_FLAG_VOLTAGE_MEASUREMENT);

	m_voltage_monitor_active = true;

	// after initialization, calibration will be started, which in turn
	// triggers the sampling when done.
	return saadc_init();
}


static void cb_refresh_timer(void *context)
{
	static uint32_t sec_count = 0, last_refresh_sec = 0;

	sec_count += REFRESH_TIMER_INTERVAL_SEC;

	if((sec_count - last_refresh_sec) >= m_interval) {
		last_refresh_sec += m_interval;

		APP_ERROR_CHECK(start_sampling());
	}
}


static void cb_sampling_delay_timer(void *context)
{
	ret_code_t err_code;

	// set up the sampling buffer
	err_code = nrfx_saadc_buffer_convert(m_adc_result, VOLTAGE_MONITOR_CHANNEL_COUNT);
	APP_ERROR_CHECK(err_code);

	// start the conversion
	err_code = nrfx_saadc_sample();
	APP_ERROR_CHECK(err_code);
}


ret_code_t voltage_monitor_init(voltage_monitor_callback_t callback)
{
	ret_code_t err_code;

	m_callback = callback;
	m_voltage_monitor_active = false;

	m_tracked_bat_percent = 100;

	m_interval = 60;

	// set all power state flags
	m_power_state = (1 << NUM_PWR_STATE_FLAGS) - 1;

	err_code = app_timer_create(&m_refresh_timer_id, APP_TIMER_MODE_REPEATED, cb_refresh_timer);
	VERIFY_SUCCESS(err_code);

	err_code = app_timer_create(&m_sampling_delay_timer_id, APP_TIMER_MODE_SINGLE_SHOT, cb_sampling_delay_timer);
	VERIFY_SUCCESS(err_code);

	return err_code;
}


ret_code_t voltage_monitor_start(uint32_t interval_sec)
{
	VERIFY_SUCCESS(start_sampling());

	m_interval = interval_sec;

	VERIFY_SUCCESS(app_timer_start(m_refresh_timer_id, REFRESH_TIMER_TICKS, NULL));

	return NRF_SUCCESS;
}


ret_code_t voltage_monitor_stop(void)
{
	VERIFY_SUCCESS(app_timer_stop(m_refresh_timer_id));

	return NRF_SUCCESS;
}


ret_code_t voltage_monitor_trigger(void)
{
	return start_sampling();
}


uint8_t voltage_monitor_get_power_state(void)
{
	return m_power_state;
}
