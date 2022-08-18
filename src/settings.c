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

#include <string.h>

#include <fds.h>
#include <sdk_macros.h>
#include <nrf_log.h>

#include <nrfx_nvmc.h>

#include "nrf_error.h"
#include "settings.h"

#define SETTINGS_FDS_FILE_ID  0x0001

static settings_callback m_callback;
static settings_id_t     m_pending_id = SETTINGS_ID_INVALID;

static uint8_t m_write_cache[64];

static void cb_fds(fds_evt_t const * p_evt)
{
	bool op_done = false;

	switch(p_evt->id) {
		case FDS_EVT_INIT:
			NRF_LOG_INFO("settings: callback: INIT");
			m_callback(SETTINGS_EVT_INIT, SETTINGS_ID_INVALID);
			break;

		case FDS_EVT_UPDATE:
		case FDS_EVT_WRITE:
			NRF_LOG_INFO("settings: callback: WRITE/UPDATE %04x", p_evt->write.record_key);
			op_done = (p_evt->write.record_key == m_pending_id);
			break;

		case FDS_EVT_DEL_RECORD:
			NRF_LOG_INFO("settings: callback: DEL %04x", p_evt->del.record_key);
			op_done = (p_evt->del.record_key == m_pending_id);
			break;

		default:
			break;
	}

	if(op_done) {
		NRF_LOG_INFO("settings: callback: write operation on %04x complete!", m_pending_id);
		m_callback(SETTINGS_EVT_UPDATE_COMPLETE, m_pending_id);
		m_pending_id = SETTINGS_ID_INVALID;
	}
}


ret_code_t settings_init(settings_callback callback)
{
	ret_code_t err_code;

	m_callback = callback;

	err_code = fds_register(cb_fds);
	VERIFY_SUCCESS(err_code);

	err_code = fds_init();

	if(err_code != FDS_ERR_NO_PAGES) {
		return err_code;
	} else {
		// erase the flash pages for the FDS and try again
		APP_ERROR_CHECK(nrfx_nvmc_page_erase(0xF0000));
		APP_ERROR_CHECK(nrfx_nvmc_page_erase(0xF1000));
		APP_ERROR_CHECK(nrfx_nvmc_page_erase(0xF2000));
		APP_ERROR_CHECK(nrfx_nvmc_page_erase(0xF3000));

		return fds_init();
	}
}


ret_code_t settings_query(settings_id_t id, uint8_t *data, size_t *data_len)
{
	fds_flash_record_t  flash_record;
	fds_record_desc_t   record_desc;
	fds_find_token_t    token;

	ret_code_t err_code;

	if(id == SETTINGS_ID_INVALID) {
		return NRF_ERROR_INVALID_PARAM;
	}

	memset(&token, 0x00, sizeof(fds_find_token_t));

	err_code = fds_record_find(SETTINGS_FDS_FILE_ID, id, &record_desc, &token);
	VERIFY_SUCCESS(err_code);

	err_code = fds_record_open(&record_desc, &flash_record);
	VERIFY_SUCCESS(err_code);

	size_t record_size_bytes = flash_record.p_header->length_words * 4;
	NRF_LOG_INFO("settings: size of record %04x = %d bytes", id, record_size_bytes);
	if(*data_len < record_size_bytes) {
		// insufficient memory provided for this record
		NRF_LOG_INFO("settings: insufficient memory to read record %04x: %d < %d bytes", id, *data_len, record_size_bytes);
		*data_len = record_size_bytes;
		fds_record_close(&record_desc);
		return NRF_ERROR_NO_MEM;
	}

	*data_len = record_size_bytes;
	memcpy(data, flash_record.p_data, record_size_bytes);

	return fds_record_close(&record_desc);
}


ret_code_t settings_write(settings_id_t id, const uint8_t *data, size_t data_len)
{
	fds_record_desc_t   record_desc;
	fds_find_token_t    token;
	fds_record_t        record;

	bool record_exists;

	ret_code_t err_code;

	if(id == SETTINGS_ID_INVALID) {
		return NRF_ERROR_INVALID_PARAM;
	}

	if(m_pending_id != SETTINGS_ID_INVALID) {
		return NRF_ERROR_BUSY;
	}

	if(data_len > sizeof(m_write_cache)) {
		return NRF_ERROR_NO_MEM;
	}

	memset(&token, 0x00, sizeof(fds_find_token_t));

	err_code = fds_record_find(SETTINGS_FDS_FILE_ID, id, &record_desc, &token);
	record_exists = (err_code == NRF_SUCCESS);

	NRF_LOG_INFO("settings: record %04x exists? %d", id, record_exists);

	m_pending_id = id; // for completion check. Also marks the settings module as busy.

	if(record_exists && data_len == 0) {
		// delete the existing record
		NRF_LOG_INFO("settings: deleting record %04x", id);
		return fds_record_delete(&record_desc);
	}

	// copy the data into the write cache
	memcpy(m_write_cache, data, data_len);

	// fill the record structure to write
	record.file_id           = SETTINGS_FDS_FILE_ID;
	record.key               = id;
	record.data.p_data       = m_write_cache;
	/* The following calculation takes into account any eventual remainder of the division. */
	record.data.length_words = (data_len + 3) / 4;

	if(record_exists) {
		// write new record and erase the old one
		NRF_LOG_INFO("settings: updating record %04x", id);
		return fds_record_update(&record_desc, &record);
	} else {
		// just create the new record
		NRF_LOG_INFO("settings: creating record %04x", id);
		return fds_record_write(NULL, &record);
	}
}
