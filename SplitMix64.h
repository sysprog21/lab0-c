#include <stdint.h>
#include <time.h>

static uint64_t seed;

uint64_t next()
{
    uint64_t z = (seed += 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

uint64_t splitmix64(void)
{
    seed = time(NULL);
    return next();
}