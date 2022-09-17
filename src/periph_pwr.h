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

#ifndef PERIPH_PWR_H
#define PERIPH_PWR_H

#include <sdk_errors.h>

#include <stdint.h>
#include <stdbool.h>

#define PERIPH_PWR_FLAG_INIT                (1 << 0)
#define PERIPH_PWR_FLAG_CONNECTED           (1 << 1)
#define PERIPH_PWR_FLAG_VOLTAGE_MEASUREMENT (1 << 2)
#define PERIPH_PWR_FLAG_EPAPER_UPDATE       (1 << 3)
#define PERIPH_PWR_FLAG_GPS                 (1 << 4)
#define PERIPH_PWR_FLAG_LORA                (1 << 5)
#define PERIPH_PWR_FLAG_LEDS                (1 << 6)
#define PERIPH_PWR_FLAG_BME280              (1 << 7)

#define PERIPH_PWR_FLAG_ALL                 0xFFFFFFFF

typedef uint32_t periph_pwr_activity_flag_t;

/**@brief Initialize the peripheral power management module.
 * @details
 * All controlled modules will be considered to be off initially.
 */
void periph_pwr_init(void);

/**@brief Ensure the modules related to the given activity are on.
  *
  * @param[in] activity    The activity flag to set.
  * @retval    err_code    Result code from powering up the modules.
  */
ret_code_t periph_pwr_start_activity(periph_pwr_activity_flag_t activity);

/**@brief Allow to power down the modules related to the given activity.
  *
  * @param[in] activity    The activity flag to clear.
  * @retval    err_code    Result code from powering down the modules.
  */
ret_code_t periph_pwr_stop_activity(periph_pwr_activity_flag_t activity);

/**@brief Check whether the necessary modules for the given activity are alreay on.
 * @details
 * This function can be used to check whether the given activity can be enabled
 * without requiring additional modules to be switched on.
  *
  * @param[in] activity    The activity flag to clear.
  * @returns               True if all necessary modules are already powered, false if not.
  */
bool periph_pwr_is_activity_power_already_available(periph_pwr_activity_flag_t activity);


#endif // PERIPH_PWR_H
