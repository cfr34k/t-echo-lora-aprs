#ifndef VOLTAGE_MONITOR_H
#define VOLTAGE_MONITOR_H

#include "nrfx_saadc.h"

#include "pinout.h"

#define VOLTAGE_MONITOR_CHANNEL_COUNT 1

/* Port mapping. */
#define VOLTAGE_MONITOR_VBAT_SAADC_PORT        SAADC_INPUT_VBAT

/* Output array mapping. */
#define VOLTAGE_MONITOR_VBAT_RESULT_INDEX      0

/* Power state bit indices */
#define VOLTAGE_MONITOR_STATE_IDX_ALLOW_WAKEUP   0 // if this flag disappears, the system shall go to deep sleep


/**@brief Callback type for the voltage monitor library.
 *
 * @param[in]  meas_millivolt    The measured voltages, in millivolt.
 * @param[in]  bat_percent       The battery state in percent.
 */
typedef void (*voltage_monitor_callback_t)(int16_t *meas_millivolt, uint8_t bat_percent);

/**@brief Initialize the voltage monitor module.
 * @details
 * This function initializes the internal state and also prepares the necessary
 * hardware (primarily the SAADC).
 *
 * @param[in] callback     A pointer to the function to call when sampling completes.
 * @retval                 The result of the initialization.
 */
ret_code_t voltage_monitor_init(voltage_monitor_callback_t callback);

/**@brief Start voltage monitoring.
 * @details
 * All monitored voltages will be sampled at the given interval. The callback
 * function given to @ref voltage_monitor_init() will be called once for each
 * set of sampled voltages.
 *
 * @param[in] interval_sec   The interval (in seconds) at which the voltages are measured.
 * @retval    err_code       The error code of the operation.
 */
ret_code_t voltage_monitor_start(uint32_t interval_sec);

/**@brief Stop voltage monitoring.
 * @details
 * Stop the internal timer triggering the voltage measurement.
 *
 * @retval    err_code       The error code of the operation.
 */
ret_code_t voltage_monitor_stop(void);

/**@brief Start a measurement immediately.
 * @details
 * This starts a single measurement independent of the interval timer. @ref
 * voltage_monitor_init() must have been called before, but it is not necessary
 * to call @ref voltage_monitor_start() before using this function.
 *
 * @retval    err_code       The error code of the operation.
 */
ret_code_t voltage_monitor_trigger(void);

/**@brief Get the current power state.
 * @details
 * Reports which modules can currently be used based on the battery state.
 *
 * @returns   A bit field containing the one-bit state of each module. If the
 *            bit is '1', the corresponding module can still be used.
 */
uint8_t voltage_monitor_get_power_state(void);

#endif // VOLTAGE_MONITOR_H
