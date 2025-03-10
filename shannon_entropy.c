#include <assert.h>
#include <stdint.h>
#include <string.h>

/* Precalculated log2 realization */
#include "log2_lshift16.h"

/* Shannon full integer entropy calculation */
#define BUCKET_SIZE (1 << 8)

double shannon_entropy(const uint8_t *s)
{
    assert(s);
    const uint64_t count = strlen((char *) s);
    uint64_t entropy_sum = 0;
    const uint64_t entropy_max = 8 * LOG2_RET_SHIFT;

    uint32_t bucket[BUCKET_SIZE];
    memset(&bucket, 0, sizeof(bucket));

    for (uint32_t i = 0; i < count; i++)
        bucket[s[i]]++;

    for (uint32_t i = 0; i < BUCKET_SIZE; i++) {
        if (bucket[i]) {
            uint64_t p = bucket[i];
            p *= LOG2_ARG_SHIFT / count;
            entropy_sum += -p * log2_lshift16(p);
        }
    }

    entropy_sum /= LOG2_ARG_SHIFT;
    return entropy_sum * 100.0 / entropy_max;
}
