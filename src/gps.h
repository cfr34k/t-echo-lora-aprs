#ifndef GPS_H
#define GPS_H

#include <stdbool.h>

#include <sdk_errors.h>

#include "nmea.h"

typedef void (* gps_callback_t)(const nmea_data_t *data);

ret_code_t gps_init(gps_callback_t callback);

void gps_loop(void);

void gps_config_gpios(bool power_supplied);

ret_code_t gps_reset(void);

ret_code_t gps_power_on(void);
ret_code_t gps_power_off(void);

#endif // GPS_H
