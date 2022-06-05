#include <math.h>

#include "utils.h"


#define F_PI   3.141592653589793f

#define EARTH_RADIUS_M   6371000
float great_circle_distance_m(float lat1, float lon1, float lat2, float lon2)
{
	// convert to radians
	lat1 *= F_PI / 180.0f;
	lon1 *= F_PI / 180.0f;
	lat2 *= F_PI / 180.0f;
	lon2 *= F_PI / 180.0f;

	// calculation using the haversine formula from
	// https://en.wikipedia.org/wiki/Great-circle_distance
	float sin_dlat_over_2 = sinf((lat2 - lat1) * 0.5f);
	float sin_dlon_over_2 = sinf((lon2 - lon1) * 0.5f);
	float sin_sumlat_over_2 = sinf((lat2 + lat1) * 0.5f);

	float sin_sq_dlat_over_2 = sin_dlat_over_2 * sin_dlat_over_2;
	float sin_sq_dlon_over_2 = sin_dlon_over_2 * sin_dlon_over_2;
	float sin_sq_sumlat_over_2 = sin_sumlat_over_2 * sin_sumlat_over_2;

	float arg = sqrtf(sin_sq_dlat_over_2 + (1 - sin_sq_dlat_over_2 - sin_sq_sumlat_over_2) * sin_sq_dlon_over_2);
	float angle = 2.0f * asinf(arg);
	return angle * EARTH_RADIUS_M;
}

float direction_angle(float lat1, float lon1, float lat2, float lon2)
{
	// convert to radians
	lat1 *= F_PI / 180.0f;
	lon1 *= F_PI / 180.0f;
	lat2 *= F_PI / 180.0f;
	lon2 *= F_PI / 180.0f;

	float lon12 = lon2 - lon1;

	float numer = cosf(lat2) * sinf(lon12);
	float denum = cosf(lat1) * sinf(lat2) - sinf(lat1) * cosf(lat2) * cosf(lon12);

	float angle = atan2f(numer, denum) * (180.0f / F_PI);

	if(angle < 0) {
		angle = 360.0f + angle;
	}

	return angle;
}
