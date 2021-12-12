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

typedef struct
{
	uint8_t sys_id;
	uint8_t fix_type;
	bool    auto_mode;
	uint8_t sats_used;
} nmea_fix_info_t;

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
