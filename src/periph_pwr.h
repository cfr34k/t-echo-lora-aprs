#ifndef PERIPH_PWR_H
#define PERIPH_PWR_H

#include <sdk_errors.h>

#include <stdint.h>

#define PERIPH_PWR_FLAG_INIT                (1 << 0)
#define PERIPH_PWR_FLAG_CONNECTED           (1 << 1)
#define PERIPH_PWR_FLAG_VOLTAGE_MEASUREMENT (1 << 2)
#define PERIPH_PWR_FLAG_EPAPER_UPDATE       (1 << 3)
#define PERIPH_PWR_FLAG_GPS                 (1 << 4)
#define PERIPH_PWR_FLAG_LORA                (1 << 5)
#define PERIPH_PWR_FLAG_LEDS                (1 << 6)

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


#endif // PERIPH_PWR_H
