#include <sdk_macros.h>
#include <app_timer.h>

#include "pinout.h"

#include "buttons.h"

#define NBUTTONS 2

app_button_handler_t m_callback = NULL;
uint8_t              m_longpress_btn_id;
uint8_t              m_longpress_btn_pin;

APP_TIMER_DEF(m_longpress_timer);

static void cb_app_button(uint8_t pin, uint8_t evt);

static app_button_cfg_t btn_config[NBUTTONS] = {
	{PIN_BTN_TOUCH, APP_BUTTON_ACTIVE_LOW, NRF_GPIO_PIN_NOPULL, cb_app_button},
	{PIN_BUTTON_1,  APP_BUTTON_ACTIVE_LOW, NRF_GPIO_PIN_NOPULL, cb_app_button}
};


static void cb_longpress_timer(void *ctx)
{
	if(app_button_is_pushed(m_longpress_btn_id)) {
		m_callback(m_longpress_btn_pin, BUTTONS_EVT_LONGPRESS);
	}
}


static void cb_app_button(uint8_t pin, uint8_t evt)
{
	if(!m_callback) {
		return;
	}

	m_callback(pin, evt);

	m_longpress_btn_pin = pin;

	for(uint8_t i = 0; i < NBUTTONS; i++) {
		if(btn_config[i].pin_no == pin) {
			m_longpress_btn_id = i;
			break;
		}
	}

	APP_ERROR_CHECK(app_timer_stop(m_longpress_timer));
	APP_ERROR_CHECK(app_timer_start(m_longpress_timer, APP_TIMER_TICKS(2000), NULL));
}


bool buttons_button_is_pressed(uint8_t btn)
{
	return app_button_is_pushed(btn);
}


ret_code_t buttons_init(app_button_handler_t callback)
{
	m_callback = callback;

	VERIFY_SUCCESS(app_button_init(btn_config, NBUTTONS, 50));

	VERIFY_SUCCESS(app_timer_create(&m_longpress_timer, APP_TIMER_MODE_SINGLE_SHOT, cb_longpress_timer));

	return app_button_enable();
}
