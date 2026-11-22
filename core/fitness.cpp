#include "fitness.hpp"
#include <algorithm>

double scorePostureFitness(const PhysicsWorld& w) {
    double dx      = std::max(0.0, w.bestX - w.startX);
    double finalDx = std::max(0.0, w.finalX - w.startX);
    double height  = std::max(0.0, w.startY - w.minY);
    double dur     = (w.duration > 0.0) ? w.duration : 7.0;

    auto timeScoreBonus = [&](double maxBonus) -> double {
        if (w.timeToGoal < 0.0 || w.timeToGoal > dur) return 0.0;
        return std::max(0.0, (dur - w.timeToGoal) / dur) * maxBonus;
    };

    if (w.task == TASK_JUMP) {
        double posture = clampVal(1.0 - std::max(0.0, w.tiltAvg - 0.45) * 0.4, 0.35, 1.0);
        double timeBonus = timeScoreBonus(100.0);
        return (height * 2.0 + finalDx * 0.08 + timeBonus) * posture;
    }
    if (w.task == TASK_CLIMB) {
        double climbHeight = std::min(192.0, height);
        double posture = clampVal(1.0 - std::max(0.0, w.tiltAvg - 0.52) * 0.60, 0.20, 1.0);
        double progress = dx * 0.30 + finalDx * 0.70 + climbHeight * 0.60;
        double timeBonus = timeScoreBonus(150.0);
        return (progress + timeBonus) * posture;
    }
    if (w.task == TASK_CUSTOM) {
        double initD  = std::max(20.0, vecLen({w.startX - w.goalX, w.startY - w.goalY}));
        double finalD = vecLen({w.finalX - w.goalX, w.finalY - w.goalY});
        double progress = std::max(0.0, initD - w.bestDistToGoal) * 0.75 + std::max(0.0, initD - finalD) * 0.25;
        double bonus   = (w.bestDistToGoal <= 45.0) ? (250.0 + (45.0 - w.bestDistToGoal) * 4.0) : 0.0;
        double timeBonus = timeScoreBonus(200.0);
        double posture = clampVal(1.0 - std::max(0.0, w.tiltAvg - 0.40) * 0.50, 0.25, 1.0);
        return (progress + bonus + timeBonus) * posture;
    }

    double posture = clampVal(1.0 - std::max(0.0, w.tiltAvg - 0.38) * 0.55, 0.18, 1.0);
    double timeBonus = timeScoreBonus(150.0);
    return (dx * 0.75 + finalDx * 0.25 + timeBonus) * posture;
}
