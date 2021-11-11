#ifndef LORA_H
#define LORA_H

#include <stdint.h>
#include <stdbool.h>

#include <sdk_errors.h>

void lora_config_gpios(bool power_supplied);
ret_code_t lora_init(void);
ret_code_t lora_send_packet(uint8_t *data, uint8_t length);
bool lora_is_busy(void);
void lora_loop(void);

#endif // LORA_H
