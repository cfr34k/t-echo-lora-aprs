/*
 * vim: noexpandtab
 *
 * Copyright (c) 2021-2022 Thomas Kolb <cfr34k-git@tkolb.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>

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


/**@brief Format the given floating point number into a string.
 *
 * This function is a workaround for systems where there is no floating point
 * support in sprintf().
 *
 * @param[out] s         Pointer to the output buffer.
 * @param[in]  s_len     Length of the output buffer.
 * @param[in]  f         The number to format.
 * @param[in]  decimals  The number of digits right of the decimal point.
 */
void format_float(char *s, size_t s_len, float f, uint8_t decimals);

#endif // UTILS_H
