// dna.cpp — Genetic Algorithm: fitness-proportional parent selection.
//
// Extracted from DNA.txt (share_calc + its dependency find_best).
// NOTE: this is a fragment, not a standalone program. It has no main()
// and is not compilable on its own — see "Integration" notes at the bottom.

// Builds a fitness-proportional "roulette wheel" as a flat array.
// Each individual i gets `part` slots in share[], where part is its
// fitness share scaled to the array size. select_parent() then does
//   return share[rand() % pop_size];
// to pick a parent. Slots left over after integer-division rounding are
// filled with the single best individual.
//
// Preconditions:
//   - fitness_tally[] holds pop_size fitness scores
//   - share[] must be sized at least sizeofsharecalc
void share_calc(int fitness_tally[], int pop_size, int share[], int sizeofsharecalc) {
    // firstly, getting the total fitness value for calculation and if the ting got no fiting then give it 1 cuz why not, das the human way.
    int total_fitness = 0;
    for (int i = 0; i < pop_size; i++) {
        if (fitness_tally[i] == 0) {
            total_fitness += 1;
        } else {
            total_fitness += fitness_tally[i];
        }
    }
    // here we should have total fitness value

    // ngl ye algo copy ki h maine genetic ke resources se.
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
    // sorting the deal with rest unfilled slots in share
    int bestfromtally = find_best(fitness_tally, pop_size);
    while (slot < sizeofsharecalc) {
        share[slot] = bestfromtally;
        slot++;
        // the last unfilled ones will be filled with fuckass best parent with index from find_best
    }
}

// Returns the index of the highest-scoring individual.
// Required by share_calc; kept here so this fragment stays self-contained.
int find_best(int fitness_tally[], int pop_size) {
    int best = 0;
    for (int i = 1; i < pop_size; i++) {
        if (fitness_tally[i] > fitness_tally[best]) {
            best = i;
        }
    }
    return best;
}

// Integration notes (from DNA.txt):
//   share_calc(...) is called as:
//       int share[population_size];
//       share_calc(fitness_tally, population_size, share, population_size);
//   so share[] is sized to match sizeofsharecalc. Mismatched sizes
//   write out of bounds.
//
//   Selection-pressure caveat: with population_size = 150 and most
//   individuals scoring 0-2, (fitness+1)*150/total_fitness floors to 0
//   for the weak majority, so they are dropped immediately and the
//   remainder pad hands most slots to find_best(). It converges, but
//   behaves closer to greedy hill-climbing than textbook proportional
//   selection. Carrying the fractional remainder across individuals
//   instead of truncating each one would fix that.
