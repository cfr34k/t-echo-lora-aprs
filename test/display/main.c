#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include <stdio.h>

#include "SDL_keysym.h"
#include "sdl_display.h"

#define PROGMEM
#include "fonts/Font_DIN1451Mittel_10.h"

#include "utils.h"
#include "aprs.h"
#include "nmea.h"
#include "menusystem.h"


static uint16_t m_bat_millivolt = 3456;
static uint8_t  m_bat_percent = 42;
static bool     m_lora_rx_busy = false;
static bool     m_lora_tx_busy = false;

// status info shared with other modules
bool     m_lora_rx_active = false;
bool     m_lora_tx_active = true;
bool     m_tracker_active = true;
bool     m_gps_warmup_active = true;

static nmea_data_t m_nmea_data = {
	42.345f,
	11.123f,
	100.0f,
	true,

	5.0f,
	220.0f,
	true,

	{
		{NMEA_SYS_ID_GPS, NMEA_FIX_TYPE_3D, true, 5},
		{NMEA_SYS_ID_GLONASS, NMEA_FIX_TYPE_2D, true, 3},
		{NMEA_SYS_ID_INVALID, NMEA_FIX_TYPE_2D, true, 0},
	},

	{ // Sat info GPS
		{ 9,  1},
		{ 7,  1},
		{ 5,  1},
		{ 3,  1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
	},

	{ // Sat info GLONASS
		{81,  1},
		{82,  2},
		{83, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
		{ 0, -1},
	},

	4,
	3,

	1.0f,
	2.0f,
	3.0f
};

static bool m_nmea_has_position = true;

static aprs_frame_t m_aprs_decoded_message = {
	"DL5TKL-4",
	"APZTK1",
	"WIDE1-1",
	43.21f,
	12.34f,
	100.0f,
	"Hello World!",
	'/', 'b'
};

static bool m_aprs_decode_ok = true;

static uint8_t m_display_message[256] = "Hello World!";
static uint8_t m_display_message_len = 12;

static float m_rssi = -100, m_snr = 42, m_signalRssi = -127;

static bool m_redraw_required = false;

typedef enum
{
	DISP_STATE_STARTUP,
	DISP_STATE_GPS,
	DISP_STATE_TRACKER,
	DISP_STATE_LORA_PACKET_DECODED,
	DISP_STATE_LORA_PACKET_RAW,

	DISP_STATE_END
} display_state_t;

static display_state_t m_display_state = DISP_STATE_STARTUP;


const char* nmea_fix_type_to_string(uint8_t fix_type)
{
	switch(fix_type)
	{
		case NMEA_FIX_TYPE_NONE: return "none";
		case NMEA_FIX_TYPE_2D:   return "2D";
		case NMEA_FIX_TYPE_3D:   return "3D";
		default:                 return NULL; // unknown
	}
}


const char* nmea_sys_id_to_short_name(uint8_t sys_id)
{
	switch(sys_id)
	{
		case NMEA_SYS_ID_INVALID: return "unk";
		case NMEA_SYS_ID_GPS:     return "GPS";
		case NMEA_SYS_ID_GLONASS: return "GLO";
		case NMEA_SYS_ID_GALILEO: return "GAL";
		case NMEA_SYS_ID_BEIDOU:  return "BD";
		case NMEA_SYS_ID_QZSS:    return "QZ";
		case NMEA_SYS_ID_NAVIC:   return "NAV";
		default:                  return NULL; // unknown
	}
}

static void epaper_update(bool full) {}

static uint32_t tracker_get_tx_counter(void) { return 12345; }

static void redraw_display(bool full_update)
{
	char s[32];
	char tmp1[16], tmp2[16], tmp3[16];

	uint8_t line_height = epaper_fb_get_line_height();
	uint8_t yoffset = line_height;

	// calculate GNSS satellite count
	uint8_t gps_sats_tracked = 0;
	uint8_t glonass_sats_tracked = 0;

	uint8_t gnss_total_sats_used;
	uint8_t gnss_total_sats_tracked;
	uint8_t gnss_total_sats_in_view;

	for(uint8_t i = 0; i < m_nmea_data.sat_info_count_gps; i++) {
		if(m_nmea_data.sat_info_gps[i].snr >= 0) {
			gps_sats_tracked++;
		}
	}

	for(uint8_t i = 0; i < m_nmea_data.sat_info_count_glonass; i++) {
		if(m_nmea_data.sat_info_glonass[i].snr >= 0) {
			glonass_sats_tracked++;
		}
	}

	gnss_total_sats_in_view = m_nmea_data.sat_info_count_gps + m_nmea_data.sat_info_count_glonass;
	gnss_total_sats_tracked = gps_sats_tracked + glonass_sats_tracked;

	gnss_total_sats_used = 0;
	for(uint8_t i = 0; i < NMEA_NUM_FIX_INFO; i++) {
		if(m_nmea_data.fix_info[i].sys_id != NMEA_SYS_ID_INVALID) {
			gnss_total_sats_used += m_nmea_data.fix_info[i].sats_used;
		}
	}

	epaper_fb_clear(EPAPER_COLOR_WHITE);

	// status line
	if(m_display_state != DISP_STATE_STARTUP) {
		uint8_t fill_color, line_color;
		uint8_t gwidth, gleft, gright, gbottom, gtop;

		// Satellite info box

		line_color = EPAPER_COLOR_BLACK;
		if(!(m_gps_warmup_active || m_tracker_active)) {
			line_color |= EPAPER_COLOR_FLAG_DASHED;
		}

		gleft = 0;
		gright = 98;
		gbottom = yoffset;
		gtop = yoffset - line_height;

		epaper_fb_draw_rect(gleft, gtop, gright, gbottom, line_color);

		// draw a stilized satellite

		uint8_t center_x = line_height/2;
		uint8_t center_y = line_height/2;

		// satellite: top-left wing
		epaper_fb_move_to(center_x-1, center_y-1);
		epaper_fb_line_to(center_x-2, center_y-2, EPAPER_COLOR_BLACK);
		epaper_fb_line_to(center_x-3, center_y-1, EPAPER_COLOR_BLACK);
		epaper_fb_line_to(center_x-6, center_y-4, EPAPER_COLOR_BLACK);
		epaper_fb_line_to(center_x-4, center_y-6, EPAPER_COLOR_BLACK);
		epaper_fb_line_to(center_x-1, center_y-3, EPAPER_COLOR_BLACK);
		epaper_fb_line_to(center_x-2, center_y-2, EPAPER_COLOR_BLACK);

		// satellite: bottom-right wing
		epaper_fb_move_to(center_x+1, center_y+1);
		epaper_fb_line_to(center_x+2, center_y+2, EPAPER_COLOR_BLACK);
		epaper_fb_line_to(center_x+3, center_y+1, EPAPER_COLOR_BLACK);
		epaper_fb_line_to(center_x+6, center_y+4, EPAPER_COLOR_BLACK);
		epaper_fb_line_to(center_x+4, center_y+6, EPAPER_COLOR_BLACK);
		epaper_fb_line_to(center_x+1, center_y+3, EPAPER_COLOR_BLACK);
		epaper_fb_line_to(center_x+2, center_y+2, EPAPER_COLOR_BLACK);

		// satellite: body
		epaper_fb_move_to(center_x+1, center_y-3);
		epaper_fb_line_to(center_x+3, center_y-1, EPAPER_COLOR_BLACK);
		epaper_fb_line_to(center_x-1, center_y+3, EPAPER_COLOR_BLACK);
		epaper_fb_line_to(center_x-3, center_y+1, EPAPER_COLOR_BLACK);
		epaper_fb_line_to(center_x+1, center_y-3, EPAPER_COLOR_BLACK);

		// satellite: antenna
		epaper_fb_move_to(center_x-2, center_y+2);
		epaper_fb_line_to(center_x-3, center_y+3, EPAPER_COLOR_BLACK);
		epaper_fb_move_to(center_x-5, center_y+2);
		epaper_fb_line_to(center_x-4, center_y+2, EPAPER_COLOR_BLACK);
		epaper_fb_line_to(center_x-2, center_y+4, EPAPER_COLOR_BLACK);
		epaper_fb_line_to(center_x-2, center_y+5, EPAPER_COLOR_BLACK);

		epaper_fb_move_to(gleft + 22, gbottom - 5);

		snprintf(s, sizeof(s), "%d/%d/%d",
				gnss_total_sats_used, gnss_total_sats_tracked, gnss_total_sats_in_view);

		epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

		// battery graph
		gwidth = 35;
		gleft = 160;
		gright = gleft + gwidth;
		gbottom = yoffset - 2;
		gtop = yoffset + 4 - line_height;

		epaper_fb_draw_rect(gleft, gtop, gright, gbottom, EPAPER_COLOR_BLACK);

		epaper_fb_fill_rect(
				gleft, gtop,
				gleft + (uint32_t)gwidth * (uint32_t)m_bat_percent / 100UL, gbottom,
				EPAPER_COLOR_BLACK);

		epaper_fb_fill_rect(
				gright, (gtop+gbottom)/2 - 3,
				gright + 3, (gtop+gbottom)/2 + 3,
				EPAPER_COLOR_BLACK);

		// RX status block
		if(m_lora_rx_busy) {
			fill_color = EPAPER_COLOR_BLACK;
			line_color = EPAPER_COLOR_WHITE;
		} else {
			fill_color = EPAPER_COLOR_WHITE;
			line_color = EPAPER_COLOR_BLACK;
		}

		if(!m_lora_rx_active) {
			line_color |= EPAPER_COLOR_FLAG_DASHED;
		}

		gleft = 130;
		gright = 158;
		gbottom = yoffset;
		gtop = yoffset - line_height;

		epaper_fb_fill_rect(gleft, gtop, gright, gbottom, fill_color);
		epaper_fb_draw_rect(gleft, gtop, gright, gbottom, line_color);

		epaper_fb_move_to(gleft + 2, gbottom - 5);
		epaper_fb_draw_string("RX", line_color);

		// TX status block
		if(m_lora_tx_busy) {
			fill_color = EPAPER_COLOR_BLACK;
			line_color = EPAPER_COLOR_WHITE;
		} else {
			fill_color = EPAPER_COLOR_WHITE;
			line_color = EPAPER_COLOR_BLACK;
		}

		if(!m_tracker_active) {
			line_color |= EPAPER_COLOR_FLAG_DASHED;
		}

		gleft = 100;
		gright = 128;
		gbottom = yoffset;
		gtop = yoffset - line_height;

		epaper_fb_fill_rect(gleft, gtop, gright, gbottom, fill_color);
		epaper_fb_draw_rect(gleft, gtop, gright, gbottom, line_color);

		epaper_fb_move_to(gleft + 2, gbottom - 5);
		epaper_fb_draw_string("TX", line_color);

		epaper_fb_move_to(0, yoffset + 2);
		epaper_fb_line_to(EPAPER_WIDTH, yoffset + 2, EPAPER_COLOR_BLACK | EPAPER_COLOR_FLAG_DASHED);

		yoffset += line_height + 3;
	}

	// menusystem overrides everything while it is active.
	if(menusystem_is_active()) {
		menusystem_render(yoffset);
	} else {
		epaper_fb_move_to(0, yoffset);

		switch(m_display_state)
		{
			case DISP_STATE_STARTUP:
				// bicycle frame
				epaper_fb_move_to( 65, 114);
				epaper_fb_line_to( 96, 114, EPAPER_COLOR_BLACK);
				epaper_fb_line_to(127,  88, EPAPER_COLOR_BLACK);
				epaper_fb_line_to(125,  84, EPAPER_COLOR_BLACK);
				epaper_fb_line_to( 81,  84, EPAPER_COLOR_BLACK);
				epaper_fb_line_to( 65, 114, EPAPER_COLOR_BLACK);

				epaper_fb_move_to( 79,  88);
				epaper_fb_line_to( 55,  88, EPAPER_COLOR_BLACK);
				epaper_fb_line_to( 65, 114, EPAPER_COLOR_BLACK);

				// seat post
				epaper_fb_move_to( 96, 114);
				epaper_fb_line_to( 80,  76, EPAPER_COLOR_BLACK);

				// seat
				epaper_fb_move_to( 72,  73);
				epaper_fb_line_to( 90,  73, EPAPER_COLOR_BLACK);
				epaper_fb_move_to( 74,  74);
				epaper_fb_line_to( 87,  74, EPAPER_COLOR_BLACK);
				epaper_fb_move_to( 77,  75);
				epaper_fb_line_to( 82,  75, EPAPER_COLOR_BLACK);

				// handlebar
				epaper_fb_move_to(117,  72);
				epaper_fb_line_to(130,  72, EPAPER_COLOR_BLACK);
				epaper_fb_move_to(128,  72);
				epaper_fb_line_to(124,  78, EPAPER_COLOR_BLACK);
				epaper_fb_line_to(137, 114, EPAPER_COLOR_BLACK);

				// front wheel
				epaper_fb_circle(20, EPAPER_COLOR_BLACK);

				// rear wheel
				epaper_fb_move_to( 65, 114);
				epaper_fb_circle(20, EPAPER_COLOR_BLACK);

				// Antenna mast
				epaper_fb_move_to( 55,  88);
				epaper_fb_line_to( 55,  38, EPAPER_COLOR_BLACK);
				epaper_fb_move_to( 50,  38);
				epaper_fb_line_to( 55,  43, EPAPER_COLOR_BLACK);
				epaper_fb_line_to( 60,  38, EPAPER_COLOR_BLACK);

				// waves
				epaper_fb_move_to( 55,  38);
				epaper_fb_circle(10, EPAPER_COLOR_BLACK | EPAPER_COLOR_FLAG_DASHED);
				epaper_fb_circle(20, EPAPER_COLOR_BLACK | EPAPER_COLOR_FLAG_DASHED);
				epaper_fb_circle(30, EPAPER_COLOR_BLACK | EPAPER_COLOR_FLAG_DASHED);

				epaper_fb_set_font(&din1451m10pt7b);
				epaper_fb_move_to(0, 170);
				epaper_fb_draw_string("Lora-APRS by DL5TKL", EPAPER_COLOR_BLACK);
				epaper_fb_move_to(0, 190);
				epaper_fb_draw_string(VERSION, EPAPER_COLOR_BLACK);
				break;

			case DISP_STATE_GPS:
				epaper_fb_draw_string("GNSS-Status:", EPAPER_COLOR_BLACK);

				yoffset += line_height;
				epaper_fb_move_to(0, yoffset);

				if(m_nmea_data.pos_valid) {
					format_float(tmp1, sizeof(tmp1), m_nmea_data.lat, 6);
					snprintf(s, sizeof(s), "Lat: %s", tmp1);

					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					epaper_fb_move_to(150, yoffset);
					epaper_fb_draw_string("Alt:", EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					format_float(tmp1, sizeof(tmp1), m_nmea_data.lon, 6);
					snprintf(s, sizeof(s), "Lon: %s", tmp1);

					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					epaper_fb_move_to(150, yoffset);
					snprintf(s, sizeof(s), "%d", (int)(m_nmea_data.altitude + 0.5f));
					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);
				} else {
					epaper_fb_draw_string("No fix :-(", EPAPER_COLOR_BLACK);
				}

				yoffset += line_height + line_height/2;
				epaper_fb_move_to(0, yoffset);

				for(uint8_t i = 0; i < NMEA_NUM_FIX_INFO; i++) {
					nmea_fix_info_t *fix_info = &(m_nmea_data.fix_info[i]);

					if(fix_info->sys_id == NMEA_SYS_ID_INVALID) {
						continue;
					}

					snprintf(s, sizeof(s), "%s: %s [%s] Sats: %d",
							nmea_sys_id_to_short_name(fix_info->sys_id),
							nmea_fix_type_to_string(fix_info->fix_type),
							fix_info->auto_mode ? "auto" : "man",
							fix_info->sats_used);

					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);
				}

				format_float(tmp1, sizeof(tmp1), m_nmea_data.hdop, 1);
				format_float(tmp2, sizeof(tmp2), m_nmea_data.vdop, 1);
				format_float(tmp3, sizeof(tmp3), m_nmea_data.pdop, 1);

				snprintf(s, sizeof(s), "DOP H: %s V: %s P: %s",
						tmp1, tmp2, tmp3);

				epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

				yoffset += line_height;
				epaper_fb_move_to(0, yoffset);

				snprintf(s, sizeof(s), "Trk: GP: %d/%d, GL: %d/%d",
						gps_sats_tracked, m_nmea_data.sat_info_count_gps,
						glonass_sats_tracked, m_nmea_data.sat_info_count_glonass);

				epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

				yoffset += line_height;
				epaper_fb_move_to(0, yoffset);
				break;

			case DISP_STATE_TRACKER:
				if(!aprs_can_build_frame()) {
					epaper_fb_draw_string("Tracker blocked.", EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					strncpy(s, "Check settings.", sizeof(s));
				} else {
					snprintf(s, sizeof(s), "Tracker %s.",
							m_tracker_active ? "running" : "stopped");
				}

				epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

				yoffset += line_height + line_height / 2;
				epaper_fb_move_to(0, yoffset);

				if(m_nmea_data.pos_valid) {
					format_float(tmp1, sizeof(tmp1), m_nmea_data.hdop, 1);
					format_float(tmp2, sizeof(tmp2), m_nmea_data.vdop, 1);
					format_float(tmp3, sizeof(tmp3), m_nmea_data.pdop, 1);

					snprintf(s, sizeof(s), "DOP H: %s V: %s P: %s",
							tmp1, tmp2, tmp3);

					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					format_float(tmp1, sizeof(tmp1), m_nmea_data.lat, 5);
					format_float(tmp2, sizeof(tmp2), m_nmea_data.lon, 5);

					snprintf(s, sizeof(s), "%s/%s, %d m",
							tmp1, tmp2, (int)(m_nmea_data.altitude + 0.5));

					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);
				} else {
					epaper_fb_draw_string("No fix :-(", EPAPER_COLOR_BLACK);
				}

				yoffset += line_height;
				epaper_fb_move_to(0, yoffset);

				if(m_nmea_data.speed_heading_valid) {
					format_float(tmp1, sizeof(tmp1), m_nmea_data.speed, 1);

					snprintf(s, sizeof(s), "%s m/s - %d deg",
							tmp1, (int)(m_nmea_data.heading + 0.5));

					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);
				} else {
					epaper_fb_draw_string("No speed / heading info.", EPAPER_COLOR_BLACK);
				}

				yoffset += line_height;
				epaper_fb_move_to(0, yoffset);

				snprintf(s, sizeof(s), "TX count: %lu", tracker_get_tx_counter());

				epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

				yoffset += line_height;
				epaper_fb_move_to(0, yoffset);
				break;

			case DISP_STATE_LORA_PACKET_DECODED:
				if(m_aprs_decode_ok) {
					epaper_fb_draw_string(m_aprs_decoded_message.source, EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					format_float(tmp1, sizeof(tmp1), m_aprs_decoded_message.lat, 5);
					snprintf(s, sizeof(s), "Lat: %s", tmp1);
					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					format_float(tmp1, sizeof(tmp1), m_aprs_decoded_message.lon, 5);
					snprintf(s, sizeof(s), "Lon: %s", tmp1);
					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					format_float(tmp1, sizeof(tmp1), m_aprs_decoded_message.alt, 1);
					snprintf(s, sizeof(s), "Alt: %s m", tmp1);
					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					strncpy(s, m_aprs_decoded_message.comment, sizeof(s));
					if(strlen(s) > 20) {
						s[18] = '\0';
						strcat(s, "...");
					}
					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					if(m_nmea_has_position) {
						float distance = great_circle_distance_m(
								m_nmea_data.lat, m_nmea_data.lon,
								m_aprs_decoded_message.lat, m_aprs_decoded_message.lon);

						float direction = direction_angle(
								m_nmea_data.lat, m_nmea_data.lon,
								m_aprs_decoded_message.lat, m_aprs_decoded_message.lon);

						yoffset += line_height/8; // quarter-line extra spacing
						epaper_fb_move_to(0, yoffset);

						format_float(tmp1, sizeof(tmp1), distance / 1000.0f, 2);
						snprintf(s, sizeof(s), "Distance: %s km", tmp1);
						epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

						yoffset += line_height;
						epaper_fb_move_to(0, yoffset);

						format_float(tmp1, sizeof(tmp1), direction, 1);
						snprintf(s, sizeof(s), "Direction: %s deg", tmp1);
						epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

						yoffset += line_height;

						static const uint8_t r = 30;
						uint8_t center_x = EPAPER_WIDTH - r - 5;
						uint8_t center_y = line_height*2 + r;

						uint8_t arrow_start_x = center_x;
						uint8_t arrow_start_y = center_y;

						uint8_t arrow_end_x = center_x + r * sinf(direction * (3.14f / 180.0f));
						uint8_t arrow_end_y = center_y - r * cosf(direction * (3.14f / 180.0f));

						epaper_fb_move_to(center_x, center_y);
						epaper_fb_circle(r, EPAPER_COLOR_BLACK);
						epaper_fb_circle(2, EPAPER_COLOR_BLACK);

						epaper_fb_move_to(arrow_start_x, arrow_start_y);
						epaper_fb_line_to(arrow_end_x, arrow_end_y, EPAPER_COLOR_BLACK);

						epaper_fb_move_to(center_x - 5, center_y - r + line_height/3);
						epaper_fb_draw_string("N", EPAPER_COLOR_BLACK);
					}
				} else { /* show error message */
					epaper_fb_draw_string("Decoder Error:", EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					epaper_fb_draw_string_wrapped(aprs_get_parser_error(), EPAPER_COLOR_BLACK);
				}
				break;

			case DISP_STATE_LORA_PACKET_RAW:
				epaper_fb_draw_data_wrapped(m_display_message, m_display_message_len, EPAPER_COLOR_BLACK);

				yoffset = epaper_fb_get_cursor_pos_y() + 3 * line_height / 2;
				epaper_fb_move_to(0, yoffset);

				snprintf(s, sizeof(s), "R: -%d.%01d / %d.%02d / -%d.%01d",
						(int)(-m_rssi),
						(int)(10 * ((-m_rssi) - (int)(-m_rssi))),
						(int)(m_snr),
						(int)(100 * ((m_snr) - (int)(m_snr))),
						(int)(-m_signalRssi),
						(int)(10 * ((-m_signalRssi) - (int)(-m_signalRssi))));

				epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);
				yoffset += line_height;
				epaper_fb_move_to(0, yoffset);
				break;
		}
	}

	epaper_update(full_update);
}


void cb_menusystem(menusystem_evt_t evt, const menusystem_evt_data_t *data)
{
	switch(evt) {
		case MENUSYSTEM_EVT_EXIT_MENU:
			printf("Menu exit.\n");
			break;

		case MENUSYSTEM_EVT_RX_ENABLE:
			m_lora_rx_active = true;
			break;

		case MENUSYSTEM_EVT_RX_DISABLE:
			m_lora_rx_active = false;
			break;

		case MENUSYSTEM_EVT_TRACKER_ENABLE:
			m_tracker_active = true;
			break;

		case MENUSYSTEM_EVT_TRACKER_DISABLE:
			m_tracker_active = false;
			break;

		case MENUSYSTEM_EVT_GNSS_WARMUP_ENABLE:
			// TODO
			break;

		case MENUSYSTEM_EVT_GNSS_WARMUP_DISABLE:
			m_lora_tx_active = true;
			break;

		case MENUSYSTEM_EVT_APRS_SYMBOL_CHANGED:
			printf("New APRS symbol: table = %c, symbol = %c\n", data->aprs_symbol.table, data->aprs_symbol.symbol);
			break;

		default:
			break;
	}

	m_redraw_required = true;
}


int main(int argc, char **argv) {
	SDL_Surface *screen;

	SDL_Event    event;

	bool running = true;

	aprs_set_icon('/', 'b');
	aprs_set_source("DL5TKL-4");
	aprs_set_dest("APZTK1");
	menusystem_init(cb_menusystem);

	screen = init_sdl();

	epaper_fb_set_font(&din1451m10pt7b);

	while(running && SDL_WaitEvent(&event)) {
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
			running = 0;
		} else if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN) {
			menusystem_enter();
			m_redraw_required = true;
		} else if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHT) {
			if(menusystem_is_active()) {
				menusystem_input(MENUSYSTEM_INPUT_CONFIRM);
			} else {
				m_display_state++;
				m_display_state %= DISP_STATE_END;
				m_redraw_required = true;
			}
		} else if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_DOWN) {
			if(menusystem_is_active()) {
				menusystem_input(MENUSYSTEM_INPUT_NEXT);
			}
		}

		if(m_redraw_required) {
			m_redraw_required = false;
			redraw_display(true);
			SDL_UpdateRect(screen, 0, 0, 0, 0);
		}

	}

	SDL_Quit();

	return 0;
}
