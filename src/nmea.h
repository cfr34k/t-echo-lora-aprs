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

#ifndef NMEA_H
#define NMEA_H

#include <stdbool.h>

#include <sdk_errors.h>

#define NMEA_SYS_ID_INVALID 0
#define NMEA_SYS_ID_GPS     1
#define NMEA_SYS_ID_GLONASS 2
#define NMEA_SYS_ID_GALILEO 3
#define NMEA_SYS_ID_BEIDOU  4
#define NMEA_SYS_ID_QZSS    5
#define NMEA_SYS_ID_NAVIC   6

#define NMEA_FIX_TYPE_NONE  0
#define NMEA_FIX_TYPE_2D    1
#define NMEA_FIX_TYPE_3D    2

// maximum number of simultaneously provided fix info structures
#define NMEA_NUM_FIX_INFO   3

// number of tracked satellites per satellite system
#define NMEA_NUM_SAT_INFO   32

typedef struct
{
	uint8_t sys_id;
	uint8_t fix_type;
	bool    auto_mode;
	uint8_t sats_used;
} nmea_fix_info_t;

typedef struct
{
	uint8_t sat_id;
	int8_t  snr;
} nmea_sat_info_t;

typedef struct
{
	float lat;
	float lon;
	float altitude;
	bool  pos_valid;

	float speed;               // meters per second
	float heading;             // degrees to north (0 - 360°)
	bool  speed_heading_valid;

	nmea_fix_info_t fix_info[NMEA_NUM_FIX_INFO];

	nmea_sat_info_t sat_info_gps[NMEA_NUM_SAT_INFO];
	nmea_sat_info_t sat_info_glonass[NMEA_NUM_SAT_INFO];

	uint8_t sat_info_count_gps;
	uint8_t sat_info_count_glonass;

	float pdop;
	float hdop;
	float vdop;
} nmea_data_t;


/**@brief (Try to) parse the given NMEA sentence.
 * @details
 * The parsed data is stored in the given struct. TODO
 *
 * Note that this function is destructive, i.e. the given sentence will be
 * modified. Separators will be replaced by null-bytes. This is done to save
 * memory and copy operations.
 *
 * If any error is detected while parsing (no/wrong checksum), parsing will be
 * aborted, the error will be logged and no output data will be modified.
 *
 * @param[in]  sentence           The sentence to parse. Will be modified while parsing.
 * @param[out] position_updated   Indicates whether the position was updated by this sentence.
 *                                May be NULL if not needed.
 * @param[inout] data             The data struct to fill/update.
 * @retval NRF_ERROR_INVALID_DATA     The given string was not a valid NMEA sentence.
 * @retval NRF_SUCCESS                If the sentence was parsed successfully.
 */
ret_code_t nmea_parse(char *sentence, bool *position_updated, nmea_data_t *data);

/**@brief Retrieve a string for the given fix type.
 */
const char* nmea_fix_type_to_string(uint8_t fix_type);

/**@brief Retrieve a short system name for the given system ID.
 */
const char* nmea_sys_id_to_short_name(uint8_t sys_id);

#endif // NMEA_H
