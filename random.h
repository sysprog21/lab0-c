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

#if INTPTR_MAX == INT64_MAX
#define M_INTPTR_SHIFT (3)
#elif INTPTR_MAX == INT32_MAX
#define M_INTPTR_SHIFT (2)
#else
#error Platform pointers must be 32 or 64 bits
#endif

#define M_INTPTR_SIZE (1 << M_INTPTR_SHIFT)

static inline uintptr_t random_shuffle(uintptr_t x)
{
    /* Ensure we do not get stuck in generating zeros */
    if (x == 0)
        x = 17;

#if M_INTPTR_SIZE == 8
    /* by Sebastiano Vigna, see: <http://xoshiro.di.unimi.it/splitmix64.c> */
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9UL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebUL;
    x ^= x >> 31;
#elif M_INTPTR_SIZE == 4
    /* by Chris Wellons, see: <https://nullprogram.com/blog/2018/07/31/> */
    x ^= x >> 16;
    x *= 0x7feb352dUL;
    x ^= x >> 15;
    x *= 0x846ca68bUL;
    x ^= x >> 16;
#endif

    return x;
}

#endif
