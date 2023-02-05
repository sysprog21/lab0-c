#ifndef DUDECT_TTEST_H
#define DUDECT_TTEST_H

#include <stdint.h>

typedef struct {
    double mean[2];
    double m2[2];
    double n[2];
} t_context_t;

void t_push(t_context_t *ctx, double x, uint8_t class);
double t_compute(t_context_t *ctx);
void t_init(t_context_t *ctx);

#endif
