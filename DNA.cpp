#include <iostream>
#include <cstring>
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


int main() {
    srand((unsigned int)time(NULL)); 
    char final[sl], seed[sl], p1[sl], p2[sl], p3[sl], p4[sl]; 
    char b1[sl], b2[sl]; 
    getinp(final);
