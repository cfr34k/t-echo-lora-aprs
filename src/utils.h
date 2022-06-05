#ifndef UTILS_H
#define UTILS_H

/**@brief Calculate the great-circle distance between two coordinates.
 *
 * @details
 * The calculation is done with the haversine formula from
 * https://en.wikipedia.org/wiki/Great-circle_distance which works well for
 * small distances if calculated in 32-bit float.
 *
 * @param lat1    Latitude of the first point.
 * @param lon1    Longitude of the first point.
 * @param lat2    Latitude of the second point.
 * @param lon2    Longitude of the second point.
 *
 * @returns The great-circle distance between the points in meters.
 */
float great_circle_distance_m(float lat1, float lon1, float lat2, float lon2);

/**@brief Calculate the direction angle from coordinate 1 to coordinate 2.
 *
 * Formula from https://en.wikipedia.org/wiki/Great-circle_navigation .
 *
 * @param lat1    Latitude of the first point.
 * @param lon1    Longitude of the first point.
 * @param lat2    Latitude of the second point.
 * @param lon2    Longitude of the second point.
 *
 * @returns  The direction angle in degrees (0 to 360°) from north.
 */
float direction_angle(float lat1, float lon1, float lat2, float lon2);

#endif // UTILS_H
