#ifndef DUDECT_CONSTANT_H
#define DUDECT_CONSTANT_H

#include <stdint.h>
#define dut_new() ((void) (l = q_new()))

#define dut_size(n)                                \
    do {                                           \
        for (int __iter = 0; __iter < n; ++__iter) \
            q_size(l);                             \
    } while (0)

#define dut_insert_head(s, n)    \
    do {                         \
        int j = n;               \
        while (j--)              \
            q_insert_head(l, s); \
    } while (0)

#define dut_insert_tail(s, n)    \
    do {                         \
        int j = n;               \
        while (j--)              \
            q_insert_tail(l, s); \
    } while (0)

#define dut_free() ((void) (q_free(l)))

void init_dut();
void prepare_inputs(uint8_t *input_data, uint8_t *classes);
void measure(int64_t *before_ticks,
             int64_t *after_ticks,
             uint8_t *input_data,
             int mode);

#endif
