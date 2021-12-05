#ifndef LNS_WRAP_H
#define LNS_WRAP_H

#include <sdk_errors.h>

#include "gps.h"

ret_code_t lns_wrap_init(void);
ret_code_t lns_wrap_update_data(const nmea_data_t *data);

#endif // LNS_WRAP_H
