#include <stdint.h>
// http://www.intel.com/content/www/us/en/embedded/training/ia-32-ia-64-benchmark-code-execution-paper.html
inline int64_t cpucycles(void)
{
#if defined(__i386__) || defined(__x86_64__)
    unsigned int hi, lo;
    __asm__ volatile("rdtsc\n\t" : "=a"(lo), "=d"(hi));
    return ((int64_t) lo) | (((int64_t) hi) << 32);
#else
#error Unsupported Architecture
#endif
}
