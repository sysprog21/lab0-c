#ifndef DUDECT_FIXTURE_H
#define DUDECT_FIXTURE_H

#include <stdbool.h>
#include "constant.h"

/* Interface to test if function is constant */
bool is_insert_head_const(void);
bool is_insert_tail_const(void);
bool is_remove_head_const(void);
bool is_remove_tail_const(void);

#endif
