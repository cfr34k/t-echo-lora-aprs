#ifndef LORA_H
#define LORA_H

#include <stdint.h>
#include <stdbool.h>

#include <sdk_errors.h>

typedef enum
{
	LORA_EVT_CONFIGURED_IDLE,
	LORA_EVT_TX_STARTED,
	LORA_EVT_TX_COMPLETE,
	LORA_EVT_RX_STARTED,
	LORA_EVT_PACKET_RECEIVED,
	LORA_EVT_OFF,
} lora_evt_t;

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

#endif // LORA_H
