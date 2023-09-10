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

#ifndef CONFIG_H
#define CONFIG_H

/* Default values that are used if no configuration is found in the flash memory. Change to your needs.
 *
 * Note: all settings can be changed via BLE and are stored persistently. So
 * this section is only relevant if the firmware is first flashed after erasing
 * the settings flash area.
 */

// Source call sign: transmitter is disabled if empty
#define APRS_SOURCE ""

// APRS Destination: identifies this firmware. If you want to change it for
// experiments, please use the "APZ???" range that is assigned for that
// specific purpose. If you plan to release your own firmware based on my work,
// please request an official TOCALL allocation for it here:
// https://github.com/aprsorg/aprs-deviceid
#define APRS_DESTINATION "APLETK"

// APRS Comment to be inserted in your packets (keep it short!)
#define APRS_COMMENT "T-Echo"

// APRS symbol table and icon identifier. "/." is a generic X icon.fi.
#define APRS_SYMBOL_TABLE '/'
#define APRS_SYMBOL_ICON  '.'

#endif // CONFIG_H
