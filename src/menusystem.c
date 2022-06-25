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

#define MAIN_ENTRY_IDX_RX            1
#define MAIN_ENTRY_IDX_TRACKER       2
#define MAIN_ENTRY_IDX_GNSS_WARMUP   3
#define MAIN_ENTRY_IDX_SYMBOL        4
#define MAIN_ENTRY_IDX_INFO          5

#define SYMBOL_SELECT_ENTRY_IDX_JOGGER      1
#define SYMBOL_SELECT_ENTRY_IDX_BICYCLE     2
#define SYMBOL_SELECT_ENTRY_IDX_MOTORCYCLE  3
#define SYMBOL_SELECT_ENTRY_IDX_CAR         4
#define SYMBOL_SELECT_ENTRY_IDX_TRUCK       5

#define INFO_ENTRY_IDX_VERSION           1
#define INFO_ENTRY_IDX_APRS_SOURCE       2
#define INFO_ENTRY_IDX_APRS_DEST         3
#define INFO_ENTRY_IDX_APRS_SYMBOL       4

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
	menuentry_t  entries[6];
	size_t       n_entries;
};

static menusystem_callback_t m_callback;

static menu_t m_main_menu;
static menu_t m_symbol_select_menu;
static menu_t m_info_menu;

static size_t  m_selected_entry, m_prev_selected_entry;
static menu_t *m_active_menu, *m_prev_menu;


// import some variables from main.c to make updating the menu easier
extern bool m_lora_rx_active;
extern bool m_tracker_active;


static void enter_submenu(menu_t *menu, size_t initial_index)
{
	m_prev_menu = m_active_menu;
	m_prev_selected_entry = m_selected_entry;
	m_active_menu = menu;
	m_selected_entry = initial_index;
	m_callback(MENUSYSTEM_EVT_REDRAW_REQUIRED, NULL);
}


static void leave_submenu(void)
{
	m_active_menu = m_prev_menu;
	m_selected_entry = m_prev_selected_entry;
	m_callback(MENUSYSTEM_EVT_REDRAW_REQUIRED, NULL);
}


static void menusystem_update_values(void)
{
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
	if(m_tracker_active) { // FIXME
		strncpy(entry->value, "on", sizeof(entry->value));
	} else {
		strncpy(entry->value, "off", sizeof(entry->value));
	}

	entry = &(m_main_menu.entries[MAIN_ENTRY_IDX_SYMBOL]);
	aprs_get_icon(&(entry->value[0]), &(entry->value[1]));
	entry->value[2] = '\0';

	entry = &(m_info_menu.entries[INFO_ENTRY_IDX_APRS_SOURCE]);
	aprs_get_source(entry->value, sizeof(entry->value));

	entry = &(m_info_menu.entries[INFO_ENTRY_IDX_APRS_DEST]);
	aprs_get_dest(entry->value, sizeof(entry->value));

	entry = &(m_info_menu.entries[INFO_ENTRY_IDX_APRS_SYMBOL]);
	strcpy(entry->value, m_main_menu.entries[MAIN_ENTRY_IDX_SYMBOL].value); // already filled, see above
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
			if(true) { // FIXME
				m_callback(MENUSYSTEM_EVT_GNSS_WARMUP_DISABLE, NULL);
			} else {
				m_callback(MENUSYSTEM_EVT_GNSS_WARMUP_ENABLE, NULL);
			}
			menusystem_update_values();
			break;

		case MAIN_ENTRY_IDX_SYMBOL:
			enter_submenu(&m_symbol_select_menu, 0);
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
			break;

		default:
			m_selected_entry = 0;
			m_callback(MENUSYSTEM_EVT_REDRAW_REQUIRED, NULL);
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
	m_main_menu.n_entries = 6;

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

	m_main_menu.entries[MAIN_ENTRY_IDX_SYMBOL].handler = menu_handler_main;
	m_main_menu.entries[MAIN_ENTRY_IDX_SYMBOL].text = "Symbol >";
	m_main_menu.entries[MAIN_ENTRY_IDX_SYMBOL].value[0] = '\0';

	m_main_menu.entries[MAIN_ENTRY_IDX_INFO].handler = menu_handler_main;
	m_main_menu.entries[MAIN_ENTRY_IDX_INFO].text = "Info >";
	m_main_menu.entries[MAIN_ENTRY_IDX_INFO].value[0] = '\0';

	// prepare the symbol select menu
	m_symbol_select_menu.n_entries = 6;

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
	m_info_menu.n_entries = 5;

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
