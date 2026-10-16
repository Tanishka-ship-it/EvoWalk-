#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>

using namespace std;

void share_calc(int fitness_tally[], int pop_size, int share[], int sizeofsharecalc) {
    int total_fitness = 0;
    for (int i = 0; i < pop_size; i++) {
        if (fitness_tally[i] == 0) {
            total_fitness += 1;
        } else {
            total_fitness += fitness_tally[i];
        }
    }

    int slot = 0;
    for (int i = 0; i < pop_size; i++) {
        int part = ((fitness_tally[i] + 1) * sizeofsharecalc) / total_fitness;
        for (int c = 0; c < part; c++) {
            if (slot < sizeofsharecalc) {
                share[slot] = i;
                slot++;
            }
        }
    }

    int bestfromtally = find_best(fitness_tally, pop_size);
    while (slot < sizeofsharecalc) {
        share[slot] = bestfromtally;
        slot++;
    }
}

int find_best(int fitness_tally[], int pop_size) {
    int best = 0;
    for (int i = 1; i < pop_size; i++) {
        if (fitness_tally[i] > fitness_tally[best]) {
            best = i;
        }
    }
    return best;
}
