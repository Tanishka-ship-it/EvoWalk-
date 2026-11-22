#include "evolution.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>

Genome Genome::random(int muscleCount, TaskType task, std::mt19937& rng) {
    std::uniform_real_distribution<double> delta(-0.5, 0.5);
    double baseFreq = (task == TASK_JUMP || task == TASK_CLIMB) ? 1.0 : 1.25;
    double baseBias = (task == TASK_JUMP) ? 0.10 : (task == TASK_CLIMB ? 0.08 : 0.04);
    double baseAmp  = (task == TASK_JUMP) ? 0.82 : (task == TASK_CLIMB ? 0.70 : 0.58);

    Genome g;
    g.frequency = clampVal(baseFreq + delta(rng) * 0.35, 0.6, 2.2);
    for (int i = 0; i < muscleCount; i++) {
        MuscleGene mg;
        mg.bias    = clampVal(baseBias + delta(rng) * 0.22, -0.3, 0.65);
        mg.amp     = clampVal(baseAmp  + delta(rng) * 0.30, 0.18, 1.05);
        double basePhase = (task == TASK_JUMP) ? 0.0 : ((i == 1 || i == 2) ? 0.5 : (i % 2) * 0.5);
        mg.phase   = std::fmod(basePhase + delta(rng) * 0.16 + 1.0, 1.0);
        mg.tilt    = delta(rng) * 0.35;
        mg.speed   = delta(rng) * 0.30;
        mg.contact = delta(rng) * 0.30;
        mg.vert    = delta(rng) * 0.30;
        g.muscles.push_back(mg);
    }
    return g;
}

Genome Genome::crossover(const Genome& a, const Genome& b, std::mt19937& rng) {
    int n = (int)std::min(a.muscles.size(), b.muscles.size());
    std::uniform_int_distribution<int> cutDist(0, 1 + n * 7);
    int cut = cutDist(rng);

    Genome child;
    child.frequency = (0 < cut) ? a.frequency : b.frequency;
    int idx = 1;
    for (int i = 0; i < n; i++) {
        const MuscleGene& am = a.muscles[i];
        const MuscleGene& bm = b.muscles[i];
        MuscleGene cm;
        cm.bias    = (idx++ < cut) ? am.bias    : bm.bias;
        cm.amp     = (idx++ < cut) ? am.amp     : bm.amp;
        cm.phase   = (idx++ < cut) ? am.phase   : bm.phase;
        cm.tilt    = (idx++ < cut) ? am.tilt    : bm.tilt;
        cm.speed   = (idx++ < cut) ? am.speed   : bm.speed;
        cm.contact = (idx++ < cut) ? am.contact : bm.contact;
        cm.vert    = (idx++ < cut) ? am.vert    : bm.vert;
        child.muscles.push_back(cm);
    }
    return child;
}

void Genome::mutate(double rate, std::mt19937& rng) {
    std::uniform_real_distribution<double> prob(0.0, 1.0);
    std::uniform_real_distribution<double> delta(-0.5, 0.5);

    if (prob(rng) < rate * 2.1) {
        frequency = clampVal(frequency + delta(rng) * 0.28, 0.55, 2.3);
    }
    for (size_t i = 0; i < muscles.size(); i++) {
        MuscleGene& m = muscles[i];
        if (prob(rng) < rate * 1.9) m.bias    = clampVal(m.bias    + delta(rng) * 0.16, -0.35, 0.75);
        if (prob(rng) < rate * 2.3) m.amp     = clampVal(m.amp     + delta(rng) * 0.20,  0.12, 1.15);
        if (prob(rng) < rate * 2.1) m.phase   = std::fmod(m.phase  + delta(rng) * 0.14 + 1.0, 1.0);
        if (prob(rng) < rate * 1.4) m.tilt    = clampVal(m.tilt    + delta(rng) * 0.18, -0.9,  0.9);
        if (prob(rng) < rate * 1.4) m.speed   = clampVal(m.speed   + delta(rng) * 0.16, -0.8,  0.8);
        if (prob(rng) < rate * 1.4) m.contact = clampVal(m.contact + delta(rng) * 0.16, -0.8,  0.8);
        if (prob(rng) < rate * 1.4) m.vert    = clampVal(m.vert    + delta(rng) * 0.16, -0.8,  0.8);
    }
}

Simulation::Simulation() : rng(42) {
    blueprint = BodyBlueprint::runnerPreset();
}

void Simulation::start(const BodyBlueprint& body, TaskType t, double dur, int pop,
                       double mutRate, double gx, double gy, unsigned int seed) {
    blueprint    = body;
    task         = t;
    duration     = clampVal(dur, 1.0, 60.0);
    popSize      = (int)clampVal(pop, 2, 100);
    mutationRate = clampVal(mutRate, 0.0, 1.0);
    goalX = gx; goalY = gy;
    rng.seed(seed);

    generation = 1;
    bestEverFit = 0.0;
    bestEverGen = 0;
    history.clear();

    genomes.clear();
    int mCount = (int)blueprint.muscles.size();
    for (int i = 0; i < popSize; i++) {
        genomes.push_back(Genome::random(mCount, task, rng));
    }
    spawnWorlds();
}

void Simulation::setParams(double dur, int pop, double mutRate) {
    duration = clampVal(dur, 1.0, 60.0);
    for (auto& w : worlds) w.duration = duration;
    mutationRate = clampVal(mutRate, 0.0, 1.0);
    int newPop = (int)clampVal(pop, 2, 100);
    if (newPop != popSize) {
        popSize = newPop;
        while ((int)genomes.size() > popSize) { genomes.pop_back(); worlds.pop_back(); }
        while ((int)genomes.size() < popSize) {
            Genome g = genomes.empty() ? Genome::random((int)blueprint.muscles.size(), task, rng) : genomes[0];
            g.mutate(0.45, rng);
            genomes.push_back(g);
            PhysicsWorld w;
            w.setup(blueprint, task, goalX, goalY, duration);
            worlds.push_back(w);
        }
    }
}

void Simulation::setGoal(double gx, double gy) {
    goalX = gx; goalY = gy;
    for (size_t i = 0; i < worlds.size(); i++) {
        worlds[i].goalX = gx;
        worlds[i].goalY = gy;
        worlds[i].bestDistToGoal = 1e9;
    }
}

void Simulation::spawnWorlds() {
    worlds.resize(popSize);
    for (int i = 0; i < popSize; i++) {
        worlds[i].setup(blueprint, task, goalX, goalY, duration);
    }
    genTime = 0.0;
}

bool Simulation::stepLive(int steps) {
    if (worlds.empty()) return false;
    std::vector<double> acts(blueprint.muscles.size(), 0.0);

    for (int s = 0; s < steps; s++) {
        for (int i = 0; i < popSize; i++) {
            SensorSignals sig = worlds[i].sensors();
            const Genome& g = genomes[i];
            for (size_t m = 0; m < g.muscles.size(); m++) {
                acts[m] = g.muscles[m].activate(g.frequency, worlds[i].simTime, sig);
            }
            worlds[i].step(acts, PhysicsWorld::DEFAULT_DT);
            worlds[i].fitness = scorePostureFitness(worlds[i]);
        }
        genTime += PhysicsWorld::DEFAULT_DT;
        if (genTime >= duration) {
            finishGeneration(false);
            return true;
        }
    }
    return false;
}

void Simulation::warpGenerations(int count, bool verbose) {
    int totalSteps = (int)std::round(duration / PhysicsWorld::DEFAULT_DT);
    std::vector<double> acts(blueprint.muscles.size(), 0.0);

    for (int g = 0; g < count; g++) {
        for (int s = 0; s < totalSteps; s++) {
            for (int i = 0; i < popSize; i++) {
                SensorSignals sig = worlds[i].sensors();
                const Genome& geno = genomes[i];
                for (size_t m = 0; m < geno.muscles.size(); m++) {
                    acts[m] = geno.muscles[m].activate(geno.frequency, worlds[i].simTime, sig);
                }
                worlds[i].step(acts, PhysicsWorld::DEFAULT_DT);
                worlds[i].fitness = scorePostureFitness(worlds[i]);
            }
        }
        finishGeneration(verbose);
    }
}

const Genome& Simulation::selectTournament(const std::vector<double>& fits, int k) {
    std::uniform_int_distribution<int> dist(0, popSize - 1);
    int best = dist(rng);
    for (int i = 1; i < k; i++) {
        int cand = dist(rng);
        if (fits[cand] > fits[best]) best = cand;
    }
    return genomes[best];
}

void Simulation::finishGeneration(bool verbose) {
    rankingHeap.clear();
    std::vector<double> fits(popSize, 0.0);
    double sumFit = 0.0;

    for (int i = 0; i < popSize; i++) {
        fits[i] = worlds[i].fitness;
        sumFit += fits[i];
        rankingHeap.push({i, fits[i]});
    }

    int champIdx = rankingHeap.top().index;
    double bestFit = rankingHeap.top().fitness;
    double avgFit  = sumFit / popSize;
    history.push_back({generation, bestFit, avgFit});

    if (bestFit > bestEverFit) {
        bestEverFit    = bestFit;
        bestEverGen    = generation;
        bestEverGenome = genomes[champIdx];
    }

    if (verbose) {
        std::cout << "Gen " << std::setw(2) << generation
                  << " | Best: " << std::setw(6) << std::fixed << std::setprecision(1) << bestFit
                  << " | Avg: "  << std::setw(6) << std::fixed << std::setprecision(1) << avgFit
                  << " | Champion: #" << champIdx
                  << " (Freq: " << std::setprecision(2) << genomes[champIdx].frequency << " Hz)\n";
    }

    std::vector<Genome> nextGen;
    nextGen.reserve(popSize);
    for (int e = 0; e < eliteCount && !rankingHeap.empty(); e++) {
        RankedAgent topAgent = rankingHeap.pop();
        nextGen.push_back(genomes[topAgent.index]);
    }

    while ((int)nextGen.size() < popSize) {
        Genome child = Genome::crossover(selectTournament(fits, 3), selectTournament(fits, 3), rng);
        child.mutate(mutationRate, rng);
        nextGen.push_back(child);
    }

    if (bestEverFit > 0.0 && nextGen.size() > 1) {
        nextGen.back() = bestEverGenome;
    }

    genomes = nextGen;
    generation++;
    spawnWorlds();
}
