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

#ifndef FASTTRIGON_H
#define FASTTRIGON_H

/*!
 * \file
 * 
 * This file contains function prototypes for very fast lookup-table based
 * trigonometric functions.
 * 
 * The functions take an argument that is dependent on the lookup-table size
 * (LUT_SIZE). LUT_SIZE corresponds to 2π. Values outside the range
 * [0..LUT_SIZE] are supported and interpreted normally, so just replace 2π
 * with LUT_SIZE and you're done.
 */

#include <stdint.h>

#define FASTTRIGON_8BIT(x) ((x) >> (fasttrigon::PRECISION_BITS - 8))

#define FASTTRIGON_LUT_SIZE        2048
#define FASTTRIGON_PRECISION_BITS  14
#define FASTTRIGON_SCALE           8191
#define FASTTRIGON_UNIT_SHIFT      (FASTTRIGON_PRECISION_BITS-1)

/*!
* \returns SCALE * sin(2π * arg / LUT_SIZE)
*/
int32_t fasttrigon_sin(int32_t arg);

/*!
* \returns SCALE * cos(2π * arg / LUT_SIZE)
*/
int32_t fasttrigon_cos(int32_t arg);

/*!
* \returns SCALE * tan(2π * arg / LUT_SIZE)
*/
int32_t fasttrigon_tan(int32_t arg);

#endif // FASTTRIGON_H
