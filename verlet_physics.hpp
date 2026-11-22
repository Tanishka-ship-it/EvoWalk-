#ifndef EVOWALK_EXTERNAL_VERLET_PHYSICS_HPP
#define EVOWALK_EXTERNAL_VERLET_PHYSICS_HPP

#include "../core/anatomy.hpp"
#include "../core/dsa.hpp"
#include <vector>
#include <string>

class PhysicsWorld {
public:
    static const double GRAVITY;
    static const double AIR_DAMP;
    static const double DEFAULT_DT;

    std::vector<Joint>      joints;
    std::vector<Bone>       bones;
    std::vector<Muscle>     muscles;
    std::vector<Brace>      braces;
    std::vector<TerrainSeg> terrain;
    SkeletalGraph           graph;

    TaskType task     = TASK_RUN;
    double   friction = 0.78;
    double   goalX    = 860.0;
    double   goalY    = 480.0;

    double simTime = 0.0;
    double startX = 0.0, startY = 0.0;
    double prevX = 0.0,  prevY = 0.0;
    double bestX = 0.0,  minY = 1e9;
    double finalX = 0.0, finalY = 0.0;
    double bestDistToGoal = 1e9;
    double coreAngle = 0.0, startCoreAngle = 0.0, prevTilt = 0.0;
    double tiltSum = 0.0, tiltAvg = 0.0;
    double speedX = 0.0,  speedY = 0.0;
    int    contacts = 0;
    double fitness = 0.0;
    double duration = 7.0;
    double timeToGoal = -1.0;

    PhysicsWorld();
    void setup(const BodyBlueprint& body, TaskType t, double gx = 750.0, double gy = 480.0, double dur = 7.0);
    void step(const std::vector<double>& activations, double dt = DEFAULT_DT);

    Vec2          centroid() const;
    SensorSignals sensors() const;
    double        surfaceY(double x) const;

private:
    void   configureTask(TaskType t, double gx, double gy);
    double computeCoreAngle() const;
    void   solveBones();
    void   solveBraces();
    void   solveMuscles();
    void   collideTerrain();
};

TaskType parseTask(const std::string& name);
const char* taskName(TaskType t);

#endif
