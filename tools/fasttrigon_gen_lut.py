#!/usr/bin/env python3

import numpy as np

LUT_SIZE = 2048
PRECISION_BITS = 14

SCALE = (1 << (PRECISION_BITS-1)) - 1

x = np.array(range(LUT_SIZE)) * 2*np.pi / LUT_SIZE
y = (SCALE + 0.5) * np.sin(x)

print(f"#define FASTTRIGON_LUT_SIZE       {LUT_SIZE}")
print(f"#define FASTTRIGON_PRECISION_BITS {PRECISION_BITS}")
print(f"#define FASTTRIGON_SCALE          {SCALE}")

print(list(y.astype('int')))
