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

#ifndef TIME_BASE_H
#define TIME_BASE_H

/**@file
 *
 * @brief Time Base subsystem.
 *
 * @details
 * This module tracks the current time in milliseconds. The current time value
 * can be set by the user.
 *
 * Internally, this is based on the app_timer framework which in turn uses an
 * RTC to track the time. The accuracy therefore depends on the RTC's clock
 * source.
 */

#include <stdint.h>
#include <sdk_errors.h>

/**@brief Initialize the Time Base subsystem.
 *
 * @returns   The result code of the app_timer initialization.
 */
ret_code_t time_base_init(void);

/**@brief Returns the number of milliseconds since an epoch.
 * @details
 * The epoch is defined by the device user and can be anything. It is changed
 * whenever @ref time_base_set() is called. Due to this, monotonicity cannot be
 * guaranteed by this module.
 *
 * @returns   The current time value in milliseconds.
 */
uint64_t time_base_get(void);

/**@brief Set the current time.
 * @details
 * The internal time will be set immediately and continue running from there.
 *
 * @param[in] time    The new time value to set.
 */
void time_base_set(uint64_t time);

#endif // TIME_BASE_H
