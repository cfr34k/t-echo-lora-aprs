#ifndef BUTTONS_H
#define BUTTONS_H

#include <app_button.h>

#define BUTTONS_BTN_TOUCH 0
#define BUTTONS_BTN_1     1

bool buttons_button_is_pressed(uint8_t btn);

ret_code_t buttons_init(app_button_handler_t callback);

#endif // BUTTONS_H
