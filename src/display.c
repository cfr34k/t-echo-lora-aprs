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

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "aprs.h"
#include "menusystem.h"
#include "nmea.h"
#include "tracker.h"
#include "utils.h"
#include "wall_clock.h"

#include "epaper.h"

#include "display.h"

#define PROGMEM
#include "fonts/Font_DIN1451Mittel_10.h"

// all "extern" variables come from main.c

extern nmea_data_t m_nmea_data;
extern bool m_nmea_has_position;

extern bool m_lora_rx_active;
extern bool m_tracker_active;
extern bool m_gnss_keep_active;

extern display_state_t m_display_state;

extern bool m_lora_rx_busy;
extern bool m_lora_tx_busy;

extern uint8_t  m_bat_percent;
extern uint16_t m_bat_millivolt;

extern aprs_rx_raw_data_t m_last_undecodable_data;
extern uint64_t m_last_undecodable_timestamp;

extern uint8_t m_display_rx_index;

static int format_timedelta(char *buf, size_t buf_len, uint32_t timedelta)
{
	if(timedelta < 60) {
		return snprintf(buf, buf_len, "%lus", timedelta);
	} else if(timedelta < 360*60) {
		return snprintf(buf, buf_len, "%lum", timedelta/60);
	} else if(timedelta < 72*3600) {
		return snprintf(buf, buf_len, "%luh", timedelta/3600);
	} else {
		return snprintf(buf, buf_len, "%lud", timedelta/86400);
	}
}

/**@brief Redraw the e-Paper display.
 */
void redraw_display(bool full_update)
{
	char s[64];
	char tmp1[16], tmp2[16], tmp3[16];

	const aprs_rx_history_t *aprs_history = aprs_get_rx_history();

	uint8_t line_height = epaper_fb_get_line_height();
	uint8_t yoffset = line_height;

	uint64_t unix_now = wall_clock_get_unix();

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

		bool gps_active = (m_gnss_keep_active || m_tracker_active);

		// Satellite info box

		if(m_nmea_data.pos_valid && gps_active) {
			fill_color = EPAPER_COLOR_BLACK;
			line_color = EPAPER_COLOR_WHITE;
		} else {
			fill_color = EPAPER_COLOR_WHITE;
			line_color = EPAPER_COLOR_BLACK;
		}

		if(!gps_active) {
			line_color |= EPAPER_LINE_DRAWING_MODE_DASHED;
		}

		gleft = 0;
		gright = 98;
		gbottom = yoffset;
		gtop = yoffset - line_height;

		epaper_fb_fill_rect(gleft, gtop, gright, gbottom, fill_color);
		epaper_fb_draw_rect(gleft, gtop, gright, gbottom, line_color);

		// draw a stilized satellite

		line_color &= (~EPAPER_LINE_DRAWING_MODE_DASHED);

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
			line_color |= EPAPER_LINE_DRAWING_MODE_DASHED;
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
			line_color |= EPAPER_LINE_DRAWING_MODE_DASHED;
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
		epaper_fb_line_to(EPAPER_WIDTH, yoffset + 2, EPAPER_COLOR_BLACK | EPAPER_LINE_DRAWING_MODE_DASHED);

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
				epaper_fb_circle(10, EPAPER_COLOR_BLACK | EPAPER_LINE_DRAWING_MODE_DASHED);
				epaper_fb_circle(20, EPAPER_COLOR_BLACK | EPAPER_LINE_DRAWING_MODE_DASHED);
				epaper_fb_circle(30, EPAPER_COLOR_BLACK | EPAPER_LINE_DRAWING_MODE_DASHED);

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

					epaper_fb_draw_string("Source call not set!", EPAPER_COLOR_BLACK);
					break;
				} else {
					snprintf(s, sizeof(s), "Tracker %s.",
							m_tracker_active ? "running" : "stopped");
				}

				epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

				yoffset += 5 * line_height/4;
				epaper_fb_move_to(0, yoffset);

				uint8_t altitude_yoffset = yoffset;

				if(m_nmea_data.pos_valid) {
					format_float(tmp1, sizeof(tmp1), m_nmea_data.lat, 6);
					snprintf(s, sizeof(s), "Lat: %s", tmp1);
					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					format_float(tmp1, sizeof(tmp1), m_nmea_data.lon, 6);
					snprintf(s, sizeof(s), "Lon: %s", tmp1);
					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					format_float(tmp1, sizeof(tmp1), m_nmea_data.altitude, 1);
					snprintf(s, sizeof(s), "Alt: %s m", tmp1);
					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					altitude_yoffset = yoffset;
				} else {
					epaper_fb_draw_string("No fix :-(", EPAPER_COLOR_BLACK);
				}

				yoffset += line_height * 5 / 4;
				epaper_fb_move_to(0, yoffset);

				snprintf(s, sizeof(s), "TX count: %lu", tracker_get_tx_counter());

				epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

				yoffset += line_height * 5 / 4;
				epaper_fb_move_to(0, yoffset);

				if(m_nmea_data.speed_heading_valid) {
					float speed_kmph = m_nmea_data.speed * 3.6f;

					format_float(tmp1, sizeof(tmp1), speed_kmph, 1);
					snprintf(s, sizeof(s), "%s km/h", tmp1);

					epaper_fb_move_to(EPAPER_WIDTH - epaper_fb_calc_text_width(s), altitude_yoffset);
					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					static const uint8_t r = 30;
					uint8_t center_x = EPAPER_WIDTH - r - 5;
					uint8_t center_y = line_height*2 + r - 5;

					epaper_fb_move_to(center_x, center_y);
					epaper_fb_circle(r, EPAPER_COLOR_BLACK);
					epaper_fb_circle(2, EPAPER_COLOR_BLACK);

					uint8_t arrow_start_x = center_x;
					uint8_t arrow_start_y = center_y;

					uint8_t arrow_end_x = center_x + r * sinf(m_nmea_data.heading * (3.14f / 180.0f));
					uint8_t arrow_end_y = center_y - r * cosf(m_nmea_data.heading * (3.14f / 180.0f));

					epaper_fb_move_to(arrow_start_x, arrow_start_y);
					epaper_fb_line_to(arrow_end_x, arrow_end_y, EPAPER_COLOR_BLACK);

					epaper_fb_move_to(center_x - 5, center_y - r + line_height/3);
					epaper_fb_draw_string("N", EPAPER_COLOR_BLACK);
				} else {
					epaper_fb_draw_string("No speed / heading info.", EPAPER_COLOR_BLACK);
				}
				break;

			case DISP_STATE_LORA_RX_OVERVIEW:
				yoffset -= line_height;

				for(uint8_t i = 0; i < APRS_RX_HISTORY_SIZE+1; i++) {
					yoffset += 2*line_height;

					uint8_t fg_color, bg_color;

					if(i == m_display_rx_index) {
						fg_color = EPAPER_COLOR_WHITE;
						bg_color = EPAPER_COLOR_BLACK;
					} else {
						fg_color = EPAPER_COLOR_BLACK;
						bg_color = EPAPER_COLOR_WHITE;
					}

					epaper_fb_fill_rect(0, yoffset - 2*line_height, EPAPER_WIDTH, yoffset, bg_color);

#define HISTORY_TEXT_BASE_OFFSET 6

					if(i < APRS_RX_HISTORY_SIZE) {
						// decoded entries
						const aprs_rx_history_entry_t *entry = &aprs_history->history[i];

						// skip entries that have reception time 0, i.e. are not set.
						if(entry->rx_timestamp == 0) {
							continue;
						}

						// source call
						epaper_fb_move_to(0, yoffset - line_height - HISTORY_TEXT_BASE_OFFSET);
						epaper_fb_draw_string(entry->decoded.source, fg_color);

						// time since reception
						uint32_t timedelta = unix_now - entry->rx_timestamp;

						format_timedelta(s, sizeof(s), timedelta);

						epaper_fb_move_to(0, yoffset - HISTORY_TEXT_BASE_OFFSET);
						epaper_fb_draw_string(s, fg_color);

						// calculate distance and course if we know our own position
						if(m_nmea_has_position) {
							float distance = great_circle_distance_m(
									m_nmea_data.lat, m_nmea_data.lon,
									entry->decoded.lat, entry->decoded.lon);

							float direction = direction_angle(
									m_nmea_data.lat, m_nmea_data.lon,
									entry->decoded.lat, entry->decoded.lon);

							if(distance < 1000.0f) {
								snprintf(s, sizeof(s), "%dm", (int)(distance + 0.5f));
							} else {
								format_float(s, sizeof(s), distance * 1e-3f, 1);
								strcat(s, "km");
							}

							epaper_fb_move_to(60, yoffset - HISTORY_TEXT_BASE_OFFSET);
							epaper_fb_draw_string(s, fg_color);

							// draw a nice arrow for the course

							uint8_t center_x = EPAPER_WIDTH - 3*line_height/2;
							uint8_t center_y = yoffset - line_height;

							// precalculate rotation arguments
							float rot_cos = cosf(direction * 3.14159f / 180.0f);
							float rot_sin = sinf(direction * 3.14159f / 180.0f);

							// start point at bottom
							float point_x = 0.0f;
							float point_y = (line_height-2);

							float rpoint_x = point_x * rot_cos - point_y * rot_sin;
							float rpoint_y = point_x * rot_sin + point_y * rot_cos;

							epaper_fb_move_to(
									center_x + (int8_t)(rpoint_x + 0.5f),
									center_y + (int8_t)(rpoint_y + 0.5f));

							// tip at top
							point_x = 0.0f;
							point_y = -(line_height-2);

							float tip_x = point_x * rot_cos - point_y * rot_sin;
							float tip_y = point_x * rot_sin + point_y * rot_cos;

							epaper_fb_line_to(
									center_x + (int8_t)(tip_x + 0.5f),
									center_y + (int8_t)(tip_y + 0.5f),
									fg_color);

							// line to the left of the tip
							point_x = -6.0f;
							point_y = -(line_height-2) + 6.0f;

							rpoint_x = point_x * rot_cos - point_y * rot_sin;
							rpoint_y = point_x * rot_sin + point_y * rot_cos;

							epaper_fb_line_to(
									center_x + (int8_t)(rpoint_x + 0.5f),
									center_y + (int8_t)(rpoint_y + 0.5f),
									fg_color);

							// line to the right of the tip
							point_x = 6.0f;
							point_y = -(line_height-2) + 6.0f;

							rpoint_x = point_x * rot_cos - point_y * rot_sin;
							rpoint_y = point_x * rot_sin + point_y * rot_cos;

							// first move to the tip again
							epaper_fb_move_to(
									center_x + (int8_t)(tip_x + 0.5f),
									center_y + (int8_t)(tip_y + 0.5f));

							epaper_fb_line_to(
									center_x + (int8_t)(rpoint_x + 0.5f),
									center_y + (int8_t)(rpoint_y + 0.5f),
									fg_color);
						}

					} else {
						// failed packet time
						epaper_fb_move_to(0, yoffset - line_height - HISTORY_TEXT_BASE_OFFSET);

						if(m_last_undecodable_timestamp > 0) {
							uint32_t timedelta = unix_now - m_last_undecodable_timestamp;

							format_timedelta(tmp1, sizeof(tmp1), timedelta);

							snprintf(s, sizeof(s), "Last error: %s ago", tmp1);
							epaper_fb_draw_string(s, fg_color);
						} else {
							epaper_fb_draw_string("Last error: never", fg_color);
						}
					}
				}
				break;

			case DISP_STATE_LORA_PACKET_DETAIL:
				if(m_display_rx_index < APRS_RX_HISTORY_SIZE) {
					const aprs_rx_history_entry_t *entry = &aprs_history->history[m_display_rx_index];

					epaper_fb_draw_string(entry->decoded.source, EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					format_float(tmp1, sizeof(tmp1), entry->decoded.lat, 6);
					snprintf(s, sizeof(s), "Lat: %s", tmp1);
					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					format_float(tmp1, sizeof(tmp1), entry->decoded.lon, 6);
					snprintf(s, sizeof(s), "Lon: %s", tmp1);
					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					format_float(tmp1, sizeof(tmp1), entry->decoded.alt, 1);
					snprintf(s, sizeof(s), "Alt: %s m", tmp1);
					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					uint8_t altitude_yoffset = yoffset; // store it for later use

					yoffset += 5 * line_height / 4;
					epaper_fb_move_to(0, yoffset);

					strncpy(s, entry->decoded.comment, sizeof(s));
					if(strlen(s) > 40) {
						s[38] = '\0';
						strcat(s, "...");
					}
					epaper_fb_draw_string_wrapped(s, EPAPER_COLOR_BLACK);

					yoffset = epaper_fb_get_cursor_pos_y();

					if(m_nmea_has_position) {
						float distance = great_circle_distance_m(
								m_nmea_data.lat, m_nmea_data.lon,
								entry->decoded.lat, entry->decoded.lon);

						float direction = direction_angle(
								m_nmea_data.lat, m_nmea_data.lon,
								entry->decoded.lat, entry->decoded.lon);

						format_float(tmp1, sizeof(tmp1), distance / 1000.0f, 3);
						snprintf(s, sizeof(s), "%s km", tmp1);

						uint8_t text_width = epaper_fb_calc_text_width(s);

						epaper_fb_move_to(EPAPER_WIDTH - text_width, altitude_yoffset);
						epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

						static const uint8_t r = 30;
						uint8_t center_x = EPAPER_WIDTH - r - 5;
						uint8_t center_y = line_height*2 + r - 5;

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
									EPAPER_COLOR_BLACK | EPAPER_LINE_DRAWING_MODE_DOTTED);
						}

						epaper_fb_move_to(center_x - 5, center_y - r + line_height/3);
						epaper_fb_draw_string("N", EPAPER_COLOR_BLACK);
					}

					yoffset += 5 * line_height / 4;
					epaper_fb_move_to(0, yoffset);

					epaper_fb_draw_string("R: ", EPAPER_COLOR_BLACK);

					format_float(tmp1, sizeof(tmp1), entry->raw.rssi, 1);
					epaper_fb_draw_string(tmp1, EPAPER_COLOR_BLACK);
					epaper_fb_draw_string(" / ", EPAPER_COLOR_BLACK);

					format_float(tmp1, sizeof(tmp1), entry->raw.snr, 2);
					epaper_fb_draw_string(tmp1, EPAPER_COLOR_BLACK);
					epaper_fb_draw_string(" / ", EPAPER_COLOR_BLACK);

					format_float(tmp1, sizeof(tmp1), entry->raw.signalRssi, 1);
					epaper_fb_draw_string(tmp1, EPAPER_COLOR_BLACK);
				} else {
					/* show error message */
					epaper_fb_draw_string("Decoder Error:", EPAPER_COLOR_BLACK);

					yoffset += line_height;
					epaper_fb_move_to(0, yoffset);

					epaper_fb_draw_string_wrapped(aprs_get_parser_error(), EPAPER_COLOR_BLACK);

					yoffset = epaper_fb_get_cursor_pos_y() + line_height * 5 / 4;
					epaper_fb_move_to(0, yoffset);

					/* ... and raw message */
					epaper_fb_draw_data_wrapped(m_last_undecodable_data.data, m_last_undecodable_data.data_len, EPAPER_COLOR_BLACK);

					yoffset = epaper_fb_get_cursor_pos_y() + 5 * line_height / 4;
					epaper_fb_move_to(0, yoffset);

					epaper_fb_draw_string("R: ", EPAPER_COLOR_BLACK);

					format_float(tmp1, sizeof(tmp1), m_last_undecodable_data.rssi, 1);
					epaper_fb_draw_string(tmp1, EPAPER_COLOR_BLACK);
					epaper_fb_draw_string(" / ", EPAPER_COLOR_BLACK);

					format_float(tmp1, sizeof(tmp1), m_last_undecodable_data.snr, 2);
					epaper_fb_draw_string(tmp1, EPAPER_COLOR_BLACK);
					epaper_fb_draw_string(" / ", EPAPER_COLOR_BLACK);

					format_float(tmp1, sizeof(tmp1), m_last_undecodable_data.signalRssi, 1);
					epaper_fb_draw_string(tmp1, EPAPER_COLOR_BLACK);
				}
				break;

			case DISP_STATE_CLOCK:
				{
					struct tm utc;
					wall_clock_get_utc(&utc);

					snprintf(s, sizeof(s), "%02d:%02d", utc.tm_hour, utc.tm_min);
					uint8_t textwidth = epaper_fb_calc_text_width(s);

					epaper_fb_move_to(EPAPER_WIDTH/2 - textwidth/2, EPAPER_HEIGHT/2);
					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);

					snprintf(s, sizeof(s), "%04d-%02d-%02d", utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday);
					textwidth = epaper_fb_calc_text_width(s);

					epaper_fb_move_to(EPAPER_WIDTH/2 - textwidth/2, EPAPER_HEIGHT/2 + line_height);
					epaper_fb_draw_string(s, EPAPER_COLOR_BLACK);
				}
				break;

			case DISP_STATE_END:
				// this state should never be reached.
				epaper_fb_draw_string("BUG! Please report!", EPAPER_COLOR_BLACK);
				break;
		}
	}

	epaper_update(full_update);
}


