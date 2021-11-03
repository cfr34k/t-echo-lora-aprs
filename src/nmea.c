#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <nrf_log.h>
#include <sdk_macros.h>

#include "nmea.h"

static uint8_t hexchar2num(char hex)
{
	if((hex >= '0') && (hex <= '9')) {
		return hex - '0';
	} else if((hex >= 'A') && (hex <= 'F')) {
		return hex - 'A' + 10;
	} else if((hex >= 'a') && (hex <= 'f')) {
		return hex - 'a' + 10;
	} else {
		NRF_LOG_WARNING("'%c' is not a valid hexadecimal digit.", hex);
		return 0;
	}
}

#define INVALID_COORD 1024.0f

static float nmea_coord_to_float(const char *token)
{
	char *dot = strchr(token, '.');
	if(!dot) {
		NRF_LOG_ERROR("nmea: could not find float in coordinate: '%s'", token);
		return INVALID_COORD;
	}

	size_t dotpos = dot - token;

	if((dotpos != 4) && (dotpos != 5)) {
		NRF_LOG_ERROR("nmea: wrong dot position %d in coordinate: '%s'", dotpos, token);
		return INVALID_COORD;
	}

	size_t degrees_len = dotpos - 2;

	float minutes;
	char *endptr;

	minutes = strtof(token + degrees_len, &endptr);

	if(!endptr) {
		NRF_LOG_ERROR("nmea: could not convert minute value to float: '%s'", token + degrees_len);
		return INVALID_COORD;
	}

	char degstr[4];
	strncpy(degstr, token, degrees_len);
	degstr[degrees_len] = '\0';

	long degrees = strtol(degstr, &endptr, 10);

	if(!endptr) {
		NRF_LOG_ERROR("nmea: could not convert degrees string to integer: '%s'", degstr);
		return INVALID_COORD;
	}

	return (float)degrees + minutes / 60.0f;
}

static float nmea_sign_from_char(char *polarity)
{
	char c = polarity[0];

	if((c == 'N') || (c == 'E')) {
		return 1.0f;
	} else if((c == 'S') || (c == 'W')) {
		return -1.0f;
	} else {
		NRF_LOG_ERROR("nmea: polarity char is not one of NSEW: '%s'", polarity);
		return INVALID_COORD;
	}
}

/**@brief Tokenize the given string into parts separated by given character.
 * @details
 * This works like the standard C function strtok(), but can recognize empty fields.
 */
static char* nmea_tokenize(char *input_str, char sep)
{
	static char *next_token_ptr = NULL;

	if(input_str) {
		next_token_ptr = input_str;
	}

	if(!next_token_ptr) {
		return NULL;
	}

	char *cur_token = next_token_ptr;

	char *next_sep = strchr(next_token_ptr, sep);
	if(!next_sep) {
		next_token_ptr = NULL;
	} else {
		*next_sep = '\0';
		next_token_ptr = next_sep + 1;
	}

	return cur_token;
}

static void fix_info_to_data_struct(nmea_data_t *data,
		bool auto_mode, int fix_type, float pdop, float hdop, float vdop,
		uint8_t used_sats, uint8_t sys_id)
{
	size_t empty_idx = NMEA_NUM_FIX_INFO;
	size_t found_idx = NMEA_NUM_FIX_INFO;

	// scan the existing data
	for(size_t i = 0; i < NMEA_NUM_FIX_INFO; i++) {
		uint8_t scan_sys_id = data->fix_info[i].sys_id;

		if(scan_sys_id == sys_id) {
			found_idx = i;
			break;
		}

		if(empty_idx == NMEA_NUM_FIX_INFO
				&& scan_sys_id == NMEA_SYS_ID_INVALID) {
			empty_idx = i;
		}
	}

	size_t use_idx = found_idx;

	if(use_idx == NMEA_NUM_FIX_INFO) {
		// existing entry not found, try to allocate a new one
		if(empty_idx == NMEA_NUM_FIX_INFO) {
			// no free space exists in the struct, abort
			return;
		}

		use_idx = empty_idx;

		// mark as used by this system
		data->fix_info[use_idx].sys_id = sys_id;
	}

	data->fix_info[use_idx].auto_mode = auto_mode;
	data->fix_info[use_idx].sats_used = used_sats;
	data->fix_info[use_idx].fix_type  = fix_type;

	// update generic info from this fix
	data->pdop = pdop;
	data->hdop = hdop;
	data->vdop = vdop;
}

ret_code_t nmea_parse(char *sentence, bool *position_updated, nmea_data_t *data)
{
	if(position_updated != NULL) {
		*position_updated = false;
	}

	if(sentence[0] != '$') {
		NRF_LOG_ERROR("nmea: sentence does not start with '$'");
		return NRF_ERROR_INVALID_DATA;
	}

	size_t len = strlen(sentence);

	// strip newlines and carriage-returns from the end
	char *endptr = sentence + len - 1;
	while((endptr > sentence) &&
			((*endptr == '\n') || (*endptr == '\r'))) {
		endptr--;
		len--;
	}

	sentence[len] = '\0';

	// try to find and extract the checksum, which starts at a '*'
	endptr = sentence + len - 1;
	while((endptr > sentence) && (*endptr != '*')) {
		endptr--;
	}

	if(endptr == sentence) {
		NRF_LOG_ERROR("nmea: checksum not found. Sentence incomplete? %s", NRF_LOG_PUSH(sentence));
		return NRF_ERROR_INVALID_DATA;
	} else {
		char *checksum_str = endptr + 1;

		// string ends at the asterisk before the checksum
		*endptr = '\0';

		uint8_t checksum =
			(hexchar2num(checksum_str[0]) << 4)
			+ hexchar2num(checksum_str[1]);

		uint8_t checksum_calc = 0;
		uint8_t *ptr = (uint8_t*)(sentence + 1); // skip the '$'

		while(*ptr) {
			checksum_calc ^= *ptr++;
		}

		if(checksum_calc != checksum) {
			NRF_LOG_ERROR("nmea: checksum invalid! Expected: %02x, calculated: %02x", checksum, checksum_calc);
			return NRF_ERROR_INVALID_DATA;
		}
	}

	char *token = nmea_tokenize(sentence + 1, ','); // skip the '$' in the beginning

	if(strcmp(token, "GNGLL") == 0) {
		// parse Geographic Position, Latitude / Longitude and time sentence.
		size_t info_token_idx = 0;

		float lat = INVALID_COORD, lon = INVALID_COORD;
		bool data_valid = false;

		while((token = nmea_tokenize(NULL, ','))) {
			switch(info_token_idx) {
				case 0:
					lat = nmea_coord_to_float(token);
					break;

				case 1:
					lat *= nmea_sign_from_char(token);
					break;

				case 2:
					lon = nmea_coord_to_float(token);
					break;

				case 3:
					lon *= nmea_sign_from_char(token);
					break;

				// case 4: time (not handled yet)

				case 5:
					if(token[0] == 'A') {
						data_valid = true;
					}
					break;
			}

			info_token_idx++;
		}

		if(data_valid) {
			//NRF_LOG_INFO("Got valid position: Lat: " NRF_LOG_FLOAT_MARKER ", Lon: " NRF_LOG_FLOAT_MARKER, NRF_LOG_FLOAT(lat), NRF_LOG_FLOAT(lon));

			data->lat = lat;
			data->lon = lon;
			data->pos_valid = true;
		} else {
			data->pos_valid = false;
		}

		if(position_updated != NULL) {
			*position_updated = true;
		}
	} else if(strcmp(token, "GNGSA") == 0) {
		// parse DOP and Active Satellites sentence.
		size_t info_token_idx = 0;

		bool auto_mode = false;
		int fix_type = -1;
		float pdop = 0.0f, hdop = 0.0f, vdop = 0.0f;
		uint8_t used_sats = 0;
		uint8_t sys_id = 0;

		while((token = nmea_tokenize(NULL, ','))) {
			switch(info_token_idx) {
				case 0:
					if(token[0] == 'A') {
						auto_mode = true;
					}
					break;

				case 1:
					if(token[0] >= '1' && token[0] <= '3') {
						fix_type = token[0] - '1';
					}
					break;

				case 14:
					pdop = strtof(token, NULL);
					break;

				case 15:
					hdop = strtof(token, NULL);
					break;

				case 16:
					vdop = strtof(token, NULL);
					break;

				case 17:
					sys_id = hexchar2num(token[0]);
					break;
			}

			if((info_token_idx >= 2) && (info_token_idx <= 13)) {
				if(token[0] != '\0') {
					used_sats++;
				}
			}

			info_token_idx++;
		}

		fix_info_to_data_struct(data, auto_mode, fix_type, pdop, hdop, vdop, used_sats, sys_id);
	}

	return NRF_SUCCESS;
}
