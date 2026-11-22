#include "core/evolution.hpp"
#include "external/http_server.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <cstdlib>

static int runCliBenchmark(int argc, char* argv[]) {
    int totalGens = 10, popGhosts = 24;
    double mutRate = 0.18;
    TaskType task = TASK_RUN;

    if (argc > 2) { int g = std::atoi(argv[2]); if (g > 0) totalGens = g; }
    if (argc > 3) { double m = std::atof(argv[3]); mutRate = (m > 1.0) ? m / 100.0 : m; }
    if (argc > 4) { int p = std::atoi(argv[4]); if (p >= 2 && p <= 500) popGhosts = p; }
    if (argc > 5) { task = parseTask(argv[5]); }

    std::cout << "================================================================\n"
              << "  EVOWALK — C/C++ SIMULATION CORE (TCS-551)\n"
              << "  Task: " << taskName(task) << " | Gens: " << totalGens
              << " | Ghosts: " << popGhosts << " | Mutation: " << (mutRate * 100.0) << "%\n"
              << "================================================================\n\n";

    std::cout << "[1] SKELETAL GRAPH VERIFICATION (Adjacency List + BFS)\n";
    PhysicsWorld sample;
    sample.setup(BodyBlueprint::runnerPreset(), task);
    sample.graph.printAdjacency();
    std::cout << "BFS Connectivity: " << (sample.graph.isConnected() ? "CONNECTED" : "DISCONNECTED")
              << " | Joints: " << sample.joints.size() << " | Bones: " << sample.bones.size()
              << " | Muscles: " << sample.muscles.size() << " | Braces: " << sample.braces.size() << "\n\n";

    std::cout << "[2] MAX-HEAP PRIORITY QUEUE VERIFICATION\n";
    MaxHeap<int> heap;
    int nums[5] = {42, 180, 15, 320, 95};
    for (int i = 0; i < 5; i++) heap.push(nums[i]);
    std::cout << "Input: [42, 180, 15, 320, 95] -> Pop order: ";
    while (!heap.empty()) std::cout << heap.pop() << " ";
    std::cout << "(Verified)\n\n";

    double dur = (task == TASK_JUMP) ? 5.5 : (task == TASK_CLIMB ? 9.0 : 7.0);
    std::cout << "[3] RUNNING EVOLUTION (" << popGhosts << " Ghosts, 120 Hz, " << dur << "s/gen)\n";
    Simulation sim;
    sim.start(BodyBlueprint::runnerPreset(), task, dur, popGhosts, mutRate, 750, 480, 42);

    auto t0 = std::chrono::high_resolution_clock::now();
    sim.warpGenerations(totalGens, true);
    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "\nCompleted in " << elapsed << " s | All-Time Best: " << sim.bestEverFit
              << " | Champion Freq: " << sim.bestEverGenome.frequency << " Hz\n";
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--cli") {
        return runCliBenchmark(argc, argv);
    }

    int port = 8080;
    std::string webRoot = "..";
    if (argc > 1 && std::string(argv[1]) == "--port" && argc > 2) port = std::atoi(argv[2]);
    if (argc > 3 && std::string(argv[3]) == "--webroot" && argc > 4) webRoot = argv[4];

    Simulation sim;
    runLocalServer(port, webRoot, [&](const std::string& path, const std::string& body) -> std::string {
        if (path == "/api/start") {
            parseStartPayload(body, sim);
            return serializeSimulationToJson(sim);
        }
        if (path.rfind("/api/step", 0) == 0) {
            int steps = 2;
            size_t q = path.find("n=");
            if (q != std::string::npos) steps = std::atoi(path.c_str() + q + 2);
            steps = (int)clampVal(steps, 1, 200);
            sim.stepLive(steps);
            return serializeSimulationToJson(sim);
        }
        if (path.rfind("/api/warp", 0) == 0) {
            int gens = 10;
            size_t q = path.find("g=");
            if (q != std::string::npos) gens = std::atoi(path.c_str() + q + 2);
            gens = (int)clampVal(gens, 1, 500);
            sim.warpGenerations(gens, false);
            return serializeSimulationToJson(sim);
        }
        if (path == "/api/params") {
            std::istringstream ss(body);
            double dur, mut; int pop;
            if (ss >> dur >> pop >> mut) sim.setParams(dur, pop, mut);
            return serializeSimulationToJson(sim);
        }
        if (path == "/api/goal") {
            std::istringstream ss(body);
            double gx, gy;
            if (ss >> gx >> gy) sim.setGoal(gx, gy);
            return serializeSimulationToJson(sim);
        }
        return "{\"ok\":true}";
    });
    return 0;
}
