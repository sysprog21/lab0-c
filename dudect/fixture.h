#ifndef DUDECT_FIXTURE_H
#define DUDECT_FIXTURE_H

#include <stdbool.h>
#include "constant.h"

#define ENOUGH_MEASURE 10000
#define TEST_TRIES 10

/* Interface to test if function is constant */
#define _(x) bool is_##x##_const(void);
DUT_FUNCS
#undef _

#endif
