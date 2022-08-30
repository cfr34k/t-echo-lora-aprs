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

#ifndef WALL_CLOCK_H
#define WALL_CLOCK_H

/**@file
 *
 * @brief Wall Clock subsystem.
 *
 * @details
 * This module tracks the wall clock time. The current time value can be set by
 * the user.
 *
 * Important: do not expect the time returned by this module to be monotonic.
 * It can jump at any time (whenever @ref wall_clock_set() is called)!
 *
 * Internally, this module uses the @ref time_base module to track time deltas
 * between function calls. It therefore can only be as precise in the long term
 * as the time_base reference if no external updates occur.
 */

#include <stdint.h>
#include <time.h>

#include "nmea.h"

/**@brief Initialize the Time Base subsystem.
 */
void wall_clock_init(void);

/**@brief Returns the current UNIX time in seconds.
 * @details
 * UNIX time represents the seconds since 1970-01-01 00:00:00 UTC. If the clock
 * has never been set, time starts counting at 0.
 *
 * @returns   The UNIX time in seconds.
 */
uint64_t wall_clock_get_unix(void);

/**@brief Returns the current UTC.
 * @details
 * If the clock has never been set, it starts counting at 1970-01-01 00:00:00 UTC.
 *
 * @param[out] time   Pointer to the time structure to fill with the current time.
 */
void wall_clock_get_utc(struct tm *time);

/**@brief Set the current time from an NMEA datetime structure.
 * @details
 * The internal time will be set immediately and continue running from there.
 *
 * @param[in] datetime    Pointer to the datetime structure with the new time.
 */
void wall_clock_set_from_gnss(const nmea_datetime_t *datetime);

#endif // WALL_CLOCK_H
