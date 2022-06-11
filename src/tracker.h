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

#ifndef TRACKER_H
#define TRACKER_H

#include <sdk_errors.h>

#include "nmea.h"

typedef enum {
	TRACKER_EVT_TRANSMISSION_STARTED,
} tracker_evt_t;

typedef void (*tracker_callback)(tracker_evt_t evt);

/**@brief Initialize all modules necessary for tracking.
 */
ret_code_t tracker_init(tracker_callback callback);

/**@brief Process a new position report in the tracker.
 */
ret_code_t tracker_run(const nmea_data_t *data);

/**@brief Force a transmission on the next valid GPS update.
 */
void tracker_force_tx(void);

/**@brief Get the value of the transmission counter.
 */
uint32_t tracker_get_tx_counter(void);

/**@brief Reset the transmission counter.
 */
void tracker_reset_tx_counter(void);

#endif // TRACKER_H
