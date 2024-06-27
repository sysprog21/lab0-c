#pragma once

typedef struct {
    int score, move;
} move_t;

void negamax_init();
move_t negamax_predict(char *table, char player);
