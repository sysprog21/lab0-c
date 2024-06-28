#ifndef LAB0_FIXEDPOINT_H
#define LAB0_FIXEDPOINT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* Linux-kernel fixed point arthimetic implementation */

#define FSHIFT 11             /* n bits of precision */
#define FIXED_1 (1 << FSHIFT) /* 1.0 as fixed point */

#define LOAD_FIXED(x) ((x) << FSHIFT) /* change int to fixed-point */
#define LOAD_INT(x) ((x) >> FSHIFT)

#define INV_LOG2_E 0x58b90bfc /* Inverse log base 2 of e */

typedef long fixed_point_t;

/**
 * return a * b in fixed-point format
 */
fixed_point_t multi_f(fixed_point_t a, fixed_point_t b);

/**
 * using divison-free approximation of sqrt
 * ref: https://hackmd.io/@vax-r/linux2024-homework1#Monte-Carlo-Search-Tree
 */
fixed_point_t sqrt_f(fixed_point_t num);

fixed_point_t loge_f(fixed_point_t num);

#endif