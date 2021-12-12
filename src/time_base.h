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
