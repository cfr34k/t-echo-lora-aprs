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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <assert.h>

#include <math.h>

#include "aprs.h"

const char m_icon_map[APRS_NUM_ICONS] = {
	'.', // AI_X
	'[', // AI_JOGGER
	'b', // AI_BIKE
	'>', // AI_CAR
	'j', // AI_JEEP
	'v', // AI_VAN
	'k', // AI_TRUCK
	'U', // AI_BUS
	'O', // AI_BALLOON
	'R', // AI_RECREATIONAL_VEHICLE
	'X', // AI_HELICOPTER
	'Y', // AI_YACHT
	'a', // AI_AMBULANCE
	'f', // AI_FIRE_TRUCK
	's', // AI_SHIP
};

const char * const m_icon_names[APRS_NUM_ICONS] = {
	"X",            // AI_X
	"Jogger",       // AI_JOGGER
	"Bike",         // AI_BIKE
	"Car",          // AI_CAR
	"Jeep",         // AI_JEEP
	"Van",          // AI_VAN
	"Truck",        // AI_TRUCK
	"Bus",          // AI_BUS
	"Balloon",      // AI_BALLOON
	"Rec. Vehicle", // AI_RECREATIONAL_VEHICLE
	"Helicopter",   // AI_HELICOPTER
	"Yacht",        // AI_YACHT
	"Ambulance",    // AI_AMBULANCE
	"Fire Truck",   // AI_FIRE_TRUCK
	"Ship",         // AI_SHIP
};

static float m_lat;
static float m_lon;
static float m_alt_m;
static time_t m_time;

static char m_dest[16];
static char m_src[16];

static char    m_path[8][16];
static uint8_t m_npath;

static uint8_t m_info[APRS_MAX_INFO_LEN];

static char m_table;
static char m_icon;
static char m_comment[APRS_MAX_COMMENT_LEN+1];

static char m_error_message[256];

static uint32_t m_config_flags;

static aprs_rx_history_t m_rx_history;


static void append_address(uint8_t **frameptr, char *addr, uint8_t is_last)
{
	size_t len = strlen(addr);
	strncpy((char*)*frameptr, addr, len);
	*frameptr += len;

	if(!is_last) {
		**frameptr = ',';
		(*frameptr)++;
	}
}

static char* encode_position_readable(char *str, size_t max_len)
{
	float lat = m_lat;
	float lon = m_lon;

	char lat_ns, lon_ew;
	int lat_deg, lon_deg;
	int lat_min_full_precision, lon_min_full_precision;
	int lat_min, lon_min;
	int lat_min_fract, lon_min_fract;

	char dao[6];

	// convert sign -> north/south, east/west
	if(lat < 0) {
		lat = -lat;
		lat_ns = 'S';
	} else {
		lat_ns = 'N';
	}

	if(lon < 0) {
		lon = -lon;
		lon_ew = 'W';
	} else {
		lon_ew = 'E';
	}

	// calculate integer degrees
	lat_deg = (int)lat;
	lon_deg = (int)lon;

	// calculate arc minutes with 4 fractional digits
	lat_min_full_precision = ((lat - lat_deg) * 600000);
	lon_min_full_precision = ((lon - lon_deg) * 600000);

	// calculate integer arc minutes
	lat_min = lat_min_full_precision / 10000;
	lon_min = lon_min_full_precision / 10000;

	// calculate fractional arc minutes (base precision)
	lat_min_fract = (lat_min_full_precision / 100) % 100;
	lon_min_fract = (lon_min_full_precision / 100) % 100;

	// calculate the DAO string if requested
	if(m_config_flags & APRS_FLAG_ADD_DAO) {
		dao[0] = dao[4] = '!'; // start and end markers
		dao[1] = 'w';          // WGS84 identifier
		dao[5] = '\0';         // String terminator

		// extract extended precision part
		int lat_min_fract_extended = lat_min_full_precision % 100;
		int lon_min_fract_extended = lon_min_full_precision % 100;

		// encode extended precision part as Base-91
		dao[2] = '!' + lat_min_fract_extended * 91 / 100; // note: integer division!
		dao[3] = '!' + lon_min_fract_extended * 91 / 100; // note: integer division!
	} else {
		dao[0] = '\0';
	}

	int ret = snprintf(str, max_len, "%02i%02i.%02i%c%c%03i%02i.%02i%c%c%s",
			lat_deg, lat_min, lat_min_fract, lat_ns, m_table,
			lon_deg, lon_min, lon_min_fract, lon_ew, m_icon,
			dao);

	if(ret < 0) {
		return NULL; // error
	} else if(ret < max_len) {
		return str + ret; // everything encoded ok
	} else {
		return str + max_len - 1; // string was truncated
	}
}

static char* encode_position_compressed(char *str, size_t max_len)
{
	/*
	 * compressed format: /YYYYXXXX$csT
	 * where:
	 * /    = symbol table
	 * YYYY = compressed latitude (base-91 encoded)
	 * XXXX = compressed longitude (base-91 encoded)
	 * $    = icon
	 * cs   = compressed altitude (alternatives: course/speed, radio range)
	 * T    = compression type (bitmask, base-91 encoded)
	 */

	if(max_len < 13) {
		return str; // length too small for fixed-length field. Do nothing.
	}

	// set APRS symbol
	str[0] = m_table;
	str[9] = m_icon;

	// compressed latitude calculation
	uint32_t lat_compressed = (90.0f - m_lat) * 380926.0f;

	// base-91 encoding
	for(uint8_t i = 0; i < 4; i++) {
		str[4 - i] = '!' + (lat_compressed % 91);
		lat_compressed /= 91;
	}

	// compressed longitude calculation
	uint32_t lon_compressed = (180.0f + m_lon) * 190463.0f;

	// base-91 encoding
	for(uint8_t i = 0; i < 4; i++) {
		str[8 - i] = '!' + (lon_compressed % 91);
		lon_compressed /= 91;
	}

	// compressed altitude calculation
	// encoded value = log_1.002(altitude in feet)

	float alt_ft = m_alt_m / 0.3048f;
	if(alt_ft < 1) {
		alt_ft = 1; // prevent exception in the logarithm
	}

	uint32_t alt_encoded = (uint32_t)(logf(alt_ft) / 0.00199800266f); // the magic constant is ln(1.002)

	str[10] = '!' + (alt_encoded / 91) % 91;
	str[11] = '!' + alt_encoded % 91;

	// Type byte
	uint8_t type = (1 << 5) /* current position */
	             | (2 << 3) /* source = GGA (necessary for altitude encoding) */
	             | (0 << 0) /* origin = compressed */;

	str[12] = '!' + type;

	str[13] = '\0';

	return str + 13;
}

static char* encode_altitude_readable(char *str, size_t max_len)
{
	float alt_ft = m_alt_m / 0.3048f;

	int ret = snprintf(str, max_len, "/A=%06i", (int)alt_ft);

	if(ret < 0) {
		return NULL; // error
	} else if(ret < max_len) {
		return str + ret; // everything encoded ok
	} else {
		return str + max_len - 1; // string was truncated
	}
}

static char* encode_frame_id(char *str, size_t max_len, uint32_t frame_id)
{
	if(!(m_config_flags & APRS_FLAG_ADD_FRAME_COUNTER)) {
		return str;
	}

	int ret = snprintf(str, max_len, " #%lu", frame_id);

	if(ret < 0) {
		return NULL; // error
	} else if(ret < max_len) {
		return str + ret; // everything encoded ok
	} else {
		return str + max_len - 1; // string was truncated
	}
}

static char* encode_vbat(char *str, size_t max_len, uint16_t vbat_millivolt)
{
	if(!(m_config_flags & APRS_FLAG_ADD_VBAT)) {
		return str;
	}

	uint16_t vbat_int = vbat_millivolt / 1000;
	uint16_t vbat_fract = (vbat_millivolt / 10) % 100;

	int ret = snprintf(str, max_len, " %d.%02dV", vbat_int, vbat_fract);

	if(ret < 0) {
		return NULL; // error
	} else if(ret < max_len) {
		return str + ret; // everything encoded ok
	} else {
		return str + max_len - 1; // string was truncated
	}
}

static char* encode_weather(char *str, size_t max_len, const aprs_args_t *args)
{
	if(!(m_config_flags & APRS_FLAG_ADD_WEATHER)) {
		return str;
	}

	if(!args->transmit_env_data) {
		// data was already transmitted or no data available
		return str;
	}

	int32_t temp_fahrenheit = (int32_t)((args->temperature_celsius * 9.0f / 5.0f + 32.0f) + 0.5f);
	int32_t humidity = (int32_t)(args->humidity_rH + 0.5f) % 100; // h00 = 100%
	int32_t pressure_dPa = (int32_t)(args->pressure_hPa * 10.0f + 0.5f); // resolution = 0.1 hPa = 10 Pa

	int ret = snprintf(str, max_len, " t%03ldh%02ldb%05ld", temp_fahrenheit, humidity, pressure_dPa);

	if(ret < 0) {
		return NULL; // error
	} else if(ret < max_len) {
		return str + ret; // everything encoded ok
	} else {
		return str + max_len - 1; // string was truncated
	}
}

static void update_info_field(const aprs_args_t *args)
{
	char *info_end = (char*)m_info + sizeof(m_info);
	char *infoptr = (char*)m_info;
	char *retptr;

	/* packet type: position, no APRS messaging */
	*infoptr = '!';
	infoptr++;

	/* encode position */

	if(m_config_flags & APRS_FLAG_COMPRESS_LOCATION) {
		retptr = encode_position_compressed(infoptr, info_end - infoptr);
	} else {
		retptr = encode_position_readable(infoptr, info_end - infoptr);
	}

	if(retptr) {
		infoptr = retptr;
	}

	/* add comment */
	size_t chars_to_copy_from_comment = strlen(m_comment);
	if((chars_to_copy_from_comment + 1) > (info_end - infoptr)) {
		chars_to_copy_from_comment = info_end - infoptr - 1;
	}

	strncpy(infoptr, m_comment, chars_to_copy_from_comment);

	infoptr += chars_to_copy_from_comment;
	*infoptr = '\0';

	/* add altitude for uncompressed packets (already included in compressed format) */
	if(!(m_config_flags & APRS_FLAG_COMPRESS_LOCATION)
			&& (m_config_flags & APRS_FLAG_ADD_ALTITUDE)) {
		retptr = encode_altitude_readable(infoptr, info_end - infoptr);
		if(retptr) {
			infoptr = retptr;
		}
	}

	/* add frame counter */
	retptr = encode_frame_id(infoptr, info_end - infoptr, args->frame_id);
	if(retptr) {
		infoptr = retptr;
	}

	/* add Vbat */
	retptr = encode_vbat(infoptr, info_end - infoptr, args->vbat_millivolt);
	if(retptr) {
		infoptr = retptr;
	}

	/* add weather report */
	retptr = encode_weather(infoptr, info_end - infoptr, args);
	if(retptr) {
		infoptr = retptr;
	}
}

// PUBLIC FUNCTIONS

void aprs_init(void)
{
	m_dest[0] = '\0';
	m_src[0] = '\0';

	for(uint8_t i = 0; i < 8; i++) {
		m_path[i][0] = '\0';
	}

	m_npath = 0;

	m_table = '/'; // default table
	m_icon = m_icon_map[AI_X];

	m_comment[0] = '\0';
	m_comment[APRS_MAX_COMMENT_LEN] = '\0';

	m_rx_history.num_entries = 0;

	// default flags (compatible with v0.3)
	m_config_flags = APRS_FLAG_ADD_FRAME_COUNTER | APRS_FLAG_ADD_ALTITUDE;
}

void aprs_set_dest(const char *dest)
{
	strncpy(m_dest, dest, sizeof(m_dest));
}

void aprs_get_dest(char *dest, size_t dest_len)
{
	strncpy(dest, m_dest, dest_len);
}

void aprs_set_source(const char *call)
{
	strncpy(m_src, call, sizeof(m_src));
}

void aprs_get_source(char *source, size_t source_len)
{
	strncpy(source, m_src, source_len);
}

void aprs_clear_path()
{
	m_npath = 0;
}

uint8_t aprs_add_path(const char *call)
{
	if(m_npath == 8) {
		return 0;
	} else {
		strncpy(m_path[m_npath], call, sizeof(m_path[0]));

		m_npath++;

		return 1;
	}
}

void aprs_update_pos_time(float lat, float lon, float alt_m, time_t t)
{
	m_lat = lat;
	m_lon = lon;
	m_alt_m = alt_m;
	m_time = t;
}

void aprs_set_icon(char table, char icon)
{
	m_table = table;
	m_icon  = icon;
}

void aprs_get_icon(char *table, char *icon)
{
	*table = m_table;
	*icon = m_icon;
}

void aprs_set_icon_default(aprs_icon_t icon)
{
	m_table = '/';
	m_icon  = m_icon_map[icon];
}

void aprs_set_comment(const char *comment)
{
	strncpy(m_comment, comment, APRS_MAX_COMMENT_LEN);
}

bool aprs_can_build_frame(void)
{
	return (m_src[0] != '\0') && (m_dest[0] != '\0');
}

size_t aprs_build_frame(uint8_t *frame, const aprs_args_t *args)
{
	uint8_t *frameptr = frame;
	uint8_t *infoptr = m_info;
	//uint16_t fcs;

	*(frameptr++) = '<';
	*(frameptr++) = 0xFF;
	*(frameptr++) = 0x01;

	append_address(&frameptr, m_src, 1);
	*(frameptr++) = '>';
	append_address(&frameptr, m_dest, (m_npath == 0) ? 1 : 0);

	for(uint8_t i = 0; i < m_npath; i++) {
		append_address(&frameptr, m_path[i], (m_npath == (i+1)) ? 1 : 0);
	}

	*(frameptr++) = ':';

	update_info_field(args);

	while(*infoptr != '\0') {
		*frameptr = *infoptr;

		frameptr++;
		infoptr++;
	}

#if 0
	fcs = calculate_fcs(frame, (frameptr-frame));

	//*(frameptr++) = bit_reverse(fcs >> 8);
	//*(frameptr++) = bit_reverse(fcs & 0xFF);
	*(frameptr++) = fcs & 0xFF;
	*(frameptr++) = fcs >> 8;
#endif

	*frameptr = '\0';

	return (size_t)(frameptr - frame);
}


uint32_t aprs_get_config_flags(void)
{
	return m_config_flags;
}

void aprs_set_config_flags(uint32_t new_flags)
{
	m_config_flags = new_flags;
}

void aprs_enable_config_flag(aprs_flag_t flag)
{
	m_config_flags |= flag;
}

void aprs_disable_config_flag(aprs_flag_t flag)
{
	m_config_flags &= ~flag;
}

void aprs_toggle_config_flag(aprs_flag_t flag)
{
	m_config_flags ^= flag;
}


/*** Parser functions ***/

static int extract_text_until(const char *start, char marker, char *dest, size_t dest_len)
{
	const char *tmpptr = strchr(start, marker);
	if(!tmpptr) {
		return -1;
	}

	size_t size = tmpptr - start; // size of field

	if(size >= dest_len) {
		size = dest_len - 1;
	}

	memcpy(dest, start, size);
	dest[size] = '\0';

	return size;
}


static int parse_location_and_symbol_readable(const char *start, aprs_frame_t *result)
{
	const char *orig_start = start;

	char buf[8];

	// parse degrees (integer)
	memcpy(buf, start, 2);
	buf[2] = '\0';

	char *endptr;
	unsigned long deg = strtoul(buf, &endptr, 10);
	if(endptr == buf) {
		snprintf(m_error_message, sizeof(m_error_message), "Location error: Lat. degrees is not an integer: '%s'.", buf);
		return -1;
	}

	start += 2;

	// parse minutes (with two fractional digits)
	memcpy(buf, start, 5);
	buf[5] = '\0';

	float minutes = strtof(buf, &endptr);
	if(endptr == buf) {
		snprintf(m_error_message, sizeof(m_error_message), "Location error: Lat. minutes is not a float: '%s'.", buf);
		return -1;
	}

	start += 5;

	result->lat = (float)deg + minutes / 60.0f;

	if(*start == 'S') {
		result->lat = -result->lat;
	} else if(*start != 'N') {
		snprintf(m_error_message, sizeof(m_error_message), "Location error: Invalid latitude polarity: '%c'.", *start);
		return -1;
	}

	start++;

	result->table = *start;
	start++;

	// same as above for the longitude

	// parse degrees (integer)
	memcpy(buf, start, 3);
	buf[3] = '\0';

	deg = strtoul(buf, &endptr, 10);
	if(endptr == buf) {
		snprintf(m_error_message, sizeof(m_error_message), "Location error: Lon. degrees is not an integer: '%s'.", buf);
		return -1;
	}

	start += 3;

	// parse minutes (with two fractional digits)
	memcpy(buf, start, 5);
	buf[5] = '\0';

	minutes = strtof(buf, &endptr);
	if(endptr == buf) {
		snprintf(m_error_message, sizeof(m_error_message), "Location error: Lon. minutes is not a float: '%s'.", buf);
		return -1;
	}

	start += 5;

	result->lon = (float)deg + minutes / 60.0f;

	if(*start == 'W') {
		result->lon = -result->lon;
	} else if(*start != 'E') {
		snprintf(m_error_message, sizeof(m_error_message), "Location error: Invalid longitude polarity: '%c'.", *start);
		return -1;
	}

	start++;

	result->symbol = *start;
	start++;

	return start - orig_start; // number of parsed characters
}


static int parse_location_and_symbol_compressed(const char *start, aprs_frame_t *result)
{
	// quick check: ensure that all 13 characters are printable ASCII characters
	for(uint8_t i = 0; i < 13; i++) {
		if(!isprint((int)start[i])) {
			snprintf(m_error_message, sizeof(m_error_message), "Compressed location: Non-printable character at index %d: 0x%02x.", i, start[i]);
			return -1;
		}
	}

	result->table = start[0];
	result->symbol = start[9];

	// check the compression type byte
	uint8_t type = start[12] - '!';
	bool has_altitude = ((type & 0x18) == 0x10);

	if((type & 0xC0) != 0) {
		snprintf(m_error_message, sizeof(m_error_message), "Compression type: unused bits are not 0: 0x%02x.", type);
		return -1;
	}

	// recover the altitude if available
	if(has_altitude) {
		uint32_t alt_encoded = (start[10] - '!') * 91 + (start[11] - '!');
		result->alt = powf(1.002f, alt_encoded) * 0.3048f; // decode and convert to meters
	}

	// recover latitude and longitude
	uint32_t lat_encoded = 0;
	uint32_t lon_encoded = 0;

	for(uint8_t i = 0; i < 4; i++) {
		lat_encoded *= 91;
		lat_encoded += start[1 + i] - '!';

		lon_encoded *= 91;
		lon_encoded += start[5 + i] - '!';
	}

	result->lat = 90.0f - lat_encoded / 380926.0f;
	result->lon = -180.0f + lon_encoded / 190463.0f;

	return 13;
}


static int parse_dao(const char *start, aprs_frame_t *result)
{
	while(start[4] != '\0') {
		if(start[0] == '!' && (start[4] == '!')) {
			// this may be a DAO sequence. Only WGS84 is supported here.
			float lat_enhance_deg_abs;
			float lon_enhance_deg_abs;
			bool pos_enhancement_set = false;

			if(start[1] == 'w') {
				// base91 notation
				uint32_t lat_add_digits = (start[2] - '!') * 100L / 91L;
				uint32_t lon_add_digits = (start[3] - '!') * 100L / 91L;

				lat_enhance_deg_abs = lat_add_digits * 1.666667e-6; // / 60 / 10000
				lon_enhance_deg_abs = lon_add_digits * 1.666667e-6; // / 60 / 10000
				pos_enhancement_set = true;
			} else if(start[1] == 'W') {
				lat_enhance_deg_abs = (start[2] - '0') * 1.666667e-5; // / 60 / 1000
				lon_enhance_deg_abs = (start[3] - '0') * 1.666667e-5; // / 60 / 1000
				pos_enhancement_set = true;
			}

			if(pos_enhancement_set) {
				if(result->lat >= 0) {
					result->lat += lat_enhance_deg_abs;
				} else {
					result->lat -= lat_enhance_deg_abs;
				}

				if(result->lon >= 0) {
					result->lon += lon_enhance_deg_abs;
				} else {
					result->lon -= lon_enhance_deg_abs;
				}
			}

			break;
		}

		start++;
	}

	return 0;
}

static int parse_location_and_symbol(const char *start, aprs_frame_t *result)
{
	// first try to parse human-readable APRS packets
	int ret = parse_location_and_symbol_readable(start, result);
	if(ret >= 0) { // success!
		// find DAO in remaining data
		parse_dao(start+ret, result);
		return ret;
	}

	// parsing as text failed => try again with compressed format. DAO parsing is
	// not necessary in this case.
	ret = parse_location_and_symbol_compressed(start, result);
	return ret;
}


static bool aprs_parse_text_frame(const uint8_t *frame, size_t len, aprs_frame_t *result)
{
	char buf[64];
	size_t size;

	// convert the input pointer to a string
	const char *textframe = (const char*)frame;
	const char *endptr = (const char*)frame + len;

	// extract the source call
	int ret = extract_text_until(textframe, '>', result->source, sizeof(result->source));
	if(ret <= 0) {
		strcpy(m_error_message, "End of source not found.");
		return false;
	}

	textframe += ret + 1; // “remove” the processed text from the buffer
	
	// find end of path character
	const char *end_of_path = strchr(textframe, ':');
	if(!end_of_path) {
		strcpy(m_error_message, "End of path not found.");
		return false;
	}

	// find end of destination
	const char *end_of_dest = strchr(textframe, ',');

	if(!end_of_dest || (end_of_dest > end_of_path)) {
		// There is no path in this message, only the destination
		ret = extract_text_until(textframe, ':', result->dest, sizeof(result->dest));
		if(ret <= 0) {
			strcpy(m_error_message, "End of destination marker not found.");
			return false;
		}
	} else {
		// Message contains the destination as well as additional path entries
		ret = extract_text_until(textframe, ',', result->dest, sizeof(result->dest));
		if(ret <= 0) {
			strcpy(m_error_message, "End of destination marker not found.");
			return false;
		}

		textframe += ret + 1; // “remove” the processed text from the buffer

		ret = extract_text_until(textframe, ':', result->via, sizeof(result->via));
		if(ret <= 0) {
			strcpy(m_error_message, "End of path not found.");
			return false;
		}
	}

	textframe += ret + 1; // “remove” the processed text from the buffer
	
	char type = *textframe;
	textframe++;

	result->alt = 0.0f; // default if altitude is not available

	switch(type) {
		case '!':
		case '=':
			// position without timestamp
			ret = parse_location_and_symbol(textframe, result);
			break;

		case '/':
		case '@':
			// position with timestamp
			textframe += 7; // skip the timestamp for now

			ret = parse_location_and_symbol(textframe, result);
			break;

		default:
			// cannot parse this type
			snprintf(m_error_message, sizeof(m_error_message), "Unknown message type: '%c'", type);
			return false;
	}

	if(ret < 0) {
		return false;
	}

	textframe += ret; // “remove” the processed text from the buffer

	// check if altitude is in remaining data
	char *ptr = strstr(textframe, "/A=");
	if(ptr) {
		memcpy(buf, ptr + 3, 6);

		char *endptr;
		long alt = strtol(buf, &endptr, 10);
		result->alt = (float)alt * 0.3048f; // convert to meters
	}

	// fill comment
	if(textframe < endptr) {
		size = endptr - textframe;
		if(size > sizeof(result->comment)) {
			size = sizeof(result->comment);
		}

		memcpy(result->comment, textframe, size);
		result->comment[size] = '\0';
	} else {
		result->comment[0] = '\0';
	}

	return true;
}


bool aprs_parse_frame(const uint8_t *frame, size_t len, aprs_frame_t *result)
{
	if(len > 3 && frame[0] == '<' && frame[1] == 0xFF && frame[2] == 0x01) {
		return aprs_parse_text_frame(frame + 3, len - 3, result);
	} else {
		strcpy(m_error_message, "Invalid header");
		return false;
	}
}


const char* aprs_get_parser_error(void)
{
	return m_error_message;
}


uint8_t aprs_rx_history_insert(
		const aprs_frame_t *frame,
		const aprs_rx_raw_data_t *raw,
		uint64_t rx_timestamp,
		uint8_t protected_index)
{
	aprs_rx_history_entry_t *insert_pos = NULL;

	// first try: check if the source call sign already exists
	if(insert_pos == NULL) {
		const char *newsrc = frame->source;
		for(uint8_t i = 0; i < m_rx_history.num_entries; i++) {
			const char *oldsrc = m_rx_history.history[i].decoded.source;

			if(strcmp(newsrc, oldsrc) == 0) {
				insert_pos = &m_rx_history.history[i];
				break;
			}
		}
	}

	// second try: append at the end
	if(m_rx_history.num_entries < APRS_RX_HISTORY_SIZE) {
		insert_pos = &m_rx_history.history[m_rx_history.num_entries];
		m_rx_history.num_entries++;
	}

	// third try: replace the oldest entry, but never the protected one (which
	// is likely currently being viewed)
	if(insert_pos == NULL) {
		uint64_t oldest_timestamp = UINT64_MAX;

		for(uint8_t i = 0; i < m_rx_history.num_entries; i++) {
			if(i == protected_index) {
				continue;
			}

			uint64_t timestamp = m_rx_history.history[i].rx_timestamp;
			if(timestamp < oldest_timestamp) {
				oldest_timestamp = timestamp;
				insert_pos = &m_rx_history.history[i];
			}
		}
	}

	assert(insert_pos != NULL);

	insert_pos->decoded = *frame;
	insert_pos->rx_timestamp = rx_timestamp;
	insert_pos->raw = *raw;

	return (insert_pos - m_rx_history.history);
}


const aprs_rx_history_t* aprs_get_rx_history(void)
{
	return &m_rx_history;
}
