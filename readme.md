# EvoWalk — Modular C++ Engine Build

## Two-Section C++ Architecture (`sim/cpp/`)

### Section 1: `sim/cpp/core/` (Project Core Implementation)
Contains the Genetic Algorithm, Creature Anatomy, Posture-Weighted Fitness, and both custom Data Structures:
- `core/anatomy.hpp`: Creature anatomy structs (`Vec2`, `Joint`, `Bone`, `Muscle`, `Brace`, `MuscleGene`, `SensorSignals`, `BodyBlueprint`) and `runnerPreset()`.
- `core/dsa.hpp`: Custom `MaxHeap<T>` (Priority Queue) and `SkeletalGraph` (Adjacency List + BFS `isConnected()` + `buildSkeletalGraphAndBraces()`).
- `core/fitness.hpp` & `core/fitness.cpp`: Posture-weighted fitness evaluator (`scorePostureFitness()`) for RUN, JUMP, CLIMB, and CUSTOM tasks.
- `core/evolution.hpp` & `core/evolution.cpp`: Genetic Algorithm (`Genome::random`, `Genome::crossover`, `Genome::mutate`, `Simulation::selectTournament`, `Simulation::finishGeneration`, `stepLive`, `warpGenerations`) calling into `external/` for physics simulation.

### Section 2: `sim/cpp/external/` (Open-Source / Utility Modules)
Contains the 2D Verlet physics solver and local HTTP server utilities invoked by Section 1:
- `external/verlet_physics.hpp` & `external/verlet_physics.cpp`: 120 Hz Verlet integration, PD balance reflex, bone/brace/muscle/terrain solvers, sensor normalization, and task parsing.
- `external/http_server.hpp`: Zero-dependency Winsock/POSIX localhost HTTP server, payload parser, and JSON serializer.

### Entry Point & Thin Frontend
- `sim/cpp/main.cpp`: Wires `core/` and `external/` together for live browser mode (`http://localhost:8080`) or `--cli` benchmark mode.
- `sim/index.html`, `sim/styles.css`, `sim/ui.js`: Presentation-only web canvas UI.

## How to Run
- **Windows:** Double-click `run_evowalk.bat` (or `sim/cpp/build.bat`).
- **Linux / macOS:**
  ```bash
  cd sim/cpp
  make
  ./evowalk                       # Live server on http://localhost:8080
  ./evowalk --cli 10 18 24 run    # Terminal DSA verification + GA benchmark
  ```
