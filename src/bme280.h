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

bool bme280_is_present(void);
bool bme280_is_ready(void);

float bme280_get_temperature(void);
float bme280_get_humidity(void);
float bme280_get_pressure(void);

void bme280_loop(void);

#endif // BME280_H
