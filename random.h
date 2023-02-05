#ifndef LAB0_RANDOM_H
#define LAB0_RANDOM_H

#include <stddef.h>
#include <stdint.h>

extern int randombytes(uint8_t *buf, size_t len);

static inline uint8_t randombit(void)
{
    uint8_t ret = 0;
    randombytes(&ret, 1);
    return ret & 1;
}

#endif
