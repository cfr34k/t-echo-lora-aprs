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

#include <nrfx_uarte.h>
#include <nrf_gpio.h>

#include <sdk_macros.h>

#define NRF_LOG_MODULE_NAME gps
#include <nrf_log.h>
NRF_LOG_MODULE_REGISTER();

#include <app_timer.h>

#include "pinout.h"
#include "periph_pwr.h"
#include "nmea.h"

#include "gps.h"

typedef enum {
	GPS_RESET_WAIT1,
	GPS_RESET_ACTIVE,
	GPS_RESET_SEND_CONFIG,
	GPS_RESET_WAIT3,
	GPS_RESET_COMPLETE
} gps_reset_state_t;

#define GPS_RESET_MS_WAIT1      10  // power-on delay
#define GPS_RESET_MS_ACTIVE    200  // apply reset for this duration
#define GPS_RESET_MS_WAIT2    3000  // boot time after reset
#define GPS_RESET_MS_WAIT3    1000  // time between configuration and power-off


static nrfx_uarte_t m_uarte = NRFX_UARTE_INSTANCE(0);

APP_TIMER_DEF(m_gps_reset_timer);

static gps_callback_t m_callback;

#define RX_BUF_SIZE 85   // NMEA sentence length is max. 82 bytes; + "\r\n\0"

static uint8_t m_rx_buffer[2][RX_BUF_SIZE];
static uint8_t m_rx_buffer_used[2];

static uint8_t m_rx_buffer_idx;

static uint8_t m_rx_buffer_complete_idx;
static bool    m_rx_buffer_complete;

static nmea_data_t m_nmea_data;

static gps_reset_state_t m_reset_state;

static bool m_is_powered;

static void cb_uarte(nrfx_uarte_event_t const * p_event, void *p_context)
{
	ret_code_t err_code;

	uint8_t rx_byte;

	switch(p_event->type)
	{
		case NRFX_UARTE_EVT_RX_DONE:
			rx_byte = m_rx_buffer[m_rx_buffer_idx][ m_rx_buffer_used[m_rx_buffer_idx] ];

			m_rx_buffer_used[m_rx_buffer_idx]++;

			if((rx_byte == '\n') || (m_rx_buffer_used[m_rx_buffer_idx] >= RX_BUF_SIZE)) {
				m_rx_buffer_complete_idx = m_rx_buffer_idx;
				m_rx_buffer_complete = true;

				m_rx_buffer_idx = (m_rx_buffer_idx + 1) % 2;
				m_rx_buffer_used[m_rx_buffer_idx] = 0;
			}

			err_code = nrfx_uarte_rx(
					&m_uarte,
					&m_rx_buffer[m_rx_buffer_idx][ m_rx_buffer_used[m_rx_buffer_idx] ],
					1);
			APP_ERROR_CHECK(err_code);
			break;

		case NRFX_UARTE_EVT_ERROR:
			NRF_LOG_ERROR("UART error! Trying to restart.");

			nrfx_uarte_rx_abort(&m_uarte);

			err_code = nrfx_uarte_rx(
					&m_uarte,
					&m_rx_buffer[m_rx_buffer_idx][ m_rx_buffer_used[m_rx_buffer_idx] ],
					1);
			APP_ERROR_CHECK(err_code);
			break;

		case NRFX_UARTE_EVT_TX_DONE:
			// nothing to do here
			break;
	}
}


void cb_gps_reset_timer(void *p_context)
{
	ret_code_t err_code;

	switch(m_reset_state)
	{
		case GPS_RESET_WAIT1:
			nrf_gpio_pin_clear(PIN_GPS_RESET_N);
			nrf_gpio_cfg_output(PIN_GPS_RESET_N);

			m_reset_state = GPS_RESET_ACTIVE;

			err_code = app_timer_start(m_gps_reset_timer, APP_TIMER_TICKS(GPS_RESET_MS_ACTIVE), NULL);
			APP_ERROR_CHECK(err_code);
			break;

		case GPS_RESET_ACTIVE:
			nrf_gpio_cfg_default(PIN_GPS_RESET_N); // let the pullup do it's work again

			m_reset_state = GPS_RESET_SEND_CONFIG;

			err_code = app_timer_start(m_gps_reset_timer, APP_TIMER_TICKS(GPS_RESET_MS_WAIT2), NULL);
			APP_ERROR_CHECK(err_code);
			break;

		case GPS_RESET_SEND_CONFIG:
			{
				//static uint8_t cmd[] = "\r\n$PMTK103*30\r\n"; // cold boot (MTK version)
				//static uint8_t cmd[] = "$PMTK314,0,1,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n"; // set sentence interval config (MTK version)
				static uint8_t cmd[] = "$PCAS03,1,0,1,1,1,0,0,0,0,0,,,0,0,,,,0*32\r\n"; // set sentence interval config (CASIC version)

				APP_ERROR_CHECK(nrfx_uarte_tx(&m_uarte, cmd, strlen((const char*)cmd)));

				m_reset_state = GPS_RESET_WAIT3;

				err_code = app_timer_start(m_gps_reset_timer, APP_TIMER_TICKS(GPS_RESET_MS_WAIT3), NULL);
				APP_ERROR_CHECK(err_code);
			}
			break;

		case GPS_RESET_WAIT3:
			m_reset_state = GPS_RESET_COMPLETE;
			m_callback(GPS_EVT_RESET_COMPLETE, NULL);
			break;

		case GPS_RESET_COMPLETE:
			// this state should never be reached in the timer callback
			APP_ERROR_CHECK(NRF_ERROR_INVALID_STATE);
			break;
	}
}


ret_code_t gps_init(gps_callback_t callback)
{
	ret_code_t err_code;

	m_callback = callback;

	gps_config_gpios(false);

	err_code = app_timer_create(&m_gps_reset_timer, APP_TIMER_MODE_SINGLE_SHOT, cb_gps_reset_timer);
	VERIFY_SUCCESS(err_code);

	m_is_powered = false;

	return NRF_SUCCESS;
}


void gps_config_gpios(bool power_supplied)
{
	nrf_gpio_cfg_default(PIN_GPS_RESET_N); // internal pullup in the GPS module
	nrf_gpio_cfg_default(PIN_GPS_WAKEUP);  // not used -> left open
	nrf_gpio_cfg_default(PIN_GPS_PPS);
	nrf_gpio_cfg_default(PIN_GPS_TX);
	nrf_gpio_cfg_default(PIN_GPS_RX);
}


ret_code_t gps_reset(void)
{
	VERIFY_SUCCESS(gps_power_on());

	m_reset_state = GPS_RESET_WAIT1;

	return app_timer_start(m_gps_reset_timer, APP_TIMER_TICKS(GPS_RESET_MS_WAIT1), NULL);
}


ret_code_t gps_power_on(void)
{
	ret_code_t err_code;

	if(m_is_powered) {
		return NRF_SUCCESS;
	}

	// prepare buffers
	m_rx_buffer_used[0] = 0;
	m_rx_buffer_idx = 0;

	// power on
	err_code = periph_pwr_start_activity(PERIPH_PWR_FLAG_GPS);
	VERIFY_SUCCESS(err_code);

	nrfx_uarte_config_t uart_config = NRFX_UARTE_DEFAULT_CONFIG;

	uart_config.baudrate = NRF_UARTE_BAUDRATE_9600;
	uart_config.pselrxd  = PIN_GPS_RX;
	uart_config.pseltxd  = PIN_GPS_TX;
	uart_config.hwfc     = false;

	err_code = nrfx_uarte_init(&m_uarte, &uart_config, cb_uarte);
	VERIFY_SUCCESS(err_code);

	/* Start a 1-byte transfer. When a byte is received, cb_uarte() will be
	 * called, which will start another 1-byte transfer. */
	err_code = nrfx_uarte_rx(
			&m_uarte,
			&m_rx_buffer[m_rx_buffer_idx][ m_rx_buffer_used[m_rx_buffer_idx] ],
			1);
	VERIFY_SUCCESS(err_code);

	m_is_powered = true;

	return NRF_SUCCESS;
}


ret_code_t gps_power_off(void)
{
	ret_code_t err_code;

	m_is_powered = false;

	nrfx_uarte_rx_abort(&m_uarte);
	nrfx_uarte_uninit(&m_uarte);

	err_code = periph_pwr_stop_activity(PERIPH_PWR_FLAG_GPS);
	VERIFY_SUCCESS(err_code);
	return NRF_SUCCESS;
}


void gps_loop(void)
{
	if(m_rx_buffer_complete) {
		m_rx_buffer_complete = false;

		uint8_t len = m_rx_buffer_used[m_rx_buffer_complete_idx];
		uint8_t *buf = m_rx_buffer[m_rx_buffer_complete_idx];

		// ensure that the buffer is safe to print
		if(len >= RX_BUF_SIZE) {
			len = RX_BUF_SIZE - 1;
		}

		buf[len] = '\0';

		//NRF_LOG_INFO("received sentence: %s", NRF_LOG_PUSH((char*)buf));

		bool pos_updated = false;
		nmea_parse((char*)buf, &pos_updated, &m_nmea_data);

		if(pos_updated) {
			m_callback(GPS_EVT_DATA_RECEIVED, &m_nmea_data);
		}
	}
}


ret_code_t gps_cold_restart(void)
{
	if(!m_is_powered) {
		return NRF_ERROR_INVALID_STATE;
	}

	static uint8_t cmd[] = "$PCAS10,2*1E\r\n"; // cold restart (forget everything except configuration)
	return nrfx_uarte_tx(&m_uarte, cmd, strlen((const char*)cmd));
}
