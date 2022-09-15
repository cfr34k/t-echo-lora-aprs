#ifndef BME280_COMP_H
#define BME280_COMP_H

#include <stdint.h>

/*!@brief Calculate temperature from BME280 raw sensor value.
 * @returns The temperature in °C.
 */
float bme280_comp_temperature(int32_t adc);

/*!@brief Calculate relative humidity from BME280 raw sensor value.
 * @returns The relative humidity in %.
 */
float bme280_comp_humidity(int32_t adc);

/*!@brief Calculate pressure from BME280 raw sensor value.
 * @returns The pressure in hPa.
 */
float bme280_comp_pressure(int32_t adc);

#endif // BME280_COMP_H
