#include <sdk_macros.h>

#include "pinout.h"

#include "buttons.h"

#define NBUTTONS 1

static app_button_cfg_t btn_config[NBUTTONS] = {
	{PIN_BTN_TOUCH, APP_BUTTON_ACTIVE_LOW, NRF_GPIO_PIN_NOPULL, NULL}
};


ret_code_t buttons_init(app_button_handler_t callback)
{
	for(int i = 0; i < NBUTTONS; i++) {
		btn_config[i].button_handler = callback;
	}

	VERIFY_SUCCESS(app_button_init(btn_config, NBUTTONS, 50));

	return app_button_enable();
}
