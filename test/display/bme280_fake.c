#include "bme280.h"

bool bme280_is_present(void)
{
	return 1;
}


float bme280_get_temperature(void)
{
	return 23.42f;
}


float bme280_get_humidity(void)
{
	return 67.89f;
}


float bme280_get_pressure(void)
{
	return 1013.42;
}
