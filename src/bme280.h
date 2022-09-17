#ifndef BME280_H
#define BME280_H

#include <stdint.h>
#include <stdbool.h>

#ifdef SDL_DISPLAY
	// compiling for the display emulator
	#include "sdk_fake.h"
#else
	#include <sdk_errors.h>
#endif


typedef enum {
	BME280_EVT_INIT_DONE,
	BME280_EVT_INIT_NOT_PRESENT,

	BME280_EVT_READOUT_COMPLETE,

	BME280_EVT_COMMUNICATION_ERROR //!< Indicates a fatal communication error. bme280_init() must be called again.
} bme280_evt_t;

typedef void (*bme280_callback_t)(bme280_evt_t evt);

ret_code_t bme280_init(bme280_callback_t callback);
ret_code_t bme280_start_readout(void);

bool bme280_is_ready(void);

void bme280_powersave(void);

float bme280_get_temperature(void);
float bme280_get_humidity(void);
float bme280_get_pressure(void);

#endif // BME280_H
