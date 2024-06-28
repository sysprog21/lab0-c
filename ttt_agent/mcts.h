#pragma once

#define ITERATIONS 100000
#define EXPLORATION_FACTOR sqrt_f(LOAD_FIXED(2L))

int mcts(char *table, char player);
