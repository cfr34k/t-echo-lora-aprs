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

#include "ble_gap.h"
#include "ble_gatt.h"
#include "ble_gatts.h"
#include "ble_types.h"
#include "nrf_error.h"
#include "nrf_saadc.h"
#include "sdk_common.h"
#include "nrf_log.h"

#include "aprs_service.h"

#include "ble_srv_common.h"
#include "ble_conn_state.h"

#include "config.h"

/**@brief Function for handling the Write event.
 *
 * @param[in] p_srv      Service structure.
 * @param[in] p_ble_evt  Event received from the BLE stack.
 */
static void on_write(aprs_service_t * p_srv, ble_evt_t const * p_ble_evt)
{
	ble_gatts_evt_write_t const * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

	if (p_evt_write->handle == p_srv->mycall_char_handles.value_handle)
	{
		p_srv->callback(APRS_SERVICE_EVT_MYCALL_CHANGED);
	}
	else if (p_evt_write->handle == p_srv->comment_char_handles.value_handle)
	{
		p_srv->callback(APRS_SERVICE_EVT_COMMENT_CHANGED);
	}
	else if (p_evt_write->handle == p_srv->symbol_char_handles.value_handle)
	{
		p_srv->callback(APRS_SERVICE_EVT_SYMBOL_CHANGED);
	}
}

/**@brief Handle BLE events.
 * @details
 * The actual event handling is distributed over event-specific functions.
 */
void aprs_service_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
{
	aprs_service_t * p_srv = (aprs_service_t *)p_context;

	switch (p_ble_evt->header.evt_id)
	{
		case BLE_GATTS_EVT_WRITE:
			on_write(p_srv, p_ble_evt);
			break;

		default:
			// No implementation needed.
			break;
	}
}


static void fill_user_desc(ble_add_char_user_desc_t *user_desc, const char *str)
{
	memset(user_desc, 0, sizeof(ble_add_char_user_desc_t));
	user_desc->is_var_len       = 0;
	user_desc->char_props.read  = 1;
	user_desc->size             = strlen((char*)str);
	user_desc->max_size         = user_desc->size;
	user_desc->p_char_user_desc = (uint8_t*)str;
	user_desc->is_value_user    = 0;
	user_desc->read_access      = SEC_OPEN;
	user_desc->write_access     = SEC_NO_ACCESS;
}


uint32_t aprs_service_init(aprs_service_t * p_srv, const aprs_service_init_t * p_srv_init)
{
	uint32_t                 err_code;
	ble_uuid_t               ble_uuid;
	ble_add_char_params_t    add_char_params;
	ble_add_char_user_desc_t add_user_desc;
	uint8_t                  init_val[16];
	size_t                   len;

	// Initialize service structure.
	p_srv->callback  = p_srv_init->callback;

	// Add service.
	ble_uuid128_t base_uuid = {APRS_SERVICE_UUID_BASE};
	err_code = sd_ble_uuid_vs_add(&base_uuid, &p_srv->uuid_type);
	VERIFY_SUCCESS(err_code);

	ble_uuid.type = p_srv->uuid_type;
	ble_uuid.uuid = APRS_SERVICE_UUID_SERVICE;

	err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_srv->service_handle);
	VERIFY_SUCCESS(err_code);

	/* Add my call characteristic. */
	strcpy((char*)init_val, APRS_SOURCE);
	len = strlen((char*)init_val);

	memset(&add_char_params, 0, sizeof(add_char_params));
	add_char_params.uuid              = APRS_SERVICE_UUID_MYCALL;
	add_char_params.uuid_type         = p_srv->uuid_type;
	add_char_params.init_len          = len;
	add_char_params.max_len           = 16;
	add_char_params.p_init_value      = init_val;
	add_char_params.is_var_len        = 1;
	add_char_params.char_props.read   = 1;
	add_char_params.char_props.write  = 1;

	add_char_params.read_access       = SEC_OPEN;
	add_char_params.write_access      = SEC_MITM;

	fill_user_desc(&add_user_desc, "My Call");
	add_char_params.p_user_descr = &add_user_desc;

	err_code = characteristic_add(p_srv->service_handle, &add_char_params, &p_srv->mycall_char_handles);
	VERIFY_SUCCESS(err_code);

	/* Add comment characteristic. */
	memset(&add_char_params, 0, sizeof(add_char_params));
	add_char_params.uuid              = APRS_SERVICE_UUID_COMMENT;
	add_char_params.uuid_type         = p_srv->uuid_type;
	add_char_params.init_len          = 0;
	add_char_params.max_len           = 43;
	add_char_params.is_var_len        = 1;
	add_char_params.char_props.read   = 1;
	add_char_params.char_props.write  = 1;

	add_char_params.read_access       = SEC_OPEN;
	add_char_params.write_access      = SEC_MITM;

	fill_user_desc(&add_user_desc, "Comment");
	add_char_params.p_user_descr = &add_user_desc;

	err_code = characteristic_add(p_srv->service_handle, &add_char_params, &p_srv->comment_char_handles);
	VERIFY_SUCCESS(err_code);

	/* Add symbol code characteristic. */
	init_val[0] = APRS_SYMBOL_TABLE;
	init_val[1] = APRS_SYMBOL_ICON;

	memset(&add_char_params, 0, sizeof(add_char_params));
	add_char_params.uuid              = APRS_SERVICE_UUID_SYMBOL;
	add_char_params.uuid_type         = p_srv->uuid_type;
	add_char_params.init_len          = 2;
	add_char_params.max_len           = 2;
	add_char_params.is_var_len        = 0;
	add_char_params.p_init_value      = init_val;
	add_char_params.char_props.read   = 1;
	add_char_params.char_props.write  = 1;

	add_char_params.read_access       = SEC_OPEN;
	add_char_params.write_access      = SEC_MITM;

	fill_user_desc(&add_user_desc, "Symbol code");
	add_char_params.p_user_descr = &add_user_desc;

	err_code = characteristic_add(p_srv->service_handle, &add_char_params, &p_srv->symbol_char_handles);
	VERIFY_SUCCESS(err_code);

	/* Add rx message characteristic. */
	memset(&add_char_params, 0, sizeof(add_char_params));
	add_char_params.uuid              = APRS_SERVICE_UUID_RX_MESSAGE;
	add_char_params.uuid_type         = p_srv->uuid_type;
	add_char_params.init_len          = 0;
	add_char_params.max_len           = 256;
	add_char_params.is_var_len        = 1;
	add_char_params.char_props.read   = 1;
	add_char_params.char_props.notify = 1;

	add_char_params.read_access       = SEC_OPEN;
	add_char_params.cccd_write_access = SEC_OPEN;

	fill_user_desc(&add_user_desc, "RX Message");
	add_char_params.p_user_descr = &add_user_desc;

	err_code = characteristic_add(p_srv->service_handle, &add_char_params, &p_srv->rx_message_char_handles);
	VERIFY_SUCCESS(err_code);

	return err_code;
}


ret_code_t aprs_service_set_mycall(aprs_service_t * p_srv, const char *p_mycall)
{
	ble_gatts_value_t value = {strlen(p_mycall), 0, (uint8_t*)p_mycall};

	return sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID, p_srv->mycall_char_handles.value_handle, &value);
}


ret_code_t aprs_service_get_mycall(aprs_service_t * p_srv, char *p_mycall, uint8_t mycall_len)
{
	ble_gatts_value_t value = {mycall_len-1, 0, (uint8_t*)p_mycall};

	ret_code_t err_code = sd_ble_gatts_value_get(BLE_CONN_HANDLE_INVALID, p_srv->mycall_char_handles.value_handle, &value);

	if(err_code == NRF_SUCCESS) {
		p_mycall[value.len] = '\0';
	}

	return err_code;
}


ret_code_t aprs_service_set_comment(aprs_service_t * p_srv, const char *p_comment)
{
	ble_gatts_value_t value = {strlen(p_comment), 0, (uint8_t*)p_comment};

	return sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID, p_srv->comment_char_handles.value_handle, &value);
}


ret_code_t aprs_service_get_comment(aprs_service_t * p_srv, char *p_comment, uint8_t comment_len)
{
	ble_gatts_value_t value = {comment_len-1, 0, (uint8_t*)p_comment};

	ret_code_t err_code = sd_ble_gatts_value_get(BLE_CONN_HANDLE_INVALID, p_srv->comment_char_handles.value_handle, &value);

	if(err_code == NRF_SUCCESS) {
		p_comment[value.len] = '\0';
	}

	return err_code;
}


ret_code_t aprs_service_set_symbol(aprs_service_t * p_srv, char table, char symbol)
{
	uint8_t buf[2] = {table, symbol};
	ble_gatts_value_t value = {sizeof(buf), 0, buf};

	return sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID, p_srv->symbol_char_handles.value_handle, &value);
}


ret_code_t aprs_service_get_symbol(aprs_service_t * p_srv, char *p_table, char *p_symbol)
{
	uint8_t buf[2];
	ble_gatts_value_t value = {sizeof(buf), 0, buf};

	ret_code_t err_code = sd_ble_gatts_value_get(BLE_CONN_HANDLE_INVALID, p_srv->symbol_char_handles.value_handle, &value);

	if(err_code == NRF_SUCCESS) {
		*p_table = buf[0];
		*p_symbol = buf[1];
	}

	return err_code;
}


ret_code_t aprs_service_notify_rx_message(aprs_service_t * p_srv, uint16_t conn_handle, uint8_t *p_message, uint8_t message_len)
{

	if(ble_conn_state_status(conn_handle) != BLE_CONN_STATUS_CONNECTED) {
		// not connected, so we can't send a notification. Simply set the value.
		ble_gatts_value_t value = {message_len, 0, p_message};

		return sd_ble_gatts_value_set(
				BLE_CONN_HANDLE_INVALID,
				p_srv->rx_message_char_handles.value_handle,
				&value);
	} else {
		uint16_t len = message_len;
		ble_gatts_hvx_params_t params;

		memset(&params, 0, sizeof(params));
		params.type   = BLE_GATT_HVX_NOTIFICATION;
		params.handle = p_srv->rx_message_char_handles.value_handle;
		params.p_data = p_message;
		params.p_len  = &len;

		return sd_ble_gatts_hvx(conn_handle, &params);
	}
}
