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

#ifndef LORA_H
#define LORA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef SDL_DISPLAY
	// compiling for the display emulator
	#include "sdk_fake.h"
#else
	#include <sdk_errors.h>
#endif

typedef enum
{
	LORA_EVT_CONFIGURED_IDLE,
	LORA_EVT_TX_STARTED,
	LORA_EVT_TX_COMPLETE,
	LORA_EVT_RX_STARTED,
	LORA_EVT_PACKET_RECEIVED,
	LORA_EVT_OFF,
} lora_evt_t;

typedef enum
{
	LORA_PWR_PLUS_22_DBM = 0,
	LORA_PWR_PLUS_20_DBM,
	LORA_PWR_PLUS_17_DBM,
	LORA_PWR_PLUS_14_DBM,
	LORA_PWR_PLUS_10_DBM,
	LORA_PWR_PLUS_0_DBM,
	LORA_PWR_MINUS_9_DBM,

	LORA_PWR_NUM_ENTRIES,
} lora_pwr_t;

extern const char *LORA_PWR_STRINGS[LORA_PWR_NUM_ENTRIES];

typedef union
{
	struct {
		uint8_t *data;
		uint8_t  data_len;

		float rssi;
		float snr;
		float signalRssi;
	} rx_packet_data;
} lora_evt_data_t;

typedef void (* lora_callback_t)(lora_evt_t evt, const lora_evt_data_t *data);

void lora_config_gpios(bool power_supplied);
ret_code_t lora_init(lora_callback_t callback);
ret_code_t lora_power_on(void);
void lora_power_off(void);
ret_code_t lora_send_packet(const uint8_t *data, uint8_t length);
ret_code_t lora_start_rx(void);
bool lora_is_busy(void);
void lora_loop(void);

ret_code_t lora_set_power(lora_pwr_t power);
lora_pwr_t lora_get_power(void);
const char* lora_power_to_str(lora_pwr_t power);

#endif // LORA_H
