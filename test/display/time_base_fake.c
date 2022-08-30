#include <time.h>
#include <stdint.h>

uint64_t time_base_get(void)
{
	return time(NULL) * 1000;
}
