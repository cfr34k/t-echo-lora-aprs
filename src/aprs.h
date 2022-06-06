#ifndef APRS_H
#define APRS_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// normal AX.25 frame length, not applicable for LoRa APRS
//#define APRS_MAX_FRAME_LEN (1+7+7+8*7+1+1+256+2+1)
#define APRS_MAX_FRAME_LEN 256
#define APRS_MAX_INFO_LEN (APRS_MAX_FRAME_LEN - (1+7+7+8*7+1+1+2+1))
#define APRS_MAX_COMMENT_LEN 32

typedef enum
{
	AI_X = 0,
	AI_JOGGER,
	AI_BIKE,
	AI_CAR,
	AI_JEEP,
	AI_VAN,
	AI_TRUCK,
	AI_BUS,
	AI_BALLOON,
	AI_RECREATIONAL_VEHICLE,
	AI_HELICOPTER,
	AI_YACHT,
	AI_AMBULANCE,
	AI_FIRE_TRUCK,
	AI_SHIP,

	APRS_NUM_ICONS
} aprs_icon_t;


typedef struct {
	char source[16];
	char dest[16];
	char via[32];

	float lat; // in degrees
	float lon; // in degrees
	float alt; // in meters

	char comment[64];

	char table;
	char symbol;
} aprs_frame_t;


void aprs_init(void);
void aprs_set_dest(const char *dest);
void aprs_set_source(const char *call);
void aprs_clear_path();
uint8_t aprs_add_path(const char *call);
void aprs_update_pos_time(float lat, float lon, float alt_m, time_t t);
void aprs_set_icon(char table, char icon);
void aprs_set_icon_default(aprs_icon_t icon);
void aprs_set_comment(const char *comment);
size_t aprs_build_frame(uint8_t *frame);

bool aprs_parse_frame(const uint8_t *frame, size_t len, aprs_frame_t *result);

#endif // APRS_H
