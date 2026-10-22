#include <iostream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

using namespace std;
const int max_length = 67;
const int sl = 68; 
const int population_size = 150; 
const int chill = 30;
const double mutation_rate = 0.05;
string sample_set = "0123456789 QWERTYUIOPASDFGHJKLZXCVBNMqwertyuiopasdfghjklzxcvbnm.,/-:";
int length;

void getinp(char final[]) {
    string input;
    cout << "Enter a " << max_length << " length target string (digits + spaces)[c++ varient]: ";
    if (!getline(cin, input)) {
        cout << "Input error." << endl;
        return;
    }

    int input_length = static_cast<int>(input.length());

    int copy_length = input_length;
    if (copy_length > max_length)
        copy_length = max_length;
  
    for (int i = 0; i < copy_length; i++) {
        final[i] = input[i];
    }
    final[copy_length] = '\0'; 
    length = copy_length;
}
void generate_random(char placeholder[]) {
    for (int i = 0; i < length; i++) {
        placeholder[i] = sample_set[rand() % sl];
    }
}
    placeholder[length] = '\0';
int fitness_score(char final[], char seed[]) {
    int a = 0;
    for (int i = 0; i < length; i++) {
        if (final[i] == seed[i]) {
            a++;
        }
    }
    return a;
}
void delay_ms(int ms) 
{ 
    clock_t start = clock();
    double target_ticks = (ms / 1000.0) * CLOCKS_PER_SEC;
    while ((double)(clock() - start) < target_ticks) {
    }
}

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

int select_parent(int share[], int pop_size) {
    return share[rand() % pop_size];
}

void crossover_single_point(char parentA[], char parentB[], char child[]) {
    int cut_point = rand() % length;
    for (int i = 0; i < length; i++) {
        if (i < cut_point) {
            child[i] = parentA[i];
        }
        else {
            child[i] = parentB[i];
        }
}

child[length] = '\0';
}
void mutate(char individual[]) {
    for (int i = 0; i < length; i++) {
        double roll = (double)rand() / RAND_MAX;
        if (roll < mutation_rate) {
            individual[i] = sample_set[rand() % sl];
        }
    }
}

int main() {
    srand((unsigned int)time(NULL)); 

    char final[sl], seed[sl], p1[sl], p2[sl], p3[sl], p4[sl]; 
    char b1[sl], b2[sl]; 

    getinp(final);

    char population[population_size][sl]; // 2d array containing all the seeds.
    int fitness_tally[population_size];
    for (int i = 0; i < population_size; i++) {
    generate_random(population[i]);
    fitness_tally[i] = fitness_score(final, population[i]);
    }
    int share[population_size];
    int generation = 0;

    while (true) {
        int best = find_best(fitness_tally, population_size);
        printf("Gen %-3d | Input: %-12s | Best: %-12s | Fitness: %2d/%d\n", generation, final, population[best], fitness_tally[best], length);
        delay_ms(chill);
        if (fitness_tally[best] == length) {
            cout << "\nSOLVED in " << generation << " generations!" << endl;
        break;
        }
        share_calc(fitness_tally, population_size, share, population_size);
        char new_population[population_size][max_length + 1];
        for (int i = 0; i < population_size; i++) {
            int a = select_parent(share, population_size);
            int b = select_parent(share, population_size);
            crossover_single_point(population[a], population[b],
            new_population[i]);
            mutate(new_population[i]);
        }
        for (int i = 0; i < population_size; i++) {
            strcpy(population[i], new_population[i]);
            fitness_tally[i] = fitness_score(population[i], final);
        }
        generation++;
    }
    return 0;
}
