#ifndef TRACKER_H
#define TRACKER_H

#include <sdk_errors.h>

#include "nmea.h"

typedef enum {
	TRACKER_EVT_TRANSMISSION_STARTED,
} tracker_evt_t;

typedef void (*tracker_callback)(tracker_evt_t evt);

/**@brief Initialize all modules necessary for tracking.
 */
ret_code_t tracker_init(tracker_callback callback);

/**@brief Process a new position report in the tracker.
 */
ret_code_t tracker_run(const nmea_data_t *data);

#endif // TRACKER_H
