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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

float m_lat;
float m_lon;
float m_alt_m;
time_t m_time;

char m_dest[16];
char m_src[16];

char    m_path[8][16];
uint8_t m_npath;

uint8_t m_info[APRS_MAX_INFO_LEN];

char m_table;
char m_icon;
char m_comment[APRS_MAX_COMMENT_LEN+1];


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

static void update_info_field(void)
{
	float lat = m_lat;
	float lon = m_lon;

	char lat_ns, lon_ew;
	int lat_deg, lon_deg;
	int lat_min, lon_min;
	int lat_min_fract, lon_min_fract;

	float alt_ft;

	struct tm *tms;

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

	// calculate integer arc minutes
	lat_min = ((lat - lat_deg) * 60);
	lon_min = ((lon - lon_deg) * 60);

	// calculate fractional arc minutes
	lat_min_fract = (lat * 6000 - lat_deg * 6000 - lat_min * 100);
	lon_min_fract = (lon * 6000 - lon_deg * 6000 - lon_min * 100);

	// convert meters to feet
	alt_ft = m_alt_m / 0.3048;

	// generate time string from timestamp
	tms = gmtime(&m_time);

	/* Time as ddhhmm
	sprintf((char*)m_info, "/%02i%02i%02iz%02i%05.2f%c/%03i%05.2f%c%c%s /a=%06i",
			tms->tm_mday, tms->tm_hour, tms->tm_min,
			lat_deg, lat_min, lat_ns,
			lon_deg, lon_min, lon_ew,
			ICON_DATA[m_icon].aprs_char, m_comment,
			(int)alt_ft);
	*/
	/*
	// time as hhmmss
	snprintf((char*)m_info, sizeof(m_info), "/%02i%02i%02ih%02i%02i.%02i%c/%03i%02i.%02i%c%c/A=%06i %s",
			tms->tm_hour, tms->tm_min, tms->tm_sec,
			lat_deg, lat_min, lat_min_fract, lat_ns,
			lon_deg, lon_min, lon_min_fract, lon_ew,
			m_icon_map[m_icon], (int)alt_ft, m_comment);
	*/
	// no time at all
	(void)tms;
	snprintf((char*)m_info, sizeof(m_info), "!%02i%02i.%02i%c%c%03i%02i.%02i%c%c%s /A=%06i",
			lat_deg, lat_min, lat_min_fract, lat_ns, m_table,
			lon_deg, lon_min, lon_min_fract, lon_ew, m_icon,
			m_comment, (int)alt_ft);
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
}

void aprs_set_dest(const char *dest)
{
	strncpy(m_dest, dest, sizeof(m_dest));
}

void aprs_set_source(const char *call)
{
	strncpy(m_src, call, sizeof(m_src));
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

void aprs_set_icon_default(aprs_icon_t icon)
{
	m_table = '/';
	m_icon  = m_icon_map[icon];
}

void aprs_set_comment(const char *comment)
{
	strncpy(m_comment, comment, APRS_MAX_COMMENT_LEN);
}

size_t aprs_build_frame(uint8_t *frame)
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

	update_info_field();

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


static int parse_location_and_symbol(const char *start, float *lat, float *lon, char *table, char *symbol)
{
	const char *orig_start = start;

	char buf[8];

	// parse degrees (integer)
	memcpy(buf, start, 2);
	buf[2] = '\0';

	char *endptr;
	unsigned long deg = strtoul(buf, &endptr, 10);
	if(endptr == buf) {
		return -1;
	}

	start += 2;

	// parse minutes (with two fractional digits)
	memcpy(buf, start, 5);
	buf[5] = '\0';

	float minutes = strtof(buf, &endptr);
	if(endptr == buf) {
		return -1;
	}

	start += 5;

	*lat = (float)deg + minutes / 60.0f;

	if(*start == 'S') {
		*lat = -*lat;
	} else if(*start != 'N') {
		return -1;
	}

	start++;

	*table = *start;
	start++;

	// same as above for the longitude

	// parse degrees (integer)
	memcpy(buf, start, 3);
	buf[3] = '\0';

	deg = strtoul(buf, &endptr, 10);
	if(endptr == buf) {
		return -1;
	}

	start += 3;

	// parse minutes (with two fractional digits)
	memcpy(buf, start, 5);
	buf[5] = '\0';

	minutes = strtof(buf, &endptr);
	if(endptr == buf) {
		return -1;
	}

	start += 5;

	*lon = (float)deg + minutes / 60.0f;

	if(*start == 'W') {
		*lon = -*lon;
	} else if(*start != 'E') {
		return -1;
	}

	start++;

	*symbol = *start;
	start++;

	return start - orig_start; // number of parsed characters
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
		return false;
	}

	textframe += ret + 1; // “remove” the processed text from the buffer

	// extract the destination call (variant 1: path appended)
	ret = extract_text_until(textframe, ',', result->dest, sizeof(result->dest));
	if(ret >= 0) {
		// destination successfully extracted => extract path
		textframe += ret + 1; // “remove” the processed text from the buffer

		ret = extract_text_until(textframe, ':', result->via, sizeof(result->via));
		if(ret <= 0) {
			return false;
		}
	} else {
		// there is no path appended, so destination is everything until ':'
		ret = extract_text_until(textframe, ':', result->dest, sizeof(result->dest));
		if(ret <= 0) {
			return false;
		}
	}

	textframe += ret + 1; // “remove” the processed text from the buffer
	
	char type = *textframe;
	textframe++;

	switch(type) {
		case '!':
		case '=':
			// position without timestamp
			ret = parse_location_and_symbol(textframe, &result->lat, &result->lon, &result->table, &result->symbol);
			break;

		case '/':
		case '@':
			// position with timestamp
			textframe += 7; // skip the timestamp for now

			ret = parse_location_and_symbol(textframe, &result->lat, &result->lon, &result->table, &result->symbol);
			break;

		default:
			// cannot parse this type
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
	} else {
		result->alt = 0.0f;
	}

	// fill comment
	if(textframe < endptr) {
		size = endptr - textframe - 1;
		if(size > sizeof(result->comment)) {
			size = sizeof(result->comment) - 1;
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
		return false;
	}
}
