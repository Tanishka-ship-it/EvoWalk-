#ifndef EVOWALK_CORE_EVOLUTION_HPP
#define EVOWALK_CORE_EVOLUTION_HPP

#include "anatomy.hpp"
#include "dsa.hpp"
#include "fitness.hpp"
#include "../external/verlet_physics.hpp"
#include <vector>
#include <random>

struct Genome {
    double frequency = 1.25;
    std::vector<MuscleGene> muscles;

    static Genome random(int muscleCount, TaskType task, std::mt19937& rng);
    static Genome crossover(const Genome& a, const Genome& b, std::mt19937& rng);
    void mutate(double rate, std::mt19937& rng);
};

struct RankedAgent {
    int    index   = -1;
    double fitness = 0.0;
    bool operator<(const RankedAgent& o) const { return fitness < o.fitness; }
};

struct GenStats {
    int    gen;
    double best;
    double avg;
};

class Simulation {
public:
    BodyBlueprint             blueprint;
    TaskType                  task         = TASK_RUN;
    double                    duration     = 7.0;
    int                       popSize      = 24;
    int                       eliteCount   = 3;
    double                    mutationRate = 0.18;
    double                    goalX        = 750.0;
    double                    goalY        = 480.0;

    int                       generation   = 1;
    double                    genTime      = 0.0;
    double                    bestEverFit  = 0.0;
    int                       bestEverGen  = 0;
    Genome                    bestEverGenome;
    std::vector<GenStats>     history;

    std::vector<Genome>       genomes;
    std::vector<PhysicsWorld> worlds;
    MaxHeap<RankedAgent>      rankingHeap;
    std::mt19937              rng;

    Simulation();

    void start(const BodyBlueprint& body, TaskType t, double dur, int pop, double mutRate,
               double gx, double gy, unsigned int seed = 42);
    void setParams(double dur, int pop, double mutRate);
    void setGoal(double gx, double gy);

    bool stepLive(int steps);
    void warpGenerations(int count, bool verbose = false);

private:
    void spawnWorlds();
    void finishGeneration(bool verbose = false);
    const Genome& selectTournament(const std::vector<double>& fits, int k = 3);
};

#endif
