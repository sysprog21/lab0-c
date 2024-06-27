#pragma once

#include "game.h"

static inline int eval_line_segment_score(const char *table,
                                          char player,
                                          int i,
                                          int j,
                                          line_t line)
{
    int score = 0;
    for (int k = 0; k < GOAL; k++) {
        char curr =
            table[GET_INDEX(i + k * line.i_shift, j + k * line.j_shift)];
        if (curr == player) {
            if (score < 0)
                return 0;
            if (score)
                score *= 10;
            else
                score = 1;
        } else if (curr != ' ') {
            if (score > 0)
                return 0;
            if (score)
                score *= 10;
            else
                score = -1;
        }
    }
    return score;
}

static inline int get_score(const char *table, char player)
{
    int score = 0;
    for (int i_line = 0; i_line < 4; ++i_line) {
        line_t line = lines[i_line];
        for (int i = line.i_lower_bound; i < line.i_upper_bound; ++i) {
            for (int j = line.j_lower_bound; j < line.j_upper_bound; ++j) {
                score += eval_line_segment_score(table, player, i, j, line);
            }
        }
    }
    return score;
}
