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
 * @author Thomas Kolb <cfr34k-git@tkolb.de>
 * @date   2022-06-08
 * @brief Main file for the T-Echo LoRa-APRS tracker firmware.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

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
#include "wall_clock.h"
#include "epaper.h"
#include "gps.h"
#include "lora.h"
#include "voltage_monitor.h"
#include "periph_pwr.h"
#include "leds.h"
#include "buttons.h"
#include "tracker.h"
#include "utils.h"
#include "settings.h"
#include "menusystem.h"
#include "display.h"
#include "bme280.h"

#include "aprs.h"

#include "config.h"


#define DEVICE_NAME                     "T-Echo"                                /**< Name of device. Will be included in the advertising data. */
#define MANUFACTURER_NAME               "HW: Lilygo / FW: cfr34k"               /**< Manufacturer. Will be passed to Device Information Service. */
#define APP_ADV_INTERVAL                300                                     /**< The advertising interval (in units of 0.625 ms. This value corresponds to 187.5 ms). */
#define APP_ADV_DURATION                18000                                   /**< The advertising duration (180 seconds) in units of 10 milliseconds. */
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
#define SEC_PARAM_IO_CAPABILITIES       BLE_GAP_IO_CAPS_DISPLAY_ONLY            /**< Only display, no keyboard. */
#define SEC_PARAM_OOB                   0                                       /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE          7                                       /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE          16                                      /**< Maximum encryption key size. */

#define DEAD_BEEF                       0xDEADBEEF                              /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define VOLTAGE_MONITOR_INTERVAL_IDLE        3600   // seconds
#define VOLTAGE_MONITOR_INTERVAL_ACTIVE        60   // seconds

#define SHUTDOWN_FLAG_INITIATED (1 << 0)
#define SHUTDOWN_FLAG_DISPLAY_LOCKED (1 << 1)
#define SHUTDOWN_FLAG_DISPLAY_CLEARED (1 << 2)
#define SHUTDOWN_FLAG_LORA_OFF (1 << 3)

#define SHUTDOWN_FLAG_ALL_SET 0x0F

NRF_BLE_GATT_DEF(m_gatt);                                                       /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                         /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                             /**< Advertising module instance. */

APP_TIMER_DEF(m_backlight_timer);
APP_TIMER_DEF(m_lowspeed_tick_timer);
APP_TIMER_DEF(m_startup_timer);

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;                        /**< Handle of the current connection. */

static bool m_epaper_update_requested = false;                                  /**< If set to true, the e-paper display will be redrawn ASAP from the main loop. */
static bool m_epaper_force_full_refresh = false;                                /**< e-Paper needs a full refresh from time to time to get rid of ghosting. */

static bool m_bme280_updated = false;
static uint64_t m_bme280_next_readout_time = 0;

uint8_t  m_bat_percent;
uint16_t m_bat_millivolt;

nmea_data_t m_nmea_data;
bool m_nmea_has_position = false;

#define DISP_CYCLE_FIRST   DISP_STATE_GPS
#define DISP_CYCLE_LAST    DISP_STATE_CLOCK_BME280

display_state_t m_display_state = DISP_STATE_STARTUP;
display_state_t m_prev_display_state = DISP_CYCLE_FIRST;
uint8_t         m_display_rx_index = 0;

aprs_rx_raw_data_t m_last_undecodable_data;
uint64_t m_last_undecodable_timestamp;

bool m_lora_rx_busy = false;
bool m_lora_tx_busy = false;

bool m_lora_rx_active = false;
bool m_tracker_active = false;
bool m_gnss_keep_active = false;

char m_passkey[6];

static uint8_t m_shutdown_flags = 0x0;


BLE_BAS_DEF(m_ble_bas); // battery service

APRS_SERVICE_DEF(m_aprs_service);

// YOUR_JOB: Use UUIDs for service(s) used in your application.
static ble_uuid_t m_adv_uuids[] =                                               /**< Universally unique service identifiers. */
{
	{BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE}
};


static void advertising_start(bool erase_bonds);
static void cb_menusystem(menusystem_evt_t evt, const menusystem_evt_data_t *data);


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
static void cb_backlight_timer(void *arg)
{
	led_off(LED_EPAPER_BACKLIGHT);
}


void readout_bme280_if_already_powered(void)
{
	uint64_t now = time_base_get();

	if(bme280_is_ready()
			&& periph_pwr_is_activity_power_already_available(PERIPH_PWR_FLAG_BME280)
			&& (now >= m_bme280_next_readout_time)) {
		APP_ERROR_CHECK(bme280_start_readout());
		m_bme280_next_readout_time = now + 15000; // only refresh once per minute
	}
}


/**@brief Timeout handler for low-frequency background jobs
 *
 * This timer handles various background jobs that are executed at very low
 * rate. The timer is executed every 15 seconds.
 *
 * Currently implemented jobs are:
 *
 * - Trigger a full e-Paper refresh every 1 hour.
 * - Trigger a BME280 readout every tick, but only if it is powered already.
 * - Update the display on every tick, but only if it is powered already.
 */
static void cb_lowspeed_tick_timer(void *arg)
{
	static uint32_t tick_count = 1;

	readout_bme280_if_already_powered();

	if(tick_count % 240 == 0) {
		m_epaper_force_full_refresh = true;
		m_epaper_update_requested = true;
	}

	// refresh epaper if power is on anyways
	if(periph_pwr_is_activity_power_already_available(PERIPH_PWR_FLAG_EPAPER_UPDATE)) {
		m_epaper_update_requested = true;
	}

	tick_count++;
}


/**@brief Startup timer.
 * @details
 * Clears the startup screen and starts delayed actions.
 */
static void cb_startup_timer(void *arg)
{
	m_display_state = DISP_CYCLE_FIRST;
	m_epaper_update_requested = true;
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

	err_code = app_timer_create(&m_backlight_timer, APP_TIMER_MODE_SINGLE_SHOT, cb_backlight_timer);
	APP_ERROR_CHECK(err_code);

	err_code = app_timer_create(&m_lowspeed_tick_timer, APP_TIMER_MODE_REPEATED, cb_lowspeed_tick_timer);
	APP_ERROR_CHECK(err_code);

	err_code = app_timer_create(&m_startup_timer, APP_TIMER_MODE_SINGLE_SHOT, cb_startup_timer);
	APP_ERROR_CHECK(err_code);
}


/**@brief Set device name.
 */
static void gap_update_device_name(void)
{
	ret_code_t              err_code;
	ble_gap_conn_sec_mode_t sec_mode;

	char devname[BLE_GAP_DEVNAME_DEFAULT_LEN + 1];

	strcpy(devname, DEVICE_NAME);
	strcat(devname, " ");

	size_t len = strlen(devname);
	aprs_get_source(devname + len, sizeof(devname) - len - 1);

	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&sec_mode);

	err_code = sd_ble_gap_device_name_set(&sec_mode,
			(const uint8_t *)devname,
			strlen(devname));
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

	gap_update_device_name();

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


static void notify_current_setting_value(settings_id_t id, bool op_success)
{
	uint8_t value[256];
	size_t value_len = sizeof(value);

	ret_code_t err_code = settings_query(id, value, &value_len);

	if(err_code == NRF_SUCCESS) {
		aprs_service_notify_setting(
				&m_aprs_service, m_conn_handle,
				id, op_success, value, (uint16_t)value_len);
	} else {
		// setting could not be retrieved
		aprs_service_notify_setting(
				&m_aprs_service, m_conn_handle,
				id, false, NULL, 0);
	}
}


static void cb_aprs_service(const aprs_service_evt_t *evt)
{
	switch(evt->type)
	{
		case APRS_SERVICE_EVT_MYCALL_CHANGED:
			{
				char mycall[16];

				APP_ERROR_CHECK(aprs_service_get_mycall(&m_aprs_service, mycall, sizeof(mycall)));

				settings_write(SETTINGS_ID_SOURCE_CALL, (const uint8_t *)mycall, strlen(mycall)+1);
				aprs_set_source(mycall);
			}
			break;

		case APRS_SERVICE_EVT_COMMENT_CHANGED:
			{
				char comment[64];

				APP_ERROR_CHECK(aprs_service_get_comment(&m_aprs_service, comment, sizeof(comment)));

				settings_write(SETTINGS_ID_COMMENT, (const uint8_t *)comment, strlen(comment)+1);
				aprs_set_comment(comment);
			}
			break;

		case APRS_SERVICE_EVT_SYMBOL_CHANGED:
			{
				char table, symbol;

				APP_ERROR_CHECK(aprs_service_get_symbol(&m_aprs_service, &table, &symbol));

				// save the new symbol code as last custom symbol code. The
				// SETTINGS_ID_SYMBOL_CODE setting is updated from
				// cb_settings() once this write completes.
				uint8_t buf[2] = {table, symbol};
				settings_write(SETTINGS_ID_LAST_BLE_SYMBOL, buf, sizeof(buf));

				aprs_set_icon(table, symbol);
			}
			break;

		case APRS_SERVICE_EVT_SETTING_SELECT:
			notify_current_setting_value(evt->params.setting.setting_id, true);
			break;

		case APRS_SERVICE_EVT_SETTING_WRITE:
			{
				// try to write the new setting value
				ret_code_t err_code = settings_write(
						evt->params.setting.setting_id,
						evt->params.setting.data,
						evt->params.setting.data_len);

				if(err_code == NRF_SUCCESS) {
					aprs_service_notify_setting(
							&m_aprs_service, m_conn_handle,
							evt->params.setting.setting_id,
							true,
							evt->params.setting.data,
							evt->params.setting.data_len);
				} else {
					// setting could not be written. Notify the currently stored value.
					notify_current_setting_value(evt->params.setting.setting_id, false);
				}
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
	ret_code_t err_code;

	err_code = app_timer_start(m_lowspeed_tick_timer, APP_TIMER_TICKS(15000), NULL);
	APP_ERROR_CHECK(err_code);

	err_code = app_timer_start(m_startup_timer, APP_TIMER_TICKS(6000), NULL);
	APP_ERROR_CHECK(err_code);
}


/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
	ret_code_t err_code;

	// switch off all LEDs
	led_off(LED_EPAPER_BACKLIGHT);
	led_off(LED_RED);
	led_off(LED_GREEN);
	led_off(LED_BLUE);

	// disable all buttons (including wakeup). Only Reset will wake the device.
	err_code = bsp_buttons_disable();
	APP_ERROR_CHECK(err_code);

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

			if(m_display_state == DISP_STATE_PASSKEY) {
				m_display_state = m_prev_display_state;
				m_epaper_update_requested = true;
			}
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

		case BLE_GAP_EVT_PASSKEY_DISPLAY:
			memcpy(m_passkey, p_ble_evt->evt.gap_evt.params.passkey_display.passkey, sizeof(m_passkey));

			m_prev_display_state = m_display_state;
			m_display_state = DISP_STATE_PASSKEY;
			m_epaper_update_requested = true;
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
static void cb_gps(gps_evt_t evt, const nmea_data_t *data)
{
	switch(evt) {
		case GPS_EVT_RESET_COMPLETE:
			APP_ERROR_CHECK(gps_power_off());
			break;

		case GPS_EVT_DATA_RECEIVED:
			// make a copy for display rendering
			m_nmea_data = *data;
			m_nmea_has_position = m_nmea_has_position || m_nmea_data.pos_valid;

			//APP_ERROR_CHECK(lns_wrap_update_data(data));

			if(data->datetime_valid) {
				wall_clock_set_from_gnss(&data->datetime);
			}

			if(m_tracker_active) {
				aprs_args_t aprs_args;
				aprs_args.vbat_millivolt = m_bat_millivolt;

				aprs_args.transmit_env_data = m_bme280_updated;
				if(m_bme280_updated) {
					aprs_args.temperature_celsius = bme280_get_temperature();
					aprs_args.humidity_rH         = bme280_get_humidity();
					aprs_args.pressure_hPa        = bme280_get_pressure();
				}

				tracker_run(data, &aprs_args);
			}
			break;
	}
}


static void cb_lora(lora_evt_t evt, const lora_evt_data_t *data)
{
	ret_code_t err_code;

	bool     decode_ok;
	uint64_t rx_timestamp;
	bool     rx_time_valid;

	aprs_frame_t decoded_frame;
	aprs_rx_raw_data_t raw;

	bool switch_to_rxd = (m_display_state != DISP_STATE_LORA_PACKET_DETAIL);

	switch(evt)
	{
		case LORA_EVT_PACKET_RECEIVED:
			// try to parse the packet.
			rx_timestamp = wall_clock_get_unix();
			rx_time_valid = wall_clock_is_valid();

			decode_ok = aprs_parse_frame(
			                       data->rx_packet_data.data,
			                       data->rx_packet_data.data_len,
			                       &decoded_frame);

			if(decode_ok) {
				raw.rssi       = data->rx_packet_data.rssi;
				raw.signalRssi = data->rx_packet_data.signalRssi;
				raw.snr        = data->rx_packet_data.snr;

				memcpy(raw.data, data->rx_packet_data.data, data->rx_packet_data.data_len);
				raw.data_len = data->rx_packet_data.data_len;

				uint8_t idx = aprs_rx_history_insert(
						&decoded_frame,
						&raw,
						rx_timestamp,
						rx_time_valid,
						m_display_rx_index);

				if(switch_to_rxd) {
					m_display_rx_index = idx;
				}
			} else {
				m_last_undecodable_data.rssi       = data->rx_packet_data.rssi;
				m_last_undecodable_data.signalRssi = data->rx_packet_data.signalRssi;
				m_last_undecodable_data.snr        = data->rx_packet_data.snr;

				memcpy(m_last_undecodable_data.data, data->rx_packet_data.data, data->rx_packet_data.data_len);
				m_last_undecodable_data.data_len = data->rx_packet_data.data_len;

				m_last_undecodable_timestamp = rx_timestamp;

				if(switch_to_rxd) {
					m_display_rx_index = APRS_RX_HISTORY_SIZE;
				}
			}

			if(switch_to_rxd) {
				m_display_state = DISP_STATE_LORA_RX_OVERVIEW;
			}

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

			m_lora_rx_busy = false;
			m_epaper_update_requested = true;
			break;

		case LORA_EVT_CONFIGURED_IDLE:
			if(m_shutdown_flags & SHUTDOWN_FLAG_INITIATED) {
				lora_power_off();
			} else if(m_lora_rx_active) {
				lora_start_rx();
			} else {
				lora_power_off();
			}
			break;

		case LORA_EVT_RX_STARTED:
			m_lora_rx_busy = true;
			m_epaper_update_requested = true;
			break;

		case LORA_EVT_TX_STARTED:
			m_lora_tx_busy = true;
			m_epaper_update_requested = true;
			break;

		case LORA_EVT_TX_COMPLETE:
			m_lora_tx_busy = false;
			m_epaper_update_requested = true;
			break;

		case LORA_EVT_OFF:
			m_lora_rx_busy = false;
			m_lora_tx_busy = false;
			m_epaper_update_requested = true;
			m_shutdown_flags |= SHUTDOWN_FLAG_LORA_OFF;
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
			m_bme280_updated = false;
			m_epaper_update_requested = true;
			break;
	}
}


/**@brief BME280 event handler.
 */
static void cb_bme280(bme280_evt_t evt)
{
	switch(evt) {
		case BME280_EVT_INIT_DONE:
			NRF_LOG_INFO("BME280 detected.");
			break;

		case BME280_EVT_INIT_NOT_PRESENT:
			NRF_LOG_WARNING("BME280 is not present.");
			break;

		case BME280_EVT_COMMUNICATION_ERROR:
			NRF_LOG_ERROR("BME280 communication error.");
			break;

		case BME280_EVT_READOUT_COMPLETE:
			m_bme280_updated = true;
			NRF_LOG_INFO("BME280 readout complete.");
			break;
	}
}


/**@brief Buttons callback.
 */
static void cb_buttons(uint8_t btn_id, uint8_t evt)
{
	// enable backlight on any button event
	APP_ERROR_CHECK(app_timer_stop(m_backlight_timer));
	led_on(LED_EPAPER_BACKLIGHT);
	APP_ERROR_CHECK(app_timer_start(m_backlight_timer, APP_TIMER_TICKS(3000), NULL));

	// ensure the BME280 readout is up to date if the user is interacting with the device
	readout_bme280_if_already_powered();

	switch(btn_id)
	{
		case BUTTONS_BTN_TOUCH:
			if(evt == APP_BUTTON_PUSH) {
				// The transmitter interferes with the touch button, so we
				// ignore "critical" inputs while a packet is transmitted.
				if(!m_lora_tx_busy) {
					if(menusystem_is_active()) {
						menusystem_input(MENUSYSTEM_INPUT_NEXT);
					} else if(m_display_state == DISP_STATE_LORA_RX_OVERVIEW) {
						const aprs_rx_history_t *history = aprs_get_rx_history();

						// skip unset entries
						do {
							m_display_rx_index++;
						} while(m_display_rx_index < APRS_RX_HISTORY_SIZE
								&& history->history[m_display_rx_index].rx_timestamp == 0);

						m_display_rx_index %= (APRS_RX_HISTORY_SIZE + 1);
					}

					// always refresh the display when touch button is pressed
					// (only uses minimal additional power because the
					// backlight is on anyways and the display therefore
					// already powered).
					m_epaper_update_requested = true;
				}
			}
			break;

		case BUTTONS_BTN_1:
			if(evt == APP_BUTTON_PUSH) {
				if(menusystem_is_active()) {
					menusystem_input(MENUSYSTEM_INPUT_CONFIRM);
				} else if(m_display_state == DISP_STATE_PASSKEY) {
					m_display_state = m_prev_display_state;
					m_epaper_update_requested = true;
				} else if(buttons_button_is_pressed(BUTTONS_BTN_TOUCH)) {
					// cycle through various module enable states:
					// all off -> RX on -> RX+TX on -> TX on -> all off.
					// Transitions are processed by the menusystem callback function to
					// avoid code duplication.
					if(!m_lora_rx_active && !m_tracker_active) {
						cb_menusystem(MENUSYSTEM_EVT_RX_ENABLE, NULL);
					} else if(m_lora_rx_active && !m_tracker_active) {
						cb_menusystem(MENUSYSTEM_EVT_TRACKER_ENABLE, NULL);
					} else if(m_lora_rx_active && m_tracker_active) {
						cb_menusystem(MENUSYSTEM_EVT_RX_DISABLE, NULL);
					} else {
						cb_menusystem(MENUSYSTEM_EVT_TRACKER_DISABLE, NULL);
					}
				} else {
					// cycle through the displays
					if(m_display_state == DISP_CYCLE_LAST) {
						m_display_state = DISP_CYCLE_FIRST;
					} else {
						m_display_state++;
					}

					m_epaper_update_requested = true;
				}
			} else if(evt == BUTTONS_EVT_LONGPRESS) {
				if(!menusystem_is_active()) {
					menusystem_enter();
					m_epaper_update_requested = true;
				}
			}
			break;
	}
}


/**@brief Settings callback.
 */
static void cb_settings(settings_evt_t evt, settings_id_t id)
{
	uint8_t buffer[64];
	size_t  len;

	ret_code_t err_code;

	switch(evt) {
		case SETTINGS_EVT_INIT:
			NRF_LOG_INFO("Settings initialized. Loading...");

			// read all settings and forward them to the relevant modules
			len = sizeof(buffer);
			err_code = settings_query(SETTINGS_ID_SOURCE_CALL, buffer, &len);
			if(err_code == NRF_SUCCESS) {
				NRF_LOG_INFO("Source call loaded: %s", NRF_LOG_PUSH((char*)buffer));
				aprs_service_set_mycall(&m_aprs_service, (const char *)buffer);
				aprs_set_source((const char *)buffer);
			} else {
				NRF_LOG_WARNING("Error while loading source call: 0x%08x", err_code);
				aprs_set_source(APRS_SOURCE);
			}

			len = sizeof(buffer);
			err_code = settings_query(SETTINGS_ID_SYMBOL_CODE, buffer, &len);
			if(err_code == NRF_SUCCESS) {
				NRF_LOG_INFO("Symbol code loaded: %c%c", buffer[0], buffer[1]);
				aprs_service_set_symbol(&m_aprs_service, buffer[0], buffer[1]);
				aprs_set_icon(buffer[0], buffer[1]);
			} else {
				NRF_LOG_WARNING("Error while loading symbol code: 0x%08x", err_code);
				aprs_set_icon(APRS_SYMBOL_TABLE, APRS_SYMBOL_ICON);
			}

			len = sizeof(buffer);
			err_code = settings_query(SETTINGS_ID_COMMENT, buffer, &len);
			if(err_code == NRF_SUCCESS) {
				NRF_LOG_INFO("Comment loaded: %s", NRF_LOG_PUSH((char*)buffer));
				aprs_service_set_comment(&m_aprs_service, (const char *)buffer);
				aprs_set_comment((const char *)buffer);
			} else {
				NRF_LOG_WARNING("Error while loading comment: 0x%08x", err_code);
				aprs_set_comment(APRS_COMMENT);
			}

			len = sizeof(buffer);
			err_code = settings_query(SETTINGS_ID_LORA_POWER, buffer, &len);
			if(err_code == NRF_SUCCESS) {
				NRF_LOG_INFO("LoRa power loaded: %i", buffer[0]);
				lora_set_power(buffer[0]);
			} else {
				NRF_LOG_WARNING("Error while loading LoRa power: 0x%08x", err_code);
				// use default power set in lora_init().
			}

			len = sizeof(buffer);
			err_code = settings_query(SETTINGS_ID_APRS_FLAGS, buffer, &len);
			if(err_code == NRF_SUCCESS) {
				uint32_t flags = *(uint32_t*)buffer;
				NRF_LOG_INFO("APRS flags loaded: 0x%08x", flags);
				aprs_set_config_flags(flags);
			} else {
				NRF_LOG_WARNING("Error while loading APRS flags: 0x%08x", err_code);
				// use default flags set in aprs_init().
			}
			break;

		case SETTINGS_EVT_UPDATE_COMPLETE:
			// whenever the last BLE symbol code was stored, also update the
			// current APRS symbol
			if(id == SETTINGS_ID_LAST_BLE_SYMBOL) {
				aprs_get_icon((char*)&buffer[0], (char*)&buffer[1]);
				settings_write(SETTINGS_ID_SYMBOL_CODE, buffer, 2);
			}
			break;
	}
}


/**@brief Menusystem callback.
 */
static void cb_menusystem(menusystem_evt_t evt, const menusystem_evt_data_t *data)
{
	bool gps_active_pre = m_gnss_keep_active || m_tracker_active;

	bool gnss_cold_reboot_request = false;

	switch(evt) {
		case MENUSYSTEM_EVT_EXIT_MENU:
			break;

		case MENUSYSTEM_EVT_RX_ENABLE:
			m_lora_rx_active = true;
			lora_start_rx();
			break;

		case MENUSYSTEM_EVT_RX_DISABLE:
			m_lora_rx_active = false;
			lora_power_off();
			break;

		case MENUSYSTEM_EVT_TRACKER_ENABLE:
			if(aprs_can_build_frame()) {
				m_tracker_active = true;
				tracker_reset_tx_counter();
				tracker_force_tx();
			}
			break;

		case MENUSYSTEM_EVT_TRACKER_DISABLE:
			m_tracker_active = false;
			break;

		case MENUSYSTEM_EVT_GNSS_WARMUP_ENABLE:
			m_gnss_keep_active = true;
			break;

		case MENUSYSTEM_EVT_GNSS_WARMUP_DISABLE:
			m_gnss_keep_active = false;
			break;

		case MENUSYSTEM_EVT_GNSS_COLD_REBOOT:
			m_gnss_keep_active = true; // power is required for cold restart
			gnss_cold_reboot_request = true;
			break;

		case MENUSYSTEM_EVT_APRS_SYMBOL_CHANGED:
			{
				char table = data->aprs_symbol.table;
				char symbol = data->aprs_symbol.symbol;

				APP_ERROR_CHECK(aprs_service_set_symbol(&m_aprs_service, table, symbol));

				uint8_t buf[2] = {table, symbol};
				settings_write(SETTINGS_ID_SYMBOL_CODE, buf, sizeof(buf));

				aprs_set_icon(table, symbol);
			}
			break;

		case MENUSYSTEM_EVT_LORA_POWER_CHANGED:
			if(data->lora_power.power != lora_get_power()) {
				APP_ERROR_CHECK(lora_set_power(data->lora_power.power));

				uint8_t buf = data->lora_power.power;
				settings_write(SETTINGS_ID_LORA_POWER, &buf, sizeof(buf));
			}
			break;

		case MENUSYSTEM_EVT_APRS_FLAGS_CHANGED:
			settings_write(SETTINGS_ID_APRS_FLAGS, (uint8_t*)&data->aprs_flags.flags, sizeof(data->aprs_flags.flags));
			break;

		case MENUSYSTEM_EVT_SHUTDOWN:
			// initiate the shutdown
			m_shutdown_flags = SHUTDOWN_FLAG_INITIATED;

			// clear the display
			m_display_state = DISP_STATE_CLEAR;
			m_epaper_update_requested = true;
			m_epaper_force_full_refresh = true;

			// put LoRa into low power mode
			m_lora_rx_active = false;
			m_tracker_active = false;
			if(lora_is_off()) {
				m_shutdown_flags |= SHUTDOWN_FLAG_LORA_OFF;
			} else {
				lora_power_off();
			}
			break;

		case MENUSYSTEM_EVT_REDRAW_REQUIRED:
			// redraw is requested for all menu events, see below
			break;
	}

	bool gps_active_now = m_gnss_keep_active || m_tracker_active;

	if(gps_active_now && !gps_active_pre) {
		APP_ERROR_CHECK(gps_power_on());

		// as the GPS is a major power drain, we increase the voltage monitor's
		// update rate while it is on.
		voltage_monitor_stop();
		voltage_monitor_start(VOLTAGE_MONITOR_INTERVAL_ACTIVE);
	} else if(!gps_active_now && gps_active_pre) {
		APP_ERROR_CHECK(gps_power_off());

		// GPS is off -> go to lower rate again
		voltage_monitor_stop();
		voltage_monitor_start(VOLTAGE_MONITOR_INTERVAL_IDLE);
	}

	if(gnss_cold_reboot_request) {
		APP_ERROR_CHECK(gps_cold_restart());
	}

	m_epaper_update_requested = true;
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


/**@brief Function for application main entry.
*/
int main(void)
{

	// Initialize.
	log_init();
	gpio_init();
	timers_init();
	power_management_init();

	// settings set some values in this module, so we must initialize it first.
	aprs_init();

	// load the settings (must be done before peer_manager_init()!)
	settings_init(cb_settings);

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
	wall_clock_init();
	epaper_init();
	gps_init(cb_gps);
	gps_reset();
	lora_init(cb_lora);
	tracker_init(cb_tracker);
	APP_ERROR_CHECK(bme280_init(cb_bme280));

	voltage_monitor_init(cb_voltage_monitor);

	menusystem_init(cb_menusystem);

	// Start execution.
	NRF_LOG_INFO("LoRa-APRS started.");
	application_timers_start();

	voltage_monitor_start(VOLTAGE_MONITOR_INTERVAL_IDLE);

	// Initial APRS setup (values not read from flash only)
	aprs_set_dest(APRS_DESTINATION);

	aprs_clear_path();
	aprs_add_path("WIDE1-1");

	m_display_state = DISP_STATE_STARTUP;
	redraw_display(true);

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

			if((m_shutdown_flags & SHUTDOWN_FLAG_INITIATED) == 0
					|| (m_shutdown_flags & SHUTDOWN_FLAG_DISPLAY_LOCKED) == 0) {
				// lock display redraws after shutdown was initiated
				m_shutdown_flags |= SHUTDOWN_FLAG_DISPLAY_LOCKED;

				redraw_display(m_epaper_force_full_refresh);
				m_epaper_force_full_refresh = false;
			}
		} else if(((m_shutdown_flags & SHUTDOWN_FLAG_DISPLAY_LOCKED) != 0) && !epaper_is_busy()) {
			m_shutdown_flags |= SHUTDOWN_FLAG_DISPLAY_CLEARED;
		}

		// handle shutdown
		if(m_shutdown_flags == SHUTDOWN_FLAG_ALL_SET) {
			sleep_mode_enter();
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
