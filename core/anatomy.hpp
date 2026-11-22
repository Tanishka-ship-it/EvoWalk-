#ifndef EVOWALK_CORE_ANATOMY_HPP
#define EVOWALK_CORE_ANATOMY_HPP

#include <cmath>
#include <algorithm>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

inline Vec2   vecAdd(Vec2 a, Vec2 b)     { return {a.x + b.x, a.y + b.y}; }
inline Vec2   vecSub(Vec2 a, Vec2 b)     { return {a.x - b.x, a.y - b.y}; }
inline Vec2   vecScale(Vec2 a, double s) { return {a.x * s, a.y * s}; }
inline double vecLen(Vec2 a)             { return std::sqrt(a.x * a.x + a.y * a.y); }

inline double clampVal(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

inline double wrapAngle(double a) {
    while (a >  M_PI) a -= 2.0 * M_PI;
    while (a < -M_PI) a += 2.0 * M_PI;
    return a;
}

struct Joint {
    Vec2   pos;
    Vec2   prevPos;
    double radius  = 7.5;
    double mass    = 1.0;
    double invMass = 1.0;
    bool   contact = false;
};

struct Bone {
    int    jointA;
    int    jointB;
    double restLength;
};

struct Muscle {
    int    boneA;
    int    boneB;
    double restLength;
    double strength   = 1.0;
    double activation = 0.0;
};

struct Brace {
    int    jointA;
    int    jointB;
    double restLength;
    double stiffness = 0.085;
};

struct TerrainSeg {
    double x0, x1, y;
};

enum TaskType { TASK_RUN = 0, TASK_JUMP = 1, TASK_CLIMB = 2, TASK_CUSTOM = 3 };

struct SensorSignals {
    double tilt    = 0.0;
    double speed   = 0.0;
    double contact = 0.0;
    double vert    = 0.0;
};

struct MuscleGene {
    double bias    = 0.04;
    double amp     = 0.58;
    double phase   = 0.0;
    double tilt    = 0.0;
    double speed   = 0.0;
    double contact = 0.0;
    double vert    = 0.0;

    double activate(double freq, double t, const SensorSignals& s) const {
        double wave = std::sin(2.0 * M_PI * (freq * t + phase));
        double raw = bias + amp * wave
                   + tilt * s.tilt + speed * s.speed
                   + contact * s.contact + vert * s.vert;
        return clampVal(raw, -1.0, 1.0);
    }
};

inline Joint makeJoint(double x, double y, double r, double m) {
    Joint j;
    j.pos = {x, y};
    j.prevPos = {x, y};
    j.radius = r;
    j.mass = m;
    j.invMass = (m > 0.0) ? (1.0 / m) : 0.0;
    return j;
}

struct BodyBlueprint {
    std::vector<Joint>  joints;
    std::vector<Bone>   bones;
    std::vector<Muscle> muscles;

    static BodyBlueprint runnerPreset() {
        BodyBlueprint bp;
        bp.joints = {
            makeJoint(235, 382, 9.5, 1.55),
            makeJoint(330, 382, 9.5, 1.55),
            makeJoint(390, 360, 8.5, 0.80),
            makeJoint(350, 450, 7.5, 0.80),
            makeJoint(355, 512, 8.0, 0.70),
            makeJoint(215, 450, 7.5, 0.80),
            makeJoint(205, 512, 8.0, 0.70),
            makeJoint(180, 375, 7.0, 0.55)
        };

        int bonePairs[7][2] = {{0,1}, {1,2}, {0,7}, {1,3}, {3,4}, {0,5}, {5,6}};
        for (int i = 0; i < 7; i++) {
            int a = bonePairs[i][0], b = bonePairs[i][1];
            double len = vecLen(vecSub(bp.joints[a].pos, bp.joints[b].pos));
            bp.bones.push_back({a, b, len});
        }

        int musclePairs[4][2] = {{0,3}, {3,4}, {0,5}, {5,6}};
        for (int i = 0; i < 4; i++) {
            int bA = musclePairs[i][0], bB = musclePairs[i][1];
            Vec2 midA = vecScale(vecAdd(bp.joints[bp.bones[bA].jointA].pos, bp.joints[bp.bones[bA].jointB].pos), 0.5);
            Vec2 midB = vecScale(vecAdd(bp.joints[bp.bones[bB].jointA].pos, bp.joints[bp.bones[bB].jointB].pos), 0.5);
            bp.muscles.push_back({bA, bB, vecLen(vecSub(midA, midB)), 1.0, 0.0});
        }
        return bp;
    }
};

#endif
