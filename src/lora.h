#ifndef LORA_H
#define LORA_H

#include <stdint.h>
#include <stdbool.h>

#include <sdk_errors.h>

typedef enum
{
	LORA_EVT_PACKET_RECEIVED,
} lora_evt_t;

typedef void (* lora_callback_t)(lora_evt_t evt, const uint8_t *data, uint8_t len);

void lora_config_gpios(bool power_supplied);
ret_code_t lora_init(lora_callback_t callback);
ret_code_t lora_send_packet(const uint8_t *data, uint8_t length);
bool lora_is_busy(void);
void lora_loop(void);

#endif // LORA_H
