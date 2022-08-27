#include <stddef.h>
#include <string.h>

#ifdef SDL_DISPLAY
	// we are compiling for the emulator
	#include "sdl_display.h"
#else
	#include "epaper.h"
#endif

#include "menusystem.h"
#include "aprs.h"


#define ENTRY_IDX_EXIT               0

enum main_entry_ids_t {
	MAIN_ENTRY_IDX_RX           = 1,
	MAIN_ENTRY_IDX_TRACKER      = 2,
	MAIN_ENTRY_IDX_GNSS_WARMUP  = 3,
	MAIN_ENTRY_IDX_POWER        = 4,
	MAIN_ENTRY_IDX_APRS         = 5,
	MAIN_ENTRY_IDX_INFO         = 6,

	MAIN_ENTRY_COUNT
};

enum symbol_select_entry_ids_t {
	SYMBOL_SELECT_ENTRY_IDX_JOGGER      = 1,
	SYMBOL_SELECT_ENTRY_IDX_BICYCLE     = 2,
	SYMBOL_SELECT_ENTRY_IDX_MOTORCYCLE  = 3,
	SYMBOL_SELECT_ENTRY_IDX_CAR         = 4,
	SYMBOL_SELECT_ENTRY_IDX_TRUCK       = 5,

	SYMBOL_SELECT_ENTRY_COUNT
};

enum info_entry_ids_t {
	INFO_ENTRY_IDX_VERSION           = 1,
	INFO_ENTRY_IDX_APRS_SOURCE       = 2,
	INFO_ENTRY_IDX_APRS_DEST         = 3,
	INFO_ENTRY_IDX_APRS_SYMBOL       = 4,

	INFO_ENTRY_COUNT
};

enum aprs_config_entry_ids_t {
	APRS_CONFIG_ENTRY_IDX_COMPRESSED        = 1,
	APRS_CONFIG_ENTRY_IDX_ALTITUDE          = 2,
	APRS_CONFIG_ENTRY_IDX_PACKET_ID         = 3,
	APRS_CONFIG_ENTRY_IDX_DAO               = 4,
	APRS_CONFIG_ENTRY_IDX_APRS_SYMBOL       = 5,

	APRS_CONFIG_ENTRY_COUNT
};

#define POWER_SELECT_ENTRY_COUNT   (LORA_PWR_NUM_ENTRIES + 1)

typedef struct menuentry_s menuentry_t;
typedef struct menu_s menu_t;

typedef void (*menuentry_handler_t)(menu_t *menu, menuentry_t *entry);

struct menuentry_s
{
	menuentry_handler_t  handler;   /**< Function pointer to call when this entry is activated. */
	const char          *text;      /**< Entry text */
	char                 value[32]; /**< Entry value (rendered right-aligned) */
};

struct menu_s
{
	menu_t      *prev_menu;
	size_t       prev_selected_entry;

	menuentry_t *entries;
	size_t       n_entries;
};

static menusystem_callback_t m_callback;

static menu_t m_main_menu;
static menu_t m_power_select_menu;
static menu_t m_aprs_config_menu;
static menu_t m_symbol_select_menu;
static menu_t m_info_menu;

static menuentry_t m_main_entries[MAIN_ENTRY_COUNT];
static menuentry_t m_power_select_entries[POWER_SELECT_ENTRY_COUNT];
static menuentry_t m_aprs_config_entries[APRS_CONFIG_ENTRY_COUNT];
static menuentry_t m_symbol_select_entries[SYMBOL_SELECT_ENTRY_COUNT];
static menuentry_t m_info_entries[INFO_ENTRY_COUNT];

static size_t  m_selected_entry;
static menu_t *m_active_menu;


// import some variables from main.c to make updating the menu easier
extern bool m_lora_rx_active;
extern bool m_tracker_active;
extern bool m_gps_warmup_active;


static void enter_submenu(menu_t *menu, size_t initial_index)
{
	menu->prev_menu = m_active_menu;
	menu->prev_selected_entry = m_selected_entry;
	m_active_menu = menu;
	m_selected_entry = initial_index;
	m_callback(MENUSYSTEM_EVT_REDRAW_REQUIRED, NULL);
}


static void leave_submenu(void)
{
	m_selected_entry = m_active_menu->prev_selected_entry;
	m_active_menu = m_active_menu->prev_menu;

	m_callback(MENUSYSTEM_EVT_REDRAW_REQUIRED, NULL);
}


static void menusystem_update_values(void)
{
	// main menu
	menuentry_t *entry = &(m_main_menu.entries[MAIN_ENTRY_IDX_RX]);
	if(m_lora_rx_active) {
		strncpy(entry->value, "on", sizeof(entry->value));
	} else {
		strncpy(entry->value, "off", sizeof(entry->value));
	}

	entry = &(m_main_menu.entries[MAIN_ENTRY_IDX_TRACKER]);
	if(m_tracker_active) {
		strncpy(entry->value, "on", sizeof(entry->value));
	} else {
		strncpy(entry->value, "off", sizeof(entry->value));
	}

	entry = &(m_main_menu.entries[MAIN_ENTRY_IDX_GNSS_WARMUP]);
	if(m_gps_warmup_active) {
		strncpy(entry->value, "on", sizeof(entry->value));
	} else {
		strncpy(entry->value, "off", sizeof(entry->value));
	}

	// aprs config menu
	uint32_t aprs_flags = aprs_get_config_flags();

	entry = &(m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_COMPRESSED]);
	if(aprs_flags & APRS_FLAG_COMPRESS_LOCATION) {
		strncpy(entry->value,  "on", sizeof(entry->value));
	} else {
		strncpy(entry->value,  "off", sizeof(entry->value));
	}

	entry = &(m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_ALTITUDE]);
	if(aprs_flags & APRS_FLAG_ADD_ALTITUDE) {
		strncpy(entry->value,  "on", sizeof(entry->value));
	} else {
		strncpy(entry->value,  "off", sizeof(entry->value));
	}

	entry = &(m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_DAO]);
	if(aprs_flags & APRS_FLAG_ADD_DAO) {
		strncpy(entry->value,  "on", sizeof(entry->value));
	} else {
		strncpy(entry->value,  "off", sizeof(entry->value));
	}

	entry = &(m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_PACKET_ID]);
	if(aprs_flags & APRS_FLAG_ADD_FRAME_COUNTER) {
		strncpy(entry->value,  "on", sizeof(entry->value));
	} else {
		strncpy(entry->value,  "off", sizeof(entry->value));
	}

	entry = &(m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_APRS_SYMBOL]);
	aprs_get_icon(&(entry->value[0]), &(entry->value[1]));
	entry->value[2] = '\0';

	// info menu
	entry = &(m_info_menu.entries[INFO_ENTRY_IDX_APRS_SOURCE]);
	aprs_get_source(entry->value, sizeof(entry->value));

	entry = &(m_info_menu.entries[INFO_ENTRY_IDX_APRS_DEST]);
	aprs_get_dest(entry->value, sizeof(entry->value));

	entry = &(m_info_menu.entries[INFO_ENTRY_IDX_APRS_SYMBOL]);
	strcpy(entry->value, m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_APRS_SYMBOL].value); // already filled, see above

	entry = &(m_main_menu.entries[MAIN_ENTRY_IDX_POWER]);
	strncpy(entry->value, lora_power_to_str(lora_get_power()), sizeof(entry->value));

}


static void menu_handler_main(menu_t *menu, menuentry_t *entry)
{
	size_t entry_idx = entry - &(menu->entries[0]);

	switch(entry_idx) {
		case ENTRY_IDX_EXIT:
			m_active_menu = NULL;
			m_callback(MENUSYSTEM_EVT_EXIT_MENU, NULL);
			break;

		case MAIN_ENTRY_IDX_RX:
			if(m_lora_rx_active) {
				m_callback(MENUSYSTEM_EVT_RX_DISABLE, NULL);
			} else {
				m_callback(MENUSYSTEM_EVT_RX_ENABLE, NULL);
			}
			menusystem_update_values();
			break;

		case MAIN_ENTRY_IDX_TRACKER:
			if(m_tracker_active) {
				m_callback(MENUSYSTEM_EVT_TRACKER_DISABLE, NULL);
			} else {
				m_callback(MENUSYSTEM_EVT_TRACKER_ENABLE, NULL);
			}
			menusystem_update_values();
			break;

		case MAIN_ENTRY_IDX_GNSS_WARMUP:
			if(m_gps_warmup_active) {
				m_callback(MENUSYSTEM_EVT_GNSS_WARMUP_DISABLE, NULL);
			} else {
				m_callback(MENUSYSTEM_EVT_GNSS_WARMUP_ENABLE, NULL);
			}
			menusystem_update_values();
			break;

		case MAIN_ENTRY_IDX_POWER:
			enter_submenu(&m_power_select_menu, 0);
			break;

		case MAIN_ENTRY_IDX_APRS:
			enter_submenu(&m_aprs_config_menu, 0);
			break;

		case MAIN_ENTRY_IDX_INFO:
			enter_submenu(&m_info_menu, 0);
			break;

		default:
			m_selected_entry = 0;
			m_callback(MENUSYSTEM_EVT_REDRAW_REQUIRED, NULL);
			break;
	}
}


static void menu_handler_aprs_config(menu_t *menu, menuentry_t *entry)
{
	size_t entry_idx = entry - &(menu->entries[0]);

	bool flags_changed = false;

	switch(entry_idx) {
		case ENTRY_IDX_EXIT:
			leave_submenu();
			break;

		case APRS_CONFIG_ENTRY_IDX_COMPRESSED:
			aprs_toggle_config_flag(APRS_FLAG_COMPRESS_LOCATION);
			flags_changed = true;
			break;

		case APRS_CONFIG_ENTRY_IDX_DAO:
			aprs_toggle_config_flag(APRS_FLAG_ADD_DAO);
			flags_changed = true;
			break;

		case APRS_CONFIG_ENTRY_IDX_PACKET_ID:
			aprs_toggle_config_flag(APRS_FLAG_ADD_FRAME_COUNTER);
			flags_changed = true;
			break;

		case APRS_CONFIG_ENTRY_IDX_ALTITUDE:
			aprs_toggle_config_flag(APRS_FLAG_ADD_ALTITUDE);
			flags_changed = true;
			break;

		case APRS_CONFIG_ENTRY_IDX_APRS_SYMBOL:
			enter_submenu(&m_symbol_select_menu, 0);
			break;

		default:
			m_selected_entry = 0;
			m_callback(MENUSYSTEM_EVT_REDRAW_REQUIRED, NULL);
			break;
	}

	if(flags_changed) {
		menusystem_evt_data_t evt_data;

		evt_data.aprs_flags.flags = aprs_get_config_flags();
		m_callback(MENUSYSTEM_EVT_APRS_FLAGS_CHANGED, &evt_data);

		menusystem_update_values();
	}
}


static void menu_handler_symbol_select(menu_t *menu, menuentry_t *entry)
{
	size_t entry_idx = entry - &(menu->entries[0]);

	menusystem_evt_data_t evt_data;

	switch(entry_idx) {
		case ENTRY_IDX_EXIT:
			leave_submenu();
			break;

		case SYMBOL_SELECT_ENTRY_IDX_JOGGER:
		case SYMBOL_SELECT_ENTRY_IDX_BICYCLE:
		case SYMBOL_SELECT_ENTRY_IDX_MOTORCYCLE:
		case SYMBOL_SELECT_ENTRY_IDX_CAR:
		case SYMBOL_SELECT_ENTRY_IDX_TRUCK:
			evt_data.aprs_symbol.table = entry->value[0];
			evt_data.aprs_symbol.symbol = entry->value[1];
			m_callback(MENUSYSTEM_EVT_APRS_SYMBOL_CHANGED, &evt_data);
			leave_submenu();
			menusystem_update_values();
			break;

		default:
			m_selected_entry = 0;
			m_callback(MENUSYSTEM_EVT_REDRAW_REQUIRED, NULL);
			break;
	}
}


static void menu_handler_power_select(menu_t *menu, menuentry_t *entry)
{
	size_t entry_idx = entry - &(menu->entries[0]);

	menusystem_evt_data_t evt_data;

	switch(entry_idx) {
		case ENTRY_IDX_EXIT:
			leave_submenu();
			break;

		default:
			evt_data.lora_power.power = entry_idx - 1;
			m_callback(MENUSYSTEM_EVT_LORA_POWER_CHANGED, &evt_data);
			leave_submenu();
			menusystem_update_values();
			break;
	}
}


static void menu_handler_info(menu_t *menu, menuentry_t *entry)
{
	leave_submenu();
}


void menusystem_init(menusystem_callback_t callback)
{
	m_callback = callback;

	// prepare the main menu
	m_main_menu.n_entries = MAIN_ENTRY_COUNT;
	m_main_menu.entries = m_main_entries;

	m_main_menu.entries[ENTRY_IDX_EXIT].handler = menu_handler_main;
	m_main_menu.entries[ENTRY_IDX_EXIT].text = "<<< Exit";
	m_main_menu.entries[ENTRY_IDX_EXIT].value[0] = '\0';

	m_main_menu.entries[MAIN_ENTRY_IDX_RX].handler = menu_handler_main;
	m_main_menu.entries[MAIN_ENTRY_IDX_RX].text = "Receiver";
	m_main_menu.entries[MAIN_ENTRY_IDX_RX].value[0] = '\0';

	m_main_menu.entries[MAIN_ENTRY_IDX_TRACKER].handler = menu_handler_main;
	m_main_menu.entries[MAIN_ENTRY_IDX_TRACKER].text = "Tracker";
	m_main_menu.entries[MAIN_ENTRY_IDX_TRACKER].value[0] = '\0';

	m_main_menu.entries[MAIN_ENTRY_IDX_GNSS_WARMUP].handler = menu_handler_main;
	m_main_menu.entries[MAIN_ENTRY_IDX_GNSS_WARMUP].text = "GNSS Warmup";
	m_main_menu.entries[MAIN_ENTRY_IDX_GNSS_WARMUP].value[0] = '\0';

	m_main_menu.entries[MAIN_ENTRY_IDX_POWER].handler = menu_handler_main;
	m_main_menu.entries[MAIN_ENTRY_IDX_POWER].text = "TX Power >";
	m_main_menu.entries[MAIN_ENTRY_IDX_POWER].value[0] = '\0';

	m_main_menu.entries[MAIN_ENTRY_IDX_APRS].handler = menu_handler_main;
	m_main_menu.entries[MAIN_ENTRY_IDX_APRS].text = "APRS Config >";
	m_main_menu.entries[MAIN_ENTRY_IDX_APRS].value[0] = '\0';

	m_main_menu.entries[MAIN_ENTRY_IDX_INFO].handler = menu_handler_main;
	m_main_menu.entries[MAIN_ENTRY_IDX_INFO].text = "Info >";
	m_main_menu.entries[MAIN_ENTRY_IDX_INFO].value[0] = '\0';

	// prepare the power select menu
	m_power_select_menu.n_entries = POWER_SELECT_ENTRY_COUNT;
	m_power_select_menu.entries = m_power_select_entries;

	m_power_select_menu.entries[ENTRY_IDX_EXIT].handler = menu_handler_power_select;
	m_power_select_menu.entries[ENTRY_IDX_EXIT].text = "<<< Cancel";
	m_power_select_menu.entries[ENTRY_IDX_EXIT].value[0] = '\0';

	for(lora_pwr_t pwr = 0; pwr < LORA_PWR_NUM_ENTRIES; pwr++) {
		size_t menu_idx = pwr + 1;

		m_power_select_menu.entries[menu_idx].handler = menu_handler_power_select;
		m_power_select_menu.entries[menu_idx].text = lora_power_to_str(pwr);
		m_power_select_menu.entries[menu_idx].value[0] = '\0';
	}

	// prepare the APRS config menu
	m_aprs_config_menu.n_entries = APRS_CONFIG_ENTRY_COUNT;
	m_aprs_config_menu.entries = m_aprs_config_entries;

	m_aprs_config_menu.entries[ENTRY_IDX_EXIT].handler = menu_handler_aprs_config;
	m_aprs_config_menu.entries[ENTRY_IDX_EXIT].text = "<<< Back";
	m_aprs_config_menu.entries[ENTRY_IDX_EXIT].value[0] = '\0';

	m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_COMPRESSED].handler = menu_handler_aprs_config;
	m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_COMPRESSED].text = "Compressed format";
	m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_COMPRESSED].value[0] = '\0';

	m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_ALTITUDE].handler = menu_handler_aprs_config;
	m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_ALTITUDE].text = "Altitude";
	m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_ALTITUDE].value[0] = '\0';

	m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_DAO].handler = menu_handler_aprs_config;
	m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_DAO].text = "DAO";
	m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_DAO].value[0] = '\0';

	m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_PACKET_ID].handler = menu_handler_aprs_config;
	m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_PACKET_ID].text = "Frame counter";
	m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_PACKET_ID].value[0] = '\0';

	m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_APRS_SYMBOL].handler = menu_handler_aprs_config;
	m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_APRS_SYMBOL].text = "Symbol >>>";
	m_aprs_config_menu.entries[APRS_CONFIG_ENTRY_IDX_APRS_SYMBOL].value[0] = '\0';

	// prepare the symbol select menu
	m_symbol_select_menu.n_entries = SYMBOL_SELECT_ENTRY_COUNT;
	m_symbol_select_menu.entries = m_symbol_select_entries;

	m_symbol_select_menu.entries[ENTRY_IDX_EXIT].handler = menu_handler_symbol_select;
	m_symbol_select_menu.entries[ENTRY_IDX_EXIT].text = "<<< Cancel";
	m_symbol_select_menu.entries[ENTRY_IDX_EXIT].value[0] = '\0';

	m_symbol_select_menu.entries[SYMBOL_SELECT_ENTRY_IDX_JOGGER].handler = menu_handler_symbol_select;
	m_symbol_select_menu.entries[SYMBOL_SELECT_ENTRY_IDX_JOGGER].text = "Jogger";
	strcpy(m_symbol_select_menu.entries[SYMBOL_SELECT_ENTRY_IDX_JOGGER].value, "/[");

	m_symbol_select_menu.entries[SYMBOL_SELECT_ENTRY_IDX_BICYCLE].handler = menu_handler_symbol_select;
	m_symbol_select_menu.entries[SYMBOL_SELECT_ENTRY_IDX_BICYCLE].text = "Bicycle";
	strcpy(m_symbol_select_menu.entries[SYMBOL_SELECT_ENTRY_IDX_BICYCLE].value, "/b");

	m_symbol_select_menu.entries[SYMBOL_SELECT_ENTRY_IDX_MOTORCYCLE].handler = menu_handler_symbol_select;
	m_symbol_select_menu.entries[SYMBOL_SELECT_ENTRY_IDX_MOTORCYCLE].text = "Motorcycle";
	strcpy(m_symbol_select_menu.entries[SYMBOL_SELECT_ENTRY_IDX_MOTORCYCLE].value, "/<");

	m_symbol_select_menu.entries[SYMBOL_SELECT_ENTRY_IDX_CAR].handler = menu_handler_symbol_select;
	m_symbol_select_menu.entries[SYMBOL_SELECT_ENTRY_IDX_CAR].text = "Car";
	strcpy(m_symbol_select_menu.entries[SYMBOL_SELECT_ENTRY_IDX_CAR].value, "/>");

	m_symbol_select_menu.entries[SYMBOL_SELECT_ENTRY_IDX_TRUCK].handler = menu_handler_symbol_select;
	m_symbol_select_menu.entries[SYMBOL_SELECT_ENTRY_IDX_TRUCK].text = "Truck";
	strcpy(m_symbol_select_menu.entries[SYMBOL_SELECT_ENTRY_IDX_TRUCK].value, "/k");

	// prepare the info menu
	m_info_menu.n_entries = INFO_ENTRY_COUNT;
	m_info_menu.entries = m_info_entries;

	m_info_menu.entries[ENTRY_IDX_EXIT].handler = menu_handler_info;
	m_info_menu.entries[ENTRY_IDX_EXIT].text = "<<< Back";
	m_info_menu.entries[ENTRY_IDX_EXIT].value[0] = '\0';

	m_info_menu.entries[INFO_ENTRY_IDX_VERSION].handler = menu_handler_info;
	m_info_menu.entries[INFO_ENTRY_IDX_VERSION].text = "FW";
	strcpy(m_info_menu.entries[INFO_ENTRY_IDX_VERSION].value, VERSION);

	m_info_menu.entries[INFO_ENTRY_IDX_APRS_SOURCE].handler = menu_handler_info;
	m_info_menu.entries[INFO_ENTRY_IDX_APRS_SOURCE].text = "Source";
	m_info_menu.entries[INFO_ENTRY_IDX_APRS_SOURCE].value[0] = '\0';

	m_info_menu.entries[INFO_ENTRY_IDX_APRS_DEST].handler = menu_handler_info;
	m_info_menu.entries[INFO_ENTRY_IDX_APRS_DEST].text = "Destination";
	m_info_menu.entries[INFO_ENTRY_IDX_APRS_DEST].value[0] = '\0';

	m_info_menu.entries[INFO_ENTRY_IDX_APRS_SYMBOL].handler = menu_handler_info;
	m_info_menu.entries[INFO_ENTRY_IDX_APRS_SYMBOL].text = "Symbol";
	m_info_menu.entries[INFO_ENTRY_IDX_APRS_SYMBOL].value[0] = '\0';

	m_active_menu = NULL;
}


void menusystem_enter(void)
{
	m_selected_entry = ENTRY_IDX_EXIT;
	m_active_menu = &m_main_menu;

	menusystem_update_values();
}


void menusystem_input(menusystem_input_t input)
{
	if(!m_active_menu) {
		return;
	}

	switch(input) {
		case MENUSYSTEM_INPUT_NEXT:
			m_selected_entry++;
			if(m_selected_entry >= m_active_menu->n_entries) {
				m_selected_entry = 0;
			}
			m_callback(MENUSYSTEM_EVT_REDRAW_REQUIRED, NULL);
			break;

		case MENUSYSTEM_INPUT_CONFIRM:
			m_active_menu->entries[m_selected_entry].handler(
					m_active_menu, &(m_active_menu->entries[m_selected_entry]));
			break;
	}
}


void menusystem_render(uint8_t first_line_base)
{
	uint8_t yoffset = first_line_base;
	uint8_t line_height = epaper_fb_get_line_height();

	if(!m_active_menu) {
		epaper_fb_move_to(0, yoffset);
		epaper_fb_draw_string("Error: menu inactive.", EPAPER_COLOR_BLACK);
		return;
	}

	for(size_t i = 0; i < m_active_menu->n_entries; i++) {
		uint8_t fg_color, bg_color;

		if(i == m_selected_entry) {
			fg_color = EPAPER_COLOR_WHITE;
			bg_color = EPAPER_COLOR_BLACK;
		} else {
			fg_color = EPAPER_COLOR_BLACK;
			bg_color = EPAPER_COLOR_WHITE;
		}

		epaper_fb_fill_rect(0, yoffset - line_height, EPAPER_WIDTH, yoffset, bg_color);
		epaper_fb_move_to(0, yoffset-6);
		epaper_fb_draw_string(m_active_menu->entries[i].text, fg_color);

		uint8_t textwidth = epaper_fb_calc_text_width(m_active_menu->entries[i].value);
		epaper_fb_move_to(EPAPER_WIDTH - textwidth, yoffset-6);
		epaper_fb_draw_string(m_active_menu->entries[i].value, fg_color);

		yoffset += line_height;
	}
}


bool menusystem_is_active(void)
{
	return m_active_menu != NULL;
}
