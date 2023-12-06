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

#ifndef BUTTONS_H
#define BUTTONS_H

#include <app_button.h>

#define BUTTONS_BTN_TOUCH 0
#define BUTTONS_BTN_1     1

#define BUTTONS_EVT_LONGPRESS 0x11

typedef void (*buttons_callback_t)(uint8_t btn_id, uint8_t evt);

bool buttons_button_is_pressed(uint8_t btn);

ret_code_t buttons_init(buttons_callback_t callback);

/**@brief Disable button activity detection.
 * @details
 * Disables the interrupts on the button pins. This also prevents the system
 * from waking up on a button press when it goes to deep sleep.
 */
ret_code_t buttons_disable(void);

#endif // BUTTONS_H
