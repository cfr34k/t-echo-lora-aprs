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

#include "time_base.h"

#include "wall_clock.h"

static uint64_t m_unix_time_ref;
static uint64_t m_time_base_ref;


void wall_clock_init(void)
{
	m_unix_time_ref = 0;
	m_time_base_ref = time_base_get();
}


uint64_t wall_clock_get_unix(void)
{
	uint64_t unix_time_now = m_unix_time_ref
		+ (time_base_get() - m_time_base_ref) / 1000;

	return unix_time_now;
}


void wall_clock_get_utc(struct tm *time)
{
	time_t unix_time = wall_clock_get_unix();

	*time = *gmtime(&unix_time);
}


void wall_clock_set_from_gnss(const nmea_datetime_t *datetime)
{
	struct tm utc;

	utc.tm_sec = datetime->time_s;
	utc.tm_min = datetime->time_m;
	utc.tm_hour = datetime->time_h;

	utc.tm_mday = datetime->date_d;
	utc.tm_mon = datetime->date_m - 1; // tm_mon expects 0 to 11
	utc.tm_year = datetime->date_y - 1900;

	time_t unix_time = mktime(&utc);

	if(unix_time >= 0) {
		m_unix_time_ref = unix_time;
		m_time_base_ref = time_base_get();
	}
}
