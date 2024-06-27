#pragma once

#include <stdint.h>

#include "game.h"
#include "hlist.h"

#define HASH_TABLE_SIZE ((int) 1e6 + 3)  // choose a large prime number

extern uint64_t zobrist_table[N_GRIDS][2];

typedef struct {
    uint64_t key;
    int score;
    int move;
    struct hlist_node ht_list;
} zobrist_entry_t;

void zobrist_init(void);
zobrist_entry_t *zobrist_get(uint64_t key);
void zobrist_put(uint64_t key, int score, int move);
void zobrist_clear(void);
void zobrist_destroy_table(void);
