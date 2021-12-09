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

float m_lat;
float m_lon;
float m_alt_m;
time_t m_time;

aprs_addr_t m_dest;
aprs_addr_t m_src;

aprs_addr_t m_path[8];
uint8_t m_npath;

uint8_t m_info[APRS_MAX_INFO_LEN];

aprs_icon_t m_icon;
char m_comment[APRS_MAX_COMMENT_LEN+1];


#if 0
/*
 * http://n1vg.net/packet/
 *
 * "Start with the 16-bit FCS set to 0xffff. For each data bit sent, shift the
 * FCS value right one bit. If the bit that was shifted off (formerly bit 1)
 * was not equal to the bit being sent, exclusive-OR the FCS value with 0x8408.
 * After the last data bit, take the ones complement (inverse) of the FCS value
 * and send it low-byte first."
 */
static uint16_t calculate_fcs(uint8_t *data, size_t len)
{
	const uint16_t poly = 0x8408;

	uint16_t lfsr = 0xFFFF;

	for(size_t i = 0; i < len; i++) {
		for(int j = 0; j < 8; j++) {
			uint8_t b = (data[i] >> j) & 0x01;
			uint8_t o = lfsr & 0x0001;

			lfsr >>= 1;

			if(b != o) {
				lfsr ^= poly;
			}
		}
	}

	return ~lfsr; // invert all bits in the end
}
#endif

static void append_address(uint8_t **frameptr, aprs_addr_t *addr, uint8_t is_last)
{
	if(addr->ssid != 0) {
		int ret = sprintf((char*)*frameptr, "%s-%d", addr->call, addr->ssid);
		if(ret > 0) {
			*frameptr += ret;
		}
	} else {
		size_t len = strlen(addr->call);
		strncpy((char*)*frameptr, addr->call, len);
		*frameptr += len;
	}

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
	snprintf((char*)m_info, sizeof(m_info), "!%02i%02i.%02i%cL%03i%02i.%02i%c%c%s /A=%06i",
			lat_deg, lat_min, lat_min_fract, lat_ns,
			lon_deg, lon_min, lon_min_fract, lon_ew,
			m_icon_map[m_icon], m_comment, (int)alt_ft);
}

// PUBLIC FUNCTIONS

void aprs_init(void)
{
	m_dest.call[6] = '\0';
	m_src.call[6] = '\0';

	for(uint8_t i = 0; i < 8; i++) {
		m_path[i].call[6] = '\0';
	}

	m_npath = 0;

	m_icon = AI_X;

	m_comment[0] = '\0';
	m_comment[APRS_MAX_COMMENT_LEN] = '\0';
}

void aprs_set_dest(const char *dest, uint8_t ssid)
{
	strncpy(m_dest.call, dest, 6);
	m_dest.ssid = ssid;
}

void aprs_set_source(const char *call, uint8_t ssid)
{
	strncpy(m_src.call, call, 6);
	m_src.ssid = ssid;
}

void aprs_clear_path()
{
	m_npath = 0;
}

uint8_t aprs_add_path(const char *call, uint8_t ssid)
{
	if(m_npath == 8) {
		return 0;
	} else {
		strncpy(m_path[m_npath].call, call, 6);
		m_path[m_npath].ssid = ssid;

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

void aprs_set_icon(aprs_icon_t icon)
{
	m_icon = icon;
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

	append_address(&frameptr, &(m_src), 1);
	*(frameptr++) = '>';
	append_address(&frameptr, &(m_dest), (m_npath == 0) ? 1 : 0);

	for(uint8_t i = 0; i < m_npath; i++) {
		append_address(&frameptr, &(m_path[i]), (m_npath == (i+1)) ? 1 : 0);
	}

	*(frameptr++) = ':';

	update_info_field();

	while(*infoptr != '\0') {
		*frameptr = *infoptr;

		frameptr++;
		infoptr++;
	}

	*(frameptr++) = '\n';

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
