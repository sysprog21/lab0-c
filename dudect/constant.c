#include "constant.h"
#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cpucycles.h"
#include "queue.h"
#include "random.h"

#define N_MEASURE 150

/* Allow random number range from 0 to 65535 */
const size_t chunk_size = 16;

/* Number of measurements per test */
const size_t n_measure = N_MEASURE;

const int drop_size = 20;

/* Maintain a queue independent from the qtest since
 * we do not want the test to affect the original functionality
 */
static struct list_head *l = NULL;

static char random_string[N_MEASURE][8];
static int random_string_iter = 0;

enum {
    test_insert_head,
    test_insert_tail,
    test_remove_head,
    test_remove_tail,
};

/* Implement the necessary queue interface to simulation */
void init_dut(void)
{
    l = NULL;
}

char *get_random_string(void)
{
    random_string_iter = (random_string_iter + 1) % N_MEASURE;
    return random_string[random_string_iter];
}

void prepare_inputs(uint8_t *input_data, uint8_t *classes)
{
    randombytes(input_data, n_measure * chunk_size);
    for (size_t i = 0; i < n_measure; i++) {
        classes[i] = randombit();
        if (classes[i] == 0)
            memset(input_data + (size_t) i * chunk_size, 0, chunk_size);
    }

    for (size_t i = 0; i < N_MEASURE; ++i) {
        /* Generate random string */
        randombytes((uint8_t *) random_string[i], 7);
        random_string[i][7] = 0;
    }
}

void measure(int64_t *before_ticks,
             int64_t *after_ticks,
             uint8_t *input_data,
             int mode)
{
    assert(mode == test_insert_head || mode == test_insert_tail ||
           mode == test_remove_head || mode == test_remove_tail);

    switch (mode) {
    case test_insert_head:
        for (size_t i = drop_size; i < n_measure - drop_size; i++) {
            char *s = get_random_string();
            dut_new();
            dut_insert_head(
                get_random_string(),
                *(uint16_t *) (input_data + i * chunk_size) % 10000);
            before_ticks[i] = cpucycles();
            dut_insert_head(s, 1);
            after_ticks[i] = cpucycles();
            dut_free();
        }
        break;
    case test_insert_tail:
        for (size_t i = drop_size; i < n_measure - drop_size; i++) {
            char *s = get_random_string();
            dut_new();
            dut_insert_head(
                get_random_string(),
                *(uint16_t *) (input_data + i * chunk_size) % 10000);
            before_ticks[i] = cpucycles();
            dut_insert_tail(s, 1);
            after_ticks[i] = cpucycles();
            dut_free();
        }
        break;
    case test_remove_head:
        for (size_t i = drop_size; i < n_measure - drop_size; i++) {
            dut_new();
            dut_insert_head(
                get_random_string(),
                *(uint16_t *) (input_data + i * chunk_size) % 10000);
            before_ticks[i] = cpucycles();
            element_t *e = q_remove_head(l, NULL, 0);
            after_ticks[i] = cpucycles();
            if (e)
                q_release_element(e);
            dut_free();
        }
        break;
    case test_remove_tail:
        for (size_t i = drop_size; i < n_measure - drop_size; i++) {
            dut_new();
            dut_insert_head(
                get_random_string(),
                *(uint16_t *) (input_data + i * chunk_size) % 10000);
            before_ticks[i] = cpucycles();
            element_t *e = q_remove_tail(l, NULL, 0);
            after_ticks[i] = cpucycles();
            if (e)
                q_release_element(e);
            dut_free();
        }
        break;
    default:
        for (size_t i = drop_size; i < n_measure - drop_size; i++) {
            dut_new();
            dut_insert_head(
                get_random_string(),
                *(uint16_t *) (input_data + i * chunk_size) % 10000);
            before_ticks[i] = cpucycles();
            dut_size(1);
            after_ticks[i] = cpucycles();
            dut_free();
        }
    }
}
