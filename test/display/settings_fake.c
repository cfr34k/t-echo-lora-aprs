
#include <stddef.h>

#include "../../src/settings.h"

#include "sdk_fake.h"

ret_code_t settings_query(settings_id_t id, uint8_t *data, size_t *data_len)
{
	switch(id) {
		case SETTINGS_ID_LAST_BLE_SYMBOL:
			data[0] = '/';
			data[1] = 'x';
			*data_len = 2;
			break;

		default:
			return NRF_ERROR_INVALID_PARAM;
	}

	return NRF_SUCCESS;
}
