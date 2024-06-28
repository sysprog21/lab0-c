#include "fixed_point.h"

/**
 * return a * b in fixed-point format
 */
fixed_point_t multi_f(fixed_point_t a, fixed_point_t b)
{
    unsigned long result = a * b;
    result += 1UL << (FSHIFT - 1);
    result >>= FSHIFT;

    return result;
}


/**
 * using divison-free approximation of sqrt
 * ref: https://hackmd.io/@vax-r/linux2024-homework1#Monte-Carlo-Search-Tree
 */
fixed_point_t sqrt_f(fixed_point_t num)
{
    fixed_point_t res = 0L;
    /* i is intialized as the index of first bit 1 in num */
    /* t starts from num, divided by two each time */
    for (int i = 31 - __builtin_clz(num | 1); i >= 0; i--) {
        fixed_point_t t = 1UL << i;
        if (multi_f(res + t, res + t) <= num)
            res += t;
    }
    return res;
}

/**
 * ref: https://github.com/dmoulding/log2fix/blob/master/log2fix.c
 */
fixed_point_t log2_f(fixed_point_t num)
{
    if (num <= 0)
        return INT32_MIN;

    /* b = 1/2 */
    fixed_point_t b = LOAD_FIXED(1L) >> 1;
    fixed_point_t y = 0;

    /* make num in [1, 2) */
    while (num < LOAD_FIXED(1L)) {
        num <<= 1;
        y -= LOAD_FIXED(1L);
    }
    while (num >= LOAD_FIXED(2L)) {
        num >>= 1;
        y += LOAD_FIXED(1L);
    }

    fixed_point_t z = num;
    for (size_t i = 0; i < FSHIFT; i++) {
        z = multi_f(z, z);
        /* if x >= 2: x = x/2, y = y+b */
        if (z >= LOAD_FIXED(2L)) {
            z >>= 1;
            y += b;
        }
        b >>= 1;
    }

    return y;
}

fixed_point_t loge_f(fixed_point_t num)
{
    uint64_t t = log2_f(num) * INV_LOG2_E;
    return t >> 31;
}