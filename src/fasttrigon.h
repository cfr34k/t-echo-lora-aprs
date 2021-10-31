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
