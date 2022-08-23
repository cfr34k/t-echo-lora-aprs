#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>


typedef enum
{
	DISP_STATE_STARTUP,
	DISP_STATE_GPS,
	DISP_STATE_TRACKER,
	DISP_STATE_LORA_PACKET_DECODED,
	DISP_STATE_LORA_PACKET_RAW,

	DISP_STATE_END
} display_state_t;


void redraw_display(bool full_update);

#endif // DISPLAY_H
