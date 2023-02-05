#ifndef DUDECT_CPUCYCLES_H
#define DUDECT_CPUCYCLES_H

#include <stdint.h>

// http://www.intel.com/content/www/us/en/embedded/training/ia-32-ia-64-benchmark-code-execution-paper.html
static inline int64_t cpucycles(void)
{
#if defined(__i386__) || defined(__x86_64__)
    unsigned int hi, lo;
    __asm__ volatile("rdtsc\n\t" : "=a"(lo), "=d"(hi));
    return ((int64_t) lo) | (((int64_t) hi) << 32);

#elif defined(__aarch64__)
    uint64_t val;
    /* According to ARM DDI 0487F.c, from Armv8.0 to Armv8.5 inclusive, the
     * system counter is at least 56 bits wide; from Armv8.6, the counter
     * must be 64 bits wide.  So the system counter could be less than 64
     * bits wide and it is attributed with the flag 'cap_user_time_short'
     * is true.
     */
    asm volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
#else
#error Unsupported Architecture
#endif
}

#endif
