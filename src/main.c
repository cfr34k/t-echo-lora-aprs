/**
 * Copyright (c) 2014 - 2021, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @defgroup ble_sdk_app_template_main main.c
 * @{
 * @ingroup ble_sdk_app_template
 * @brief Template project main file.
 *
 * This file contains a template for creating a new application. It has the code necessary to wakeup
 * from button, advertise, get a connection restart advertising on disconnect and if no new
 * connection created go back to system-off mode.
 * It can easily be used as a starting point for creating a new application, the comments identified
 * with 'YOUR_JOB' indicates where and how you can customize.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_button.h"
#include "bsp.h"
#include "ble_types.h"
#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_bas.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_gpio.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "app_timer.h"
#include "fds.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"
#include "sensorsim.h"
#include "ble_conn_state.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "lns_wrap.h"
#include "aprs_service.h"

#include "pinout.h"
#include "time_base.h"
#include "epaper.h"
#include "gps.h"
#include "lora.h"
#include "voltage_monitor.h"
#include "periph_pwr.h"
#include "leds.h"
#include "buttons.h"
#include "tracker.h"

#include "aprs.h"

#define PROGMEM
#include "fonts/Font_DIN1451Mittel_10.h"


#define DEVICE_NAME                     "T-Echo"                                /**< Name of device. Will be included in the advertising data. */
#define MANUFACTURER_NAME               "HW: Lilygo / FW: cfr34k"               /**< Manufacturer. Will be passed to Device Information Service. */
#define APP_ADV_INTERVAL                300                                     /**< The advertising interval (in units of 0.625 ms. This value corresponds to 187.5 ms). */
#define APP_ADV_DURATION                1000                                   /**< The advertising duration (180 seconds) in units of 10 milliseconds. */
#define APP_ADV_INTERVAL_SLOW           1600                                    /**< The advertising interval (in units of 0.625 ms. This value corresponds to 1 second). */

#define APP_ADV_DURATION_SLOW           0                                       /**< The slow advertising duration (forever) in units of 10 milliseconds. */
#define APP_BLE_OBSERVER_PRIO           3                                       /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define APP_BLE_CONN_CFG_TAG            1                                       /**< A tag identifying the SoftDevice BLE configuration. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(100, UNIT_1_25_MS)        /**< Minimum acceptable connection interval (0.1 seconds). */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(200, UNIT_1_25_MS)        /**< Maximum acceptable connection interval (0.2 second). */
#define SLAVE_LATENCY                   0                                       /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)         /**< Connection supervisory timeout (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                   /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                  /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                       /**< Number of attempts before giving up the connection parameter negotiation. */

#define SEC_PARAM_BOND                  1                                       /**< Perform bonding. */
#define SEC_PARAM_MITM                  0                                       /**< Man In The Middle protection not required. */
#define SEC_PARAM_LESC                  0                                       /**< LE Secure Connections not enabled. */
#define SEC_PARAM_KEYPRESS              0                                       /**< Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES       BLE_GAP_IO_CAPS_NONE                    /**< No I/O capabilities. */
#define SEC_PARAM_OOB                   0                                       /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE          7                                       /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE          16                                      /**< Maximum encryption key size. */

#define DEAD_BEEF                       0xDEADBEEF                              /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define VOLTAGE_MONITOR_INTERVAL_IDLE        3600   // seconds
#define VOLTAGE_MONITOR_INTERVAL_CONNECTED     60   // seconds


NRF_BLE_GATT_DEF(m_gatt);                                                       /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                         /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                             /**< Advertising module instance. */

APP_TIMER_DEF(m_backlight_timer);

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;                        /**< Handle of the current connection. */

static bool m_epaper_update_requested = false;                                  /**< If set to true, the e-paper display will be redrawn ASAP from the main loop. */

static uint8_t  m_bat_percent;
static uint16_t m_bat_millivolt;

static nmea_data_t m_nmea_data;

typedef enum
{
	DISP_STATE_STARTUP,
	DISP_STATE_GPS,
	DISP_STATE_TRACKER,
	DISP_STATE_LORA_PACKET
} display_state_t;

#define DISP_CYCLE_FIRST   DISP_STATE_GPS
#define DISP_CYCLE_LAST    DISP_STATE_LORA_PACKET

static display_state_t m_display_state = DISP_STATE_STARTUP;
static uint8_t m_display_message[256];
static uint8_t m_display_message_len = 0;

static float m_rssi, m_snr, m_signalRssi;

static bool m_lora_rx_active = false;
static bool m_tracker_active = false;

BLE_BAS_DEF(m_ble_bas); // battery service

APRS_SERVICE_DEF(m_aprs_service);

// YOUR_JOB: Use UUIDs for service(s) used in your application.
static ble_uuid_t m_adv_uuids[] =                                               /**< Universally unique service identifiers. */
{
	{BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE}
};


static void advertising_start(bool erase_bonds);


/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num   Line number of the failing ASSERT call.
 * @param[in] file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
	app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


/**@brief Function for handling Peer Manager events.
 *
 * @param[in] p_evt  Peer Manager event.
 */
static void pm_evt_handler(pm_evt_t const * p_evt)
{
	pm_handler_on_pm_evt(p_evt);
	pm_handler_disconnect_on_sec_failure(p_evt);
	pm_handler_flash_clean(p_evt);

	switch (p_evt->evt_id)
	{
		case PM_EVT_PEERS_DELETE_SUCCEEDED:
			advertising_start(false);
			break;

		default:
			break;
	}
}


/**@brief Timeout handler for the LED backlight.
 */
void cb_backlight_timer(void *arg)
{
	led_off(LED_EPAPER_BACKLIGHT);
}


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
	// Initialize timer module.
	ret_code_t err_code = app_timer_init();
	APP_ERROR_CHECK(err_code);

	// Create timers.

	/* YOUR_JOB: Create any timers to be used by the application.
	   Below is an example of how to create a timer.
	   For every new timer needed, increase the value of the macro APP_TIMER_MAX_TIMERS by
	   one.
	   ret_code_t err_code;
	   err_code = app_timer_create(&m_app_timer_id, APP_TIMER_MODE_REPEATED, timer_timeout_handler);
	   APP_ERROR_CHECK(err_code); */

	err_code = app_timer_create(&m_backlight_timer, APP_TIMER_MODE_SINGLE_SHOT, cb_backlight_timer);
	APP_ERROR_CHECK(err_code);
}


/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
	ret_code_t              err_code;
	ble_gap_conn_params_t   gap_conn_params;
	ble_gap_conn_sec_mode_t sec_mode;

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

	err_code = sd_ble_gap_device_name_set(&sec_mode,
			(const uint8_t *)DEVICE_NAME,
			strlen(DEVICE_NAME));
	APP_ERROR_CHECK(err_code);

	err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_GENERIC_TAG);
	APP_ERROR_CHECK(err_code);

	memset(&gap_conn_params, 0, sizeof(gap_conn_params));

	gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
	gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
	gap_conn_params.slave_latency     = SLAVE_LATENCY;
	gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

	err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
	APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the GATT module.
*/
static void gatt_init(void)
{
	ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
	APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
	APP_ERROR_HANDLER(nrf_error);
}


static void cb_aprs_service(aprs_service_evt_t evt)
{
	switch(evt)
	{
		case APRS_SERVICE_EVT_MYCALL_CHANGED:
			{
				char mycall[16];

				APP_ERROR_CHECK(aprs_service_get_mycall(&m_aprs_service, mycall, sizeof(mycall)));

				aprs_set_source(mycall, 0);
			}
			break;

		case APRS_SERVICE_EVT_COMMENT_CHANGED:
			{
				char comment[64];

				APP_ERROR_CHECK(aprs_service_get_comment(&m_aprs_service, comment, sizeof(comment)));

				aprs_set_comment(comment);
			}
			break;
	}
}


/**@brief Function for initializing services that will be used by the application.
*/
static void services_init(void)
{
	ret_code_t         err_code;
	nrf_ble_qwr_init_t qwr_init = {0};

	// Initialize Queued Write Module.
	qwr_init.error_handler = nrf_qwr_error_handler;

	err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
	APP_ERROR_CHECK(err_code);

	// Initialize the Battery Service
	ble_bas_init_t bas_init;

	memset(&bas_init, 0, sizeof(bas_init));

	bas_init.evt_handler          = NULL;
	bas_init.support_notification = true;
	bas_init.p_report_ref         = NULL;
	bas_init.initial_batt_level   = 0;

	bas_init.bl_rd_sec            = SEC_OPEN;
	bas_init.bl_cccd_wr_sec       = SEC_OPEN;
	bas_init.bl_report_rd_sec     = SEC_OPEN;

	err_code = ble_bas_init(&m_ble_bas, &bas_init);
	APP_ERROR_CHECK(err_code);

	// Initialize the Location and Navigation Service
	err_code = lns_wrap_init();
	APP_ERROR_CHECK(err_code);

	// Initialize the APRS Service
	aprs_service_init_t aprs_init;

	memset(&aprs_init, 0, sizeof(aprs_init));

	aprs_init.callback = cb_aprs_service;

	err_code = aprs_service_init(&m_aprs_service, &aprs_init);
	APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
	ret_code_t err_code;

	if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
	{
		err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
		APP_ERROR_CHECK(err_code);
	}
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
	APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
*/
static void conn_params_init(void)
{
	ret_code_t             err_code;
	ble_conn_params_init_t cp_init;

	memset(&cp_init, 0, sizeof(cp_init));

	cp_init.p_conn_params                  = NULL;
	cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
	cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
	cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
	cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
	cp_init.disconnect_on_fail             = false;
	cp_init.evt_handler                    = on_conn_params_evt;
	cp_init.error_handler                  = conn_params_error_handler;

	err_code = ble_conn_params_init(&cp_init);
	APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting timers.
*/
static void application_timers_start(void)
{
	/* YOUR_JOB: Start your timers. below is an example of how to start a timer.
	   ret_code_t err_code;
	   err_code = app_timer_start(m_app_timer_id, TIMER_INTERVAL, NULL);
	   APP_ERROR_CHECK(err_code); */

}


/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
	ret_code_t err_code;

	// switch off all external peripherals
	periph_pwr_stop_activity(PERIPH_PWR_FLAG_ALL);

	err_code = bsp_indication_set(BSP_INDICATE_IDLE);
	APP_ERROR_CHECK(err_code);

	// Prepare wakeup buttons.
	/*err_code = bsp_btn_ble_sleep_mode_prepare();
	APP_ERROR_CHECK(err_code);*/

	// Go to system-off mode (this function will not return; wakeup will cause a reset).
	err_code = sd_power_system_off();
	APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
	ret_code_t err_code;

	switch (ble_adv_evt)
	{
		case BLE_ADV_EVT_FAST:
			NRF_LOG_INFO("Fast advertising.");
			periph_pwr_start_activity(PERIPH_PWR_FLAG_LEDS);
			err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
			APP_ERROR_CHECK(err_code);
			break;

		case BLE_ADV_EVT_SLOW:
			NRF_LOG_INFO("Slow advertising.");
			periph_pwr_stop_activity(PERIPH_PWR_FLAG_LEDS);
			err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING_SLOW);
			APP_ERROR_CHECK(err_code);
			break;

		case BLE_ADV_EVT_IDLE:
			sleep_mode_enter();
			break;

		default:
			break;
	}
}


/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
	ret_code_t err_code = NRF_SUCCESS;

	switch (p_ble_evt->header.evt_id)
	{
		case BLE_GAP_EVT_DISCONNECTED:
			NRF_LOG_INFO("Disconnected.");
			// LED indication will be changed when advertising starts.

			periph_pwr_stop_activity(PERIPH_PWR_FLAG_CONNECTED);

			voltage_monitor_stop();
			voltage_monitor_start(VOLTAGE_MONITOR_INTERVAL_IDLE);
			break;

		case BLE_GAP_EVT_CONNECTED:
			NRF_LOG_INFO("Connected.");
			err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
			APP_ERROR_CHECK(err_code);
			m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
			err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
			APP_ERROR_CHECK(err_code);

			// enable external peripherals
			periph_pwr_start_activity(PERIPH_PWR_FLAG_LEDS);
			periph_pwr_start_activity(PERIPH_PWR_FLAG_CONNECTED);

			voltage_monitor_stop();
			voltage_monitor_start(VOLTAGE_MONITOR_INTERVAL_CONNECTED);
			break;

		case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
			{
				NRF_LOG_DEBUG("PHY update request.");
				ble_gap_phys_t const phys =
				{
					.rx_phys = BLE_GAP_PHY_AUTO,
					.tx_phys = BLE_GAP_PHY_AUTO,
				};
				err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
				APP_ERROR_CHECK(err_code);
			} break;

		case BLE_GATTC_EVT_TIMEOUT:
			// Disconnect on GATT Client timeout event.
			NRF_LOG_DEBUG("GATT Client Timeout.");
			err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
					BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
			APP_ERROR_CHECK(err_code);
			break;

		case BLE_GATTS_EVT_TIMEOUT:
			// Disconnect on GATT Server timeout event.
			NRF_LOG_DEBUG("GATT Server Timeout.");
			err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
					BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
			APP_ERROR_CHECK(err_code);
			break;

		default:
			// No implementation needed.
			break;
	}
}


/**@brief Callback function for the voltage monitor. */
static void cb_voltage_monitor(int16_t *meas_millivolt, uint8_t bat_percent)
{
	ret_code_t err_code;

	m_bat_percent = bat_percent;
	m_bat_millivolt = meas_millivolt[0];

	m_epaper_update_requested = true;

	NRF_LOG_INFO("VBAT measured: %d mV (-> %d %%)", meas_millivolt[0], bat_percent);

	err_code = ble_bas_battery_level_update(&m_ble_bas, bat_percent, BLE_CONN_HANDLE_ALL);

	switch(err_code)
	{
		case NRF_ERROR_INVALID_STATE:
		case NRF_ERROR_RESOURCES:
		case NRF_ERROR_BUSY:
		case BLE_ERROR_GATTS_SYS_ATTR_MISSING:
			/* these may happen during normal operation */
			break;

		default:
			APP_ERROR_CHECK(err_code);
			break;
	}
}

/**@brief Callback function for the GPS. */
static void cb_gps(const nmea_data_t *data)
{
	// make a copy for display rendering
	m_nmea_data = *data;

	//APP_ERROR_CHECK(lns_wrap_update_data(data));

	if(m_tracker_active) {
		tracker_run(data);
	}
}


void cb_lora(lora_evt_t evt, const lora_evt_data_t *data)
{
	ret_code_t err_code;

	switch(evt)
	{
		case LORA_EVT_PACKET_RECEIVED:
			m_display_state = DISP_STATE_LORA_PACKET;
			memcpy(m_display_message, data->rx_packet_data.data, data->rx_packet_data.data_len);
			m_display_message_len = data->rx_packet_data.data_len;

			m_rssi = data->rx_packet_data.rssi;
			m_signalRssi = data->rx_packet_data.signalRssi;
			m_snr = data->rx_packet_data.snr;

			m_epaper_update_requested = true;

			err_code = aprs_service_notify_rx_message(
					&m_aprs_service,
					m_conn_handle,
					data->rx_packet_data.data,
					data->rx_packet_data.data_len);

			switch(err_code) {
				case NRF_ERROR_RESOURCES:
				case NRF_ERROR_INVALID_STATE:
				case NRF_ERROR_BUSY:
				case NRF_ERROR_FORBIDDEN:
				case BLE_ERROR_GATTS_SYS_ATTR_MISSING:
					// these may happen in normal operation -> do nothing
					break;

				default:
					// check all other error codes
					APP_ERROR_CHECK(err_code);
					break;
			}
			break;

		case LORA_EVT_CONFIGURED_IDLE:
			if(m_lora_rx_active) {
				lora_start_rx();
			} else {
				lora_power_off();
			}
			break;

		default:
			break;
	}
}


/**@brief Tracker event handler.
 */
static void cb_tracker(tracker_evt_t evt)
{
	switch(evt) {
		case TRACKER_EVT_TRANSMISSION_STARTED:
			m_epaper_update_requested = true;
			break;
	}
}


/**@brief Buttons callback.
 */
void cb_buttons(uint8_t pin, uint8_t evt)
{
	switch(pin)
	{
		case PIN_BTN_TOUCH:
			if(evt == APP_BUTTON_PUSH) {
				APP_ERROR_CHECK(app_timer_stop(m_backlight_timer));
				led_on(LED_EPAPER_BACKLIGHT);
				APP_ERROR_CHECK(app_timer_start(m_backlight_timer, APP_TIMER_TICKS(3000), NULL));
			} else if(evt == BUTTONS_EVT_LONGPRESS) {
				m_lora_rx_active = !m_lora_rx_active;
				if(m_lora_rx_active) {
					lora_start_rx();
				} else {
					lora_power_off();
				}
			}
			break;

		case PIN_BUTTON_1:
			if(evt == APP_BUTTON_PUSH) {
				if(m_display_state == DISP_CYCLE_LAST) {
					m_display_state = DISP_CYCLE_FIRST;
				} else {
					m_display_state++;
				}

				m_epaper_update_requested = true;
			} else if(evt == BUTTONS_EVT_LONGPRESS) {
				m_tracker_active = !m_tracker_active;
				if(m_tracker_active) {
					APP_ERROR_CHECK(gps_power_on());
				} else {
					APP_ERROR_CHECK(gps_power_off());
				}

				m_display_state = DISP_STATE_TRACKER;
				m_epaper_update_requested = true;
			}
			break;
	}
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
	ret_code_t err_code;

	err_code = nrf_sdh_enable_request();
	APP_ERROR_CHECK(err_code);

	// Configure the BLE stack using the default settings.
	// Fetch the start address of the application RAM.
	uint32_t ram_start = 0;
	err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
	APP_ERROR_CHECK(err_code);

	// Enable BLE stack.
	err_code = nrf_sdh_ble_enable(&ram_start);
	APP_ERROR_CHECK(err_code);

	// Register a handler for BLE events.
	NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}


/**@brief Function for the Peer Manager initialization.
*/
static void peer_manager_init(void)
{
	ble_gap_sec_params_t sec_param;
	ret_code_t           err_code;

	err_code = pm_init();
	APP_ERROR_CHECK(err_code);

	memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));

	// Security parameters to be used for all security procedures.
	sec_param.bond           = SEC_PARAM_BOND;
	sec_param.mitm           = SEC_PARAM_MITM;
	sec_param.lesc           = SEC_PARAM_LESC;
	sec_param.keypress       = SEC_PARAM_KEYPRESS;
	sec_param.io_caps        = SEC_PARAM_IO_CAPABILITIES;
	sec_param.oob            = SEC_PARAM_OOB;
	sec_param.min_key_size   = SEC_PARAM_MIN_KEY_SIZE;
	sec_param.max_key_size   = SEC_PARAM_MAX_KEY_SIZE;
	sec_param.kdist_own.enc  = 1;
	sec_param.kdist_own.id   = 1;
	sec_param.kdist_peer.enc = 1;
	sec_param.kdist_peer.id  = 1;

	err_code = pm_sec_params_set(&sec_param);
	APP_ERROR_CHECK(err_code);

	err_code = pm_register(pm_evt_handler);
	APP_ERROR_CHECK(err_code);
}


/**@brief Clear bond information from persistent storage.
*/
static void delete_bonds(void)
{
	ret_code_t err_code;

	NRF_LOG_INFO("Erase bonds!");

	err_code = pm_peers_delete();
	APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated when button is pressed.
 */
static void bsp_event_handler(bsp_event_t event)
{
	ret_code_t err_code;

	switch (event)
	{
		case BSP_EVENT_SLEEP:
			sleep_mode_enter();
			break; // BSP_EVENT_SLEEP

		case BSP_EVENT_DISCONNECT:
			err_code = sd_ble_gap_disconnect(m_conn_handle,
					BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
			if (err_code != NRF_ERROR_INVALID_STATE)
			{
				APP_ERROR_CHECK(err_code);
			}
			break; // BSP_EVENT_DISCONNECT

		case BSP_EVENT_WHITELIST_OFF:
			if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
			{
				err_code = ble_advertising_restart_without_whitelist(&m_advertising);
				if (err_code != NRF_ERROR_INVALID_STATE)
				{
					APP_ERROR_CHECK(err_code);
				}
			}
			break; // BSP_EVENT_KEY_0

		default:
			break;
	}
}


/**@brief Function for initializing the Advertising functionality.
*/
static void advertising_init(void)
{
	ret_code_t             err_code;
	ble_advertising_init_t init;

	memset(&init, 0, sizeof(init));

	init.advdata.name_type               = BLE_ADVDATA_FULL_NAME;
	init.advdata.include_appearance      = true;
	init.advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
	init.advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
	init.advdata.uuids_complete.p_uuids  = m_adv_uuids;

	init.config.ble_adv_fast_enabled  = true;
	init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
	init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;

	init.config.ble_adv_slow_enabled  = true;
	init.config.ble_adv_slow_interval = APP_ADV_INTERVAL_SLOW;
	init.config.ble_adv_slow_timeout  = APP_ADV_DURATION_SLOW;

	init.evt_handler = on_adv_evt;

	err_code = ble_advertising_init(&m_advertising, &init);
	APP_ERROR_CHECK(err_code);

	ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}


/**@brief Function for initializing buttons and leds.
 */
static void buttons_leds_init(void)
{
	ret_code_t err_code;

	err_code = bsp_init(BSP_INIT_LEDS, bsp_event_handler);
	APP_ERROR_CHECK(err_code);

	leds_init();
	buttons_init(cb_buttons);
}


/**@brief Function for initializing the nrf log module.
*/
static void log_init(void)
{
	ret_code_t err_code = NRF_LOG_INIT(NULL);
	APP_ERROR_CHECK(err_code);

	NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for initializing power management.
*/
static void power_management_init(void)
{
	ret_code_t err_code;
	err_code = nrf_pwr_mgmt_init();
	APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
	if (NRF_LOG_PROCESS() == false)
	{
		nrf_pwr_mgmt_run();
	}
}


/**@brief Function for starting advertising.
*/
static void advertising_start(bool erase_bonds)
{
	if (erase_bonds == true)
	{
		delete_bonds();
		// Advertising is started by PM_EVT_PEERS_DELETED_SUCEEDED event
	}
	else
	{
		ret_code_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);

		APP_ERROR_CHECK(err_code);
	}
}


/**@brief Initialize GPIOs not controlled by any module, but used by the bootloader.
*/
static void gpio_init(void)
{
	nrf_gpio_cfg_default(PIN_LED_RED);
	nrf_gpio_cfg_default(PIN_LED_GREEN);
	nrf_gpio_cfg_default(PIN_LED_BLUE);
	nrf_gpio_cfg_default(PIN_BUTTON_1);
	//nrf_gpio_cfg_default(PIN_BUTTON_2); // not sure about this, because it is the reset pin
	nrf_gpio_cfg_default(PIN_BUTTON_3);
}


static void format_float(char *s, size_t s_len, float f, uint8_t decimals)
{
	char fmt[32];

	int32_t factor = 1;

	for(uint8_t i = 0; i < decimals; i++) {
		factor *= 10;
	}

	snprintf(fmt, sizeof(fmt), "%%s%%d.%%0%dd", decimals);

	snprintf(s, s_len, fmt,
			(f < 0 && f > -1.0) ? "-" : "",
			(int32_t)f,
			(int32_t)(((f > 0) ? (f - (int32_t)f) : ((int32_t)f - f)) * factor));
}


/**@brief Redraw the e-Paper display.
 */
static void redraw_display(void)
{
	char s[32];
	char tmp1[16], tmp2[16], tmp3[16];

	uint8_t line_height = epaper_fb_get_line_height();
	uint8_t yoffset = line_height;

	epaper_fb_clear(EPAPER_COLOR_WHITE);

	if(m_display_state != DISP_STATE_STARTUP) {
		epaper_fb_move_to(0, yoffset);

		snprintf(s, sizeof(s), "BAT: %d.%02d V / %d %%",
				m_bat_millivolt / 1000,
				(m_bat_millivolt / 10) % 100,
				m_bat_percent);

		epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

		yoffset += line_height + line_height/2;
	}

	epaper_fb_move_to(0, yoffset);

	switch(m_display_state)
	{
		case DISP_STATE_STARTUP:
			// some fun
			epaper_fb_move_to( 50, 150);
			epaper_fb_line_to(150, 150, EPAPER_COLOR_BLACK); // Das
			epaper_fb_line_to(150,  70, EPAPER_COLOR_BLACK); // ist
			epaper_fb_line_to( 50, 150, EPAPER_COLOR_BLACK); // das
			epaper_fb_line_to( 50,  70, EPAPER_COLOR_BLACK); // Haus
			epaper_fb_line_to(150,  70, EPAPER_COLOR_BLACK); // vom
			epaper_fb_line_to(100,  10, EPAPER_COLOR_BLACK); // Ni-
			epaper_fb_line_to( 50,  70, EPAPER_COLOR_BLACK); // ko-
			epaper_fb_line_to(150, 150, EPAPER_COLOR_BLACK); // laus

			epaper_fb_move_to(100, 90);
			epaper_fb_circle(80, EPAPER_COLOR_BLACK);

			epaper_fb_set_font(&din1451m10pt7b);
			epaper_fb_move_to(0, 180);
			epaper_fb_draw_string("Lora-APRS " VERSION, EPAPER_COLOR_BLACK);
			break;

		case DISP_STATE_GPS:
			epaper_fb_draw_string("GNSS-Status:", EPAPER_COLOR_BLACK);

			yoffset += line_height;
			epaper_fb_move_to(0, yoffset);

			if(m_nmea_data.pos_valid) {
				format_float(tmp1, sizeof(tmp1), m_nmea_data.lat, 6);
				snprintf(s, sizeof(s), "Lat: %s", tmp1);

				epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

				epaper_fb_move_to(150, yoffset);
				epaper_fb_draw_string("Alt:", EPAPER_COLOR_BLACK);

				yoffset += line_height;
				epaper_fb_move_to(0, yoffset);

				format_float(tmp1, sizeof(tmp1), m_nmea_data.lon, 6);
				snprintf(s, sizeof(s), "Lon: %s", tmp1);

				epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

				epaper_fb_move_to(150, yoffset);
				snprintf(s, sizeof(s), "%d", (int)(m_nmea_data.altitude + 0.5f));
				epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);
			} else {
				epaper_fb_draw_string("No fix :-(", EPAPER_COLOR_BLACK);
			}

			yoffset += line_height + line_height/2;
			epaper_fb_move_to(0, yoffset);

			for(uint8_t i = 0; i < NMEA_NUM_FIX_INFO; i++) {
				nmea_fix_info_t *fix_info = &(m_nmea_data.fix_info[i]);

				if(fix_info->sys_id == NMEA_SYS_ID_INVALID) {
					continue;
				}

				snprintf(s, sizeof(s), "%s: %s [%s] Sats: %d",
						nmea_sys_id_to_short_name(fix_info->sys_id),
						nmea_fix_type_to_string(fix_info->fix_type),
						fix_info->auto_mode ? "auto" : "man",
						fix_info->sats_used);

				epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

				yoffset += line_height;
				epaper_fb_move_to(0, yoffset);
			}

			format_float(tmp1, sizeof(tmp1), m_nmea_data.hdop, 1);
			format_float(tmp2, sizeof(tmp2), m_nmea_data.vdop, 1);
			format_float(tmp3, sizeof(tmp3), m_nmea_data.pdop, 1);

			snprintf(s, sizeof(s), "DOP H: %s V: %s P: %s",
					tmp1, tmp2, tmp3);

			epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

			yoffset += line_height;
			epaper_fb_move_to(0, yoffset);
			break;

		case DISP_STATE_TRACKER:
			snprintf(s, sizeof(s), "Tracker %s.",
					m_tracker_active ? "running" : "stopped");

			epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

			yoffset += line_height + line_height / 2;
			epaper_fb_move_to(0, yoffset);

			if(m_nmea_data.pos_valid) {
				format_float(tmp1, sizeof(tmp1), m_nmea_data.hdop, 1);
				format_float(tmp2, sizeof(tmp2), m_nmea_data.vdop, 1);
				format_float(tmp3, sizeof(tmp3), m_nmea_data.pdop, 1);

				snprintf(s, sizeof(s), "DOP H: %s V: %s P: %s",
						tmp1, tmp2, tmp3);

				epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

				yoffset += line_height;
				epaper_fb_move_to(0, yoffset);

				format_float(tmp1, sizeof(tmp1), m_nmea_data.lat, 5);
				format_float(tmp2, sizeof(tmp2), m_nmea_data.lon, 5);

				snprintf(s, sizeof(s), "%s/%s, %d m",
						tmp1, tmp2, (int)(m_nmea_data.altitude + 0.5));

				epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);
			} else {
				epaper_fb_draw_string("No fix :-(", EPAPER_COLOR_BLACK);
			}

			yoffset += line_height;
			epaper_fb_move_to(0, yoffset);

			if(m_nmea_data.speed_heading_valid) {
				format_float(tmp1, sizeof(tmp1), m_nmea_data.speed, 1);

				snprintf(s, sizeof(s), "%s m/s - %d deg",
						tmp1, (int)(m_nmea_data.heading + 0.5));

				epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);
			} else {
				epaper_fb_draw_string("No speed / heading info.", EPAPER_COLOR_BLACK);
			}

			yoffset += line_height;
			epaper_fb_move_to(0, yoffset);
			break;

		case DISP_STATE_LORA_PACKET:
			{
				char line[32];
				char *lineptr = line;

				size_t len = m_display_message_len;
				if(len > 7*18) {
					len = 7*18;
				}

				for(uint8_t i = 0; i < len; i++) {
					char c = m_display_message[i];
					if(c < 0x20) {
						c = '.'; // substitude non-printable characters
					}

					*(lineptr++) = c;

					if((i % 18) == 17) {
						*lineptr = '\0';

						epaper_fb_draw_string(line, EPAPER_COLOR_BLACK);
						yoffset += line_height;
						epaper_fb_move_to(0, yoffset);

						lineptr = line;
					}
				}

				if(lineptr != line) {
					*lineptr = '\0';

					epaper_fb_draw_string(line, EPAPER_COLOR_BLACK);
					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);
				}

				snprintf(line, sizeof(line), "R: -%d.%01d / %d.%02d / -%d.%01d",
						(int)(-m_rssi),
						(int)(10 * ((-m_rssi) - (int)(-m_rssi))),
						(int)(m_snr),
						(int)(100 * ((m_snr) - (int)(m_snr))),
						(int)(-m_signalRssi),
						(int)(10 * ((-m_signalRssi) - (int)(-m_signalRssi))));

				epaper_fb_draw_string(line, EPAPER_COLOR_BLACK);
				yoffset += line_height;
				epaper_fb_move_to(0, yoffset);
			}
			break;
	}

	epaper_update();
}


/**@brief Function for application main entry.
*/
int main(void)
{
	// Initialize.
	log_init();
	gpio_init();
	timers_init();
	power_management_init();
	ble_stack_init();
	gap_params_init();
	gatt_init();
	advertising_init();
	services_init();
	conn_params_init();
	peer_manager_init();
	buttons_leds_init();

	periph_pwr_init();
	time_base_init();
	epaper_init();
	gps_init(cb_gps);
	lora_init(cb_lora);
	aprs_init();
	tracker_init(cb_tracker);

	voltage_monitor_init(cb_voltage_monitor);

	// Start execution.
	NRF_LOG_INFO("Template example started.");
	application_timers_start();

	voltage_monitor_start(VOLTAGE_MONITOR_INTERVAL_IDLE);

	// Initial APRS setup
	aprs_set_source("DL5TKL", 4);
	aprs_set_dest("APLT01", 0);

	aprs_clear_path();
	//aprs_add_path("WIDE2", 2);

	aprs_set_comment("T-Echo");

	aprs_set_icon(AI_X);

	m_display_state = DISP_STATE_STARTUP;
	redraw_display();

	m_display_state = DISP_STATE_GPS;

	bool first_redraw = true;

	// Enter main loop.
	for (;;)
	{
		if(m_epaper_update_requested && !epaper_is_busy()) {
			m_epaper_update_requested = false;

			if(first_redraw) {
				first_redraw = false;

				bool erase_bonds = buttons_button_is_pressed(BUTTONS_BTN_1);
				advertising_start(erase_bonds);
			}

			redraw_display();
		}

		epaper_loop();
		gps_loop();
		lora_loop();

		idle_state_handle();
	}
}


/**
 * @}
 */
