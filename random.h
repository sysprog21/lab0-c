#ifndef LAB0_RANDOM_H
#define LAB0_RANDOM_H

#include <stddef.h>
#include <stdint.h>

void randombytes(uint8_t *x, size_t xlen);

static inline uint8_t randombit(void)
{
    uint8_t ret = 0;
    randombytes(&ret, 1);
    return ret & 1;
}

#endif
