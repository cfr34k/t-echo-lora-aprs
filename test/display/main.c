#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include <stdio.h>

#include "SDL_keysym.h"
#include "sdl_display.h"

#include "utils.h"
#include "aprs.h"
#include "nmea.h"
#include "menusystem.h"

#include "display.h"


uint16_t m_bat_millivolt = 3456;
uint8_t  m_bat_percent = 42;
bool     m_lora_rx_busy = false;
bool     m_lora_tx_busy = false;

// status info shared with other modules
bool     m_lora_rx_active = false;
bool     m_lora_tx_active = true;
bool     m_tracker_active = true;
bool     m_gps_warmup_active = true;

nmea_data_t m_nmea_data = {
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

bool m_nmea_has_position = true;

aprs_frame_t m_aprs_decoded_message = {
	"DL5TKL-4",
	"APZTK1",
	"WIDE1-1",
	43.21f,
	12.34f,
	100.0f,
	"Hello World!",
	'/', 'b'
};

bool m_aprs_decode_ok = true;

uint8_t m_display_message[256] = "Hello World!";
uint8_t m_display_message_len = 12;

float m_rssi = -100, m_snr = 42, m_signalRssi = -127;

static bool m_redraw_required = true;

display_state_t m_display_state = DISP_STATE_STARTUP;


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

void epaper_update(bool full) {}

uint32_t tracker_get_tx_counter(void) { return 12345; }

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

		case MENUSYSTEM_EVT_APRS_FLAGS_CHANGED:
			printf("New APRS flags: 0x%08x\n", data->aprs_flags.flags);
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
