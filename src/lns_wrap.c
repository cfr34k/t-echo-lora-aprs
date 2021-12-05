#include <nrf_log.h>

#include <ble_lns.h>

#include "lns_wrap.h"

BLE_LNS_DEF(m_ble_lns);

NRF_BLE_GQ_DEF(m_ble_gatt_queue,
               NRF_SDH_BLE_PERIPHERAL_LINK_COUNT,
               4); // number of entries

// data for the Location and Navigation service
static ble_lns_loc_speed_t   m_location_speed;
static ble_lns_pos_quality_t m_position_quality;


/**@brief Location Navigation event handler.
 *
 * @details This function will be called for all events of the Location Navigation Module that
 *          are passed to the application.
 *
 * @param[in]   p_evt   Event received from the Location Navigation Module.
 */
static void cb_lns(ble_lns_t const * p_lns, ble_lns_evt_t const * p_evt)
{
	switch (p_evt->evt_type)
	{
		case BLE_LNS_CTRLPT_EVT_INDICATION_ENABLED:
			NRF_LOG_INFO("Control Point: Indication enabled");
			break;

		case BLE_LNS_CTRLPT_EVT_INDICATION_DISABLED:
			NRF_LOG_INFO("Control Point: Indication disabled");
			break;

		case BLE_LNS_LOC_SPEED_EVT_NOTIFICATION_ENABLED:
			NRF_LOG_INFO("Location/Speed: Notification enabled");
			break;

		case BLE_LNS_LOC_SPEED_EVT_NOTIFICATION_DISABLED:
			NRF_LOG_INFO("Location/Speed: Notification disabled");
			break;

		case BLE_LNS_NAVIGATION_EVT_NOTIFICATION_ENABLED:
			NRF_LOG_INFO("Navigation: Notification enabled");
			break;

		case BLE_LNS_NAVIGATION_EVT_NOTIFICATION_DISABLED:
			NRF_LOG_INFO("Navigation: Notification disabled");
			break;

		default:
			break;
	}
}


/**@brief Callback function for errors in the Location Navigation Service.
 *
 * @details This function will be called in case of an error in the Location Navigation Service.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 */
static void cb_lns_error(uint32_t err_code)
{
	app_error_handler(0xDEADBEEF, 0, 0);
}


ret_code_t lns_wrap_init(void)
{
	ble_lns_init_t lns_init;

	memset(&lns_init, 0, sizeof(lns_init));

	lns_init.evt_handler      = cb_lns;
	//lns_init.lncp_evt_handler = on_ln_ctrlpt_evt;
	lns_init.error_handler    = cb_lns_error;
	lns_init.p_gatt_queue     = &m_ble_gatt_queue;

	lns_init.available_features =
		//BLE_LNS_FEATURE_INSTANT_SPEED_SUPPORTED                 |
		//BLE_LNS_FEATURE_TOTAL_DISTANCE_SUPPORTED                |
		BLE_LNS_FEATURE_LOCATION_SUPPORTED                      |
		//BLE_LNS_FEATURE_ELEVATION_SUPPORTED                     |
		//BLE_LNS_FEATURE_HEADING_SUPPORTED                       |
		//BLE_LNS_FEATURE_ROLLING_TIME_SUPPORTED                  |
		//BLE_LNS_FEATURE_UTC_TIME_SUPPORTED                      |
		//BLE_LNS_FEATURE_REMAINING_DISTANCE_SUPPORTED            |
		//BLE_LNS_FEATURE_REMAINING_VERT_DISTANCE_SUPPORTED       |
		//BLE_LNS_FEATURE_EST_TIME_OF_ARRIVAL_SUPPORTED           |
		BLE_LNS_FEATURE_NUM_SATS_IN_SOLUTION_SUPPORTED          |
		//BLE_LNS_FEATURE_NUM_SATS_IN_VIEW_SUPPORTED              |
		//BLE_LNS_FEATURE_TIME_TO_FIRST_FIX_SUPPORTED             |
		//BLE_LNS_FEATURE_EST_HORZ_POS_ERROR_SUPPORTED            |
		//BLE_LNS_FEATURE_EST_VERT_POS_ERROR_SUPPORTED            |
		BLE_LNS_FEATURE_HORZ_DILUTION_OF_PRECISION_SUPPORTED    |
		BLE_LNS_FEATURE_VERT_DILUTION_OF_PRECISION_SUPPORTED    |
		//BLE_LNS_FEATURE_LOC_AND_SPEED_CONTENT_MASKING_SUPPORTED |
		//BLE_LNS_FEATURE_FIX_RATE_SETTING_SUPPORTED              |
		//BLE_LNS_FEATURE_ELEVATION_SETTING_SUPPORTED             |
		//BLE_LNS_FEATURE_POSITION_STATUS_SUPPORTED;
		0;

	lns_init.is_position_quality_present = true;
	lns_init.is_control_point_present    = false;
	lns_init.is_navigation_present       = false;

	// initialize the data (all invalid, no lock yet)
	memset(&m_location_speed, 0, sizeof(m_location_speed));
	memset(&m_position_quality, 0, sizeof(m_position_quality));

	m_location_speed.position_status  = BLE_LNS_NO_POSITION;
	m_location_speed.elevation_source = BLE_LNS_ELEV_SOURCE_POSITIONING_SYSTEM;

	m_position_quality.position_status = BLE_LNS_NO_POSITION;

	lns_init.p_location_speed   = &m_location_speed;
	lns_init.p_position_quality = &m_position_quality;

	lns_init.loc_nav_feature_security_req_read_perm  = SEC_OPEN;
	lns_init.loc_speed_security_req_cccd_write_perm  = SEC_OPEN;
	lns_init.position_quality_security_req_read_perm = SEC_OPEN;
	lns_init.navigation_security_req_cccd_write_perm = SEC_OPEN;
	lns_init.ctrl_point_security_req_write_perm      = SEC_OPEN;
	lns_init.ctrl_point_security_req_cccd_write_perm = SEC_OPEN;

	return ble_lns_init(&m_ble_lns, &lns_init);
}

ret_code_t lns_wrap_update_data(const nmea_data_t *data)
{
	if(data->pos_valid) {
		m_location_speed.position_status = BLE_LNS_POSITION_OK;

		m_location_speed.location_present = true;
		m_location_speed.latitude  = data->lat * 1e7;
		m_location_speed.longitude = data->lon * 1e7;

		m_position_quality.position_status = BLE_LNS_POSITION_OK;

		m_position_quality.hdop_present = true;
		m_position_quality.hdop         = 5 * data->hdop;
		m_position_quality.vdop_present = true;
		m_position_quality.vdop         = 5 * data->vdop;
	} else {
		m_location_speed.position_status = BLE_LNS_LAST_KNOWN_POSITION;
		m_position_quality.position_status = BLE_LNS_LAST_KNOWN_POSITION;
	}

	m_position_quality.number_of_satellites_in_solution_present = true;
	m_position_quality.number_of_satellites_in_solution = 0;
	for(int i = 0; i < NMEA_NUM_FIX_INFO; i++) {
		m_position_quality.number_of_satellites_in_solution
			+= data->fix_info[i].sats_used;
	}

	return ble_lns_loc_speed_send(&m_ble_lns);
}
