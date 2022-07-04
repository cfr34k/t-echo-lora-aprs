#include <stddef.h>

#include "lora.h"


static lora_pwr_t m_power = LORA_PWR_PLUS_10_DBM; // play it safe

const char *LORA_PWR_STRINGS[LORA_PWR_NUM_ENTRIES] = {
	"+22 dBm", // LORA_PWR_PLUS_22_DBM
	"+20 dBm", // LORA_PWR_PLUS_20_DBM
	"+17 dBm", // LORA_PWR_PLUS_17_DBM
	"+14 dBm", // LORA_PWR_PLUS_14_DBM
	"+10 dBm", // LORA_PWR_PLUS_10_DBM
	"0 dBm",   // LORA_PWR_PLUS_0_DBM
	"-9 dBm"   // LORA_PWR_MINUS_9_DBM
};


ret_code_t lora_set_power(lora_pwr_t power)
{
	if(power >= LORA_PWR_NUM_ENTRIES) {
		return NRF_ERROR_INVALID_PARAM;
	}

	m_power = power;

	return NRF_SUCCESS;
}


lora_pwr_t lora_get_power(void)
{
	return m_power;
}


const char* lora_power_to_str(lora_pwr_t power)
{
	if(power >= LORA_PWR_NUM_ENTRIES) {
		return NULL;
	}

	return LORA_PWR_STRINGS[power];
}
