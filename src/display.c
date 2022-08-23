#include <stdio.h>
#include <string.h>
#include <math.h>

#include "aprs.h"
#include "menusystem.h"
#include "nmea.h"
#include "tracker.h"
#include "utils.h"

#include "epaper.h"

#include "display.h"

#define PROGMEM
#include "fonts/Font_DIN1451Mittel_10.h"

// all "extern" variables come from main.c

extern nmea_data_t m_nmea_data;
extern bool m_nmea_has_position;

extern bool m_lora_rx_active;
extern bool m_tracker_active;
extern bool m_gps_warmup_active;

extern display_state_t m_display_state;

extern bool m_lora_rx_busy;
extern bool m_lora_tx_busy;

extern uint8_t  m_bat_percent;
extern uint16_t m_bat_millivolt;

extern aprs_frame_t m_aprs_decoded_message;
extern bool         m_aprs_decode_ok;

extern uint8_t m_display_message[256];
extern uint8_t m_display_message_len;

extern float m_rssi, m_snr, m_signalRssi;

/**@brief Redraw the e-Paper display.
 */
void redraw_display(bool full_update)
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

		bool gps_active = (m_gps_warmup_active || m_tracker_active);

		// Satellite info box

		if(m_nmea_data.pos_valid && gps_active) {
			fill_color = EPAPER_COLOR_BLACK;
			line_color = EPAPER_COLOR_WHITE;
		} else {
			fill_color = EPAPER_COLOR_WHITE;
			line_color = EPAPER_COLOR_BLACK;
		}

		if(!gps_active) {
			line_color |= EPAPER_COLOR_FLAG_DASHED;
		}

		gleft = 0;
		gright = 98;
		gbottom = yoffset;
		gtop = yoffset - line_height;

		epaper_fb_fill_rect(gleft, gtop, gright, gbottom, fill_color);
		epaper_fb_draw_rect(gleft, gtop, gright, gbottom, line_color);

		// draw a stilized satellite

		line_color &= (~EPAPER_COLOR_FLAG_DASHED);

		uint8_t center_x = line_height/2;
		uint8_t center_y = line_height/2;

		// satellite: top-left wing
		epaper_fb_move_to(center_x-1, center_y-1);
		epaper_fb_line_to(center_x-2, center_y-2, line_color);
		epaper_fb_line_to(center_x-3, center_y-1, line_color);
		epaper_fb_line_to(center_x-6, center_y-4, line_color);
		epaper_fb_line_to(center_x-4, center_y-6, line_color);
		epaper_fb_line_to(center_x-1, center_y-3, line_color);
		epaper_fb_line_to(center_x-2, center_y-2, line_color);

		// satellite: bottom-right wing
		epaper_fb_move_to(center_x+1, center_y+1);
		epaper_fb_line_to(center_x+2, center_y+2, line_color);
		epaper_fb_line_to(center_x+3, center_y+1, line_color);
		epaper_fb_line_to(center_x+6, center_y+4, line_color);
		epaper_fb_line_to(center_x+4, center_y+6, line_color);
		epaper_fb_line_to(center_x+1, center_y+3, line_color);
		epaper_fb_line_to(center_x+2, center_y+2, line_color);

		// satellite: body
		epaper_fb_move_to(center_x+1, center_y-3);
		epaper_fb_line_to(center_x+3, center_y-1, line_color);
		epaper_fb_line_to(center_x-1, center_y+3, line_color);
		epaper_fb_line_to(center_x-3, center_y+1, line_color);
		epaper_fb_line_to(center_x+1, center_y-3, line_color);

		// satellite: antenna
		epaper_fb_move_to(center_x-2, center_y+2);
		epaper_fb_line_to(center_x-3, center_y+3, line_color);
		epaper_fb_move_to(center_x-5, center_y+2);
		epaper_fb_line_to(center_x-4, center_y+2, line_color);
		epaper_fb_line_to(center_x-2, center_y+4, line_color);
		epaper_fb_line_to(center_x-2, center_y+5, line_color);

		epaper_fb_move_to(gleft + 22, gbottom - 5);

		snprintf(s, sizeof(s), "%d/%d/%d",
				gnss_total_sats_used, gnss_total_sats_tracked, gnss_total_sats_in_view);

		epaper_fb_draw_string(s, line_color);

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

					format_float(tmp1, sizeof(tmp1), m_aprs_decoded_message.lat, 6);
					snprintf(s, sizeof(s), "Lat: %s", tmp1);
					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					format_float(tmp1, sizeof(tmp1), m_aprs_decoded_message.lon, 6);
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

						format_float(tmp1, sizeof(tmp1), distance / 1000.0f, 3);
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

						// draw arrow of own heading for comparison (dashed)
						if(m_nmea_data.speed_heading_valid) {
							uint8_t arrow_end_x = center_x + r * sinf(m_nmea_data.heading * (3.14f / 180.0f));
							uint8_t arrow_end_y = center_y - r * cosf(m_nmea_data.heading * (3.14f / 180.0f));

							epaper_fb_move_to(arrow_start_x, arrow_start_y);
							epaper_fb_line_to(arrow_end_x, arrow_end_y,
									EPAPER_COLOR_BLACK | EPAPER_COLOR_FLAG_DASHED);
						}

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

				epaper_fb_draw_string("R: ", EPAPER_COLOR_BLACK);

				format_float(tmp1, sizeof(tmp1), m_rssi, 1);
				epaper_fb_draw_string(tmp1, EPAPER_COLOR_BLACK);
				epaper_fb_draw_string(" / ", EPAPER_COLOR_BLACK);

				format_float(tmp1, sizeof(tmp1), m_snr, 2);
				epaper_fb_draw_string(tmp1, EPAPER_COLOR_BLACK);
				epaper_fb_draw_string(" / ", EPAPER_COLOR_BLACK);

				format_float(tmp1, sizeof(tmp1), m_signalRssi, 1);
				epaper_fb_draw_string(tmp1, EPAPER_COLOR_BLACK);

				yoffset += line_height;
				epaper_fb_move_to(0, yoffset);
				break;

			case DISP_STATE_END:
				// this state should never be reached.
				epaper_fb_draw_string("BUG! Please report!", EPAPER_COLOR_BLACK);
				break;
		}
	}

	epaper_update(full_update);
}


