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

#ifndef SETTINGS_H
#define SETTINGS_H

/**@file
 *
 * @brief Settings subsystem.
 *
 * @details
 * This module implements a settings storage based on Nordic’s flash data
 * storage (FDS) library.
 */

#include <stdint.h>
#include <stddef.h>
#include <sdk_errors.h>

/**@brief Settings entry IDs that can be queried or set.
 */
typedef enum
{
	SETTINGS_ID_INVALID     = 0x0000, // do not use

	SETTINGS_ID_SOURCE_CALL = 0x0001,
	SETTINGS_ID_SYMBOL_CODE = 0x0002,
	SETTINGS_ID_COMMENT     = 0x0003,
} settings_id_t;

/**@brief Events sent via the callback function.
 */
typedef enum
{
	SETTINGS_EVT_INIT,
	SETTINGS_EVT_UPDATE_COMPLETE
} settings_evt_t;


typedef void (*settings_callback)(settings_evt_t evt, settings_id_t id);

/**@brief Initialize the Settings subsystem.
 *
 * @returns   The result code of the FDS subsystem initialization.
 */
ret_code_t settings_init(settings_callback callback);

/**@brief Query a setting.
 *
 * @note
 * The result length will always be a multiple of 4 because FDS operates only
 * in that granularity.
 *
 * @param[in] id           ID of the setting to read.
 * @param[in] data         Pointer to memory where the read data is written.
 * @param[inout] data_len  Upon call, the value shall be set to the buffer
 *                         size. Will be updated with the number of bytes
 *                         written to the buffer.
 * @retval NRF_ERROR_NO_MEM   If the provided data_len was too small for the
 *                            record. data_len will be set to the required
 *                            length if this happens.
 * @retval err_code        The result from the FDS operations.
 */
ret_code_t settings_query(settings_id_t id, uint8_t *data, size_t *data_len);

/**@brief Write or delete a setting.
 * @details
 * This is an asynchronous operation.
 *
 * Set the data_len to 0 to delete an existing record. Otherwise, the record is
 * update if it exists or created if not.
 *
 * @note
 * The data will be padded such that the length is a multiple of 4 because FDS
 * operates only in that granularity. Make sure you can find out where the data
 * actually ends when you read it back.
 *
 * @param[in] id        ID of the setting to update.
 * @param[in] data      Pointer to the data to write.
 * @param[in] data_len  Length of the data to write (0 = delete record).
 * @returns             The result from fds_record_write().
 */
ret_code_t settings_write(settings_id_t id, const uint8_t *data, size_t data_len);

#endif // SETTINGS_H

