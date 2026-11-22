#include "verlet_physics.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>

const double PhysicsWorld::GRAVITY    = 1900.0;
const double PhysicsWorld::AIR_DAMP   = 0.999;
const double PhysicsWorld::DEFAULT_DT = 1.0 / 120.0;

TaskType parseTask(const std::string& name) {
    std::string s = name;
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)std::tolower((unsigned char)s[i]);
    if (s == "jump")   return TASK_JUMP;
    if (s == "climb")  return TASK_CLIMB;
    if (s == "custom") return TASK_CUSTOM;
    return TASK_RUN;
}

const char* taskName(TaskType t) {
    if (t == TASK_JUMP)   return "JUMP";
    if (t == TASK_CLIMB)  return "CLIMB";
    if (t == TASK_CUSTOM) return "CUSTOM";
    return "RUN";
}

PhysicsWorld::PhysicsWorld() {}

void PhysicsWorld::configureTask(TaskType t, double gx, double gy) {
    task = t;
    terrain.clear();
    if (t == TASK_CLIMB) {
        friction = 0.68; goalX = 860.0; goalY = 328.0;
        terrain = {{-10000, 420, 520}, {420, 550, 472}, {550, 680, 424}, {680, 810, 376}, {810, 30000, 328}};
    } else if (t == TASK_JUMP) {
        friction = 0.62; goalX = 0.0; goalY = 0.0;
        terrain = {{-10000, 30000, 520}};
    } else if (t == TASK_CUSTOM) {
        friction = 0.75; goalX = gx; goalY = gy;
        terrain = {{-10000, 30000, 520}};
    } else {
        friction = 0.78; goalX = 860.0; goalY = 520.0;
        terrain = {{-10000, 30000, 520}};
    }
}

void PhysicsWorld::setup(const BodyBlueprint& body, TaskType t, double gx, double gy, double dur) {
    configureTask(t, gx, gy);
    duration = dur;
    timeToGoal = -1.0;
    joints  = body.joints;
    bones   = body.bones;
    muscles = body.muscles;

    double minX = 1e9, maxX = -1e9, maxY = -1e9;
    for (size_t i = 0; i < joints.size(); i++) {
        if (joints[i].pos.x < minX) minX = joints[i].pos.x;
        if (joints[i].pos.x > maxX) maxX = joints[i].pos.x;
        if (joints[i].pos.y > maxY) maxY = joints[i].pos.y;
    }
    double startLineX = (t == TASK_JUMP) ? 250.0 : 230.0;
    double dx = startLineX - (minX + maxX) * 0.5;
    double dy = (surfaceY(startLineX) - 16.0) - maxY;
    for (size_t i = 0; i < joints.size(); i++) {
        joints[i].pos.x += dx; joints[i].pos.y += dy;
        joints[i].prevPos = joints[i].pos;
    }

    buildSkeletalGraphAndBraces(joints, bones, graph, braces);

    Vec2 c = centroid();
    startX = prevX = bestX = finalX = c.x;
    startY = prevY = minY = finalY = c.y;
    bestDistToGoal = vecLen({c.x - goalX, c.y - goalY});
    coreAngle = startCoreAngle = computeCoreAngle();
    prevTilt = 0.0;
    simTime = tiltSum = tiltAvg = speedX = speedY = fitness = 0.0;
    contacts = 0;
}

double PhysicsWorld::surfaceY(double x) const {
    double y = 520.0;
    bool found = false;
    for (size_t i = 0; i < terrain.size(); i++) {
        if (x >= terrain[i].x0 && x <= terrain[i].x1) {
            if (!found || terrain[i].y < y) { y = terrain[i].y; found = true; }
        }
    }
    return y;
}

double PhysicsWorld::computeCoreAngle() const {
    if (bones.empty()) return 0.0;
    double bestScore = -1.0;
    int bestIdx = 0;
    for (size_t i = 0; i < bones.size(); i++) {
        int a = bones[i].jointA, b = bones[i].jointB;
        double score = (joints[a].mass + joints[b].mass) * 1000.0 + vecLen(vecSub(joints[a].pos, joints[b].pos));
        if (score > bestScore) { bestScore = score; bestIdx = (int)i; }
    }
    Vec2 d = vecSub(joints[bones[bestIdx].jointB].pos, joints[bones[bestIdx].jointA].pos);
    return std::atan2(d.y, d.x);
}

Vec2 PhysicsWorld::centroid() const {
    Vec2 sum = {0.0, 0.0};
    for (size_t i = 0; i < joints.size(); i++) sum = vecAdd(sum, joints[i].pos);
    return joints.empty() ? sum : vecScale(sum, 1.0 / joints.size());
}

void PhysicsWorld::solveBones() {
    for (int iter = 0; iter < 8; iter++) {
        for (size_t i = 0; i < bones.size(); i++) {
            Joint& a = joints[bones[i].jointA];
            Joint& b = joints[bones[i].jointB];
            Vec2 delta = vecSub(b.pos, a.pos);
            double d = std::max(vecLen(delta), 1e-4);
            double diff = (d - bones[i].restLength) / d;
            double wa = a.invMass / (a.invMass + b.invMass);
            double wb = 1.0 - wa;
            a.pos = vecAdd(a.pos, vecScale(delta,  diff * wa));
            b.pos = vecAdd(b.pos, vecScale(delta, -diff * wb));
        }
    }
}

void PhysicsWorld::solveBraces() {
    for (size_t i = 0; i < braces.size(); i++) {
        Joint& a = joints[braces[i].jointA];
        Joint& b = joints[braces[i].jointB];
        Vec2 delta = vecSub(b.pos, a.pos);
        double d = std::max(vecLen(delta), 1e-4);
        double corr = clampVal((d - braces[i].restLength) * braces[i].stiffness, -7.0, 7.0);
        double wa = a.invMass / (a.invMass + b.invMass);
        double wb = 1.0 - wa;
        Vec2 dir = vecScale(delta, 1.0 / d);
        a.pos = vecAdd(a.pos, vecScale(dir,  corr * wa));
        b.pos = vecAdd(b.pos, vecScale(dir, -corr * wb));
    }
}

void PhysicsWorld::solveMuscles() {
    for (size_t i = 0; i < muscles.size(); i++) {
        const Muscle& m = muscles[i];
        const Bone& bA = bones[m.boneA];
        const Bone& bB = bones[m.boneB];
        Joint& a1 = joints[bA.jointA]; Joint& a2 = joints[bA.jointB];
        Joint& b1 = joints[bB.jointA]; Joint& b2 = joints[bB.jointB];

        Vec2 midA = vecScale(vecAdd(a1.pos, a2.pos), 0.5);
        Vec2 midB = vecScale(vecAdd(b1.pos, b2.pos), 0.5);
        Vec2 delta = vecSub(midB, midA);
        double d = std::max(vecLen(delta), 1e-4);
        double target = m.restLength * (1.0 - 0.28 * m.activation);
        double corr = clampVal((d - target) * 0.055 * m.strength, -4.0, 4.0);
        Vec2 dir = vecScale(delta, 1.0 / d);

        int shared = (bA.jointA == bB.jointA || bA.jointA == bB.jointB) ? bA.jointA
                   : ((bA.jointB == bB.jointA || bA.jointB == bB.jointB) ? bA.jointB : -1);

        if (shared != -1) {
            Joint& endA = (bA.jointA == shared) ? a2 : a1;
            Joint& endB = (bB.jointA == shared) ? b2 : b1;
            double wa = endA.invMass / (endA.invMass + endB.invMass);
            double wb = 1.0 - wa;
            endA.pos = vecAdd(endA.pos, vecScale(dir,  corr * wa));
            endB.pos = vecAdd(endB.pos, vecScale(dir, -corr * wb));
        } else {
            double invA = (a1.invMass + a2.invMass) * 0.5;
            double invB = (b1.invMass + b2.invMass) * 0.5;
            if (invA + invB <= 1e-5) continue;
            double wa = invA / (invA + invB), wb = 1.0 - wa;
            Vec2 dA = vecScale(dir,  corr * wa * 0.5);
            Vec2 dB = vecScale(dir, -corr * wb * 0.5);
            a1.pos = vecAdd(a1.pos, dA); a2.pos = vecAdd(a2.pos, dA);
            b1.pos = vecAdd(b1.pos, dB); b2.pos = vecAdd(b2.pos, dB);
        }
    }
}

void PhysicsWorld::collideTerrain() {
    contacts = 0;

    if (terrain.size() > 1) {
        for (size_t s = 1; s < terrain.size(); s++) {
            const auto& prev = terrain[s - 1];
            const auto& curr = terrain[s];
            if (curr.y < prev.y && std::abs(curr.x0 - prev.x1) < 5.0) {
                double rx   = curr.x0;
                double yTop = curr.y;
                double yBot = prev.y;

                for (size_t i = 0; i < joints.size(); i++) {
                    Joint& j = joints[i];

                    if (j.pos.y + j.radius > yTop && j.pos.y - j.radius < yBot + 10.0) {
                        if (j.pos.x + j.radius > rx && (j.pos.x - j.radius < rx || j.prevPos.x <= rx)) {
                            j.pos.x = rx - j.radius;
                            double vy = j.pos.y - j.prevPos.y;
                            j.prevPos.x = j.pos.x;
                            j.prevPos.y = j.pos.y - vy * (1.0 - friction * 0.5);
                            j.contact = true;
                            contacts++;
                        }
                    }

                    double dx = j.pos.x - rx;
                    double dy = j.pos.y - yTop;
                    if (dx < 0.0 && dy > 0.0 && dx > -j.radius && dy < j.radius) {
                        double d2 = dx * dx + dy * dy;
                        if (d2 < j.radius * j.radius && d2 > 1e-4) {
                            double d = std::sqrt(d2);
                            double nx = dx / d, ny = dy / d;
                            double overlap = j.radius - d;
                            j.pos.x += nx * overlap;
                            j.pos.y += ny * overlap;
                            double vdotn = (j.pos.x - j.prevPos.x) * nx + (j.pos.y - j.prevPos.y) * ny;
                            if (vdotn < 0.0) {
                                j.prevPos.x += vdotn * nx;
                                j.prevPos.y += vdotn * ny;
                            }
                            j.contact = true;
                            contacts++;
                        }
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < joints.size(); i++) {
        Joint& j = joints[i];
        double groundY = 520.0;
        bool found = false;
        for (size_t s = 0; s < terrain.size(); s++) {
            const auto& seg = terrain[s];
            if (j.pos.x >= seg.x0 && j.pos.x <= seg.x1) {
                if (!found || seg.y < groundY) {
                    if (s > 0 && terrain[s - 1].y > seg.y && j.pos.y > seg.y + j.radius * 1.5) {
                        groundY = terrain[s - 1].y;
                    } else {
                        groundY = seg.y;
                    }
                    found = true;
                }
            }
        }

        if (j.pos.y + j.radius > groundY) {
            double vx = j.pos.x - j.prevPos.x;
            j.pos.y = groundY - j.radius;
            j.prevPos.y = j.pos.y;
            j.prevPos.x = j.pos.x - vx * (1.0 - friction);
            j.contact = true;
            contacts++;
        }
        if (j.pos.x < -1000.0)      { j.pos.x = -1000.0; j.prevPos.x = j.pos.x; }
        else if (j.pos.x > 10000.0) { j.pos.x = 10000.0; j.prevPos.x = j.pos.x; }
    }
}

void PhysicsWorld::step(const std::vector<double>& activations, double dt) {
    double tilt = wrapAngle(coreAngle - startCoreAngle);
    double omega = (tilt - prevTilt) / dt;
    prevTilt = tilt;
    double pdCorr = clampVal(-0.75 * tilt - 0.12 * omega, -0.38, 0.38);

    for (size_t i = 0; i < muscles.size(); i++) {
        double act = (i < activations.size()) ? activations[i] : 0.0;
        double sign = (i % 2 == 0) ? 1.0 : -1.0;
        muscles[i].activation = clampVal(act + pdCorr * sign, -1.0, 1.0);
    }

    for (size_t i = 0; i < joints.size(); i++) {
        Joint& j = joints[i];
        if (j.invMass == 0.0) continue;
        double vx = clampVal((j.pos.x - j.prevPos.x) * AIR_DAMP, -28.0, 28.0);
        double vy = clampVal((j.pos.y - j.prevPos.y) * AIR_DAMP, -28.0, 28.0);
        j.prevPos = j.pos;
        j.pos.x += vx;
        j.pos.y += vy + GRAVITY * dt * dt;
    }

    for (int pass = 0; pass < 7; pass++) {
        solveBraces();
        solveMuscles();
        solveBones();
    }
    collideTerrain();
    solveBones();
    collideTerrain();

    Vec2 c = centroid();
    finalX = c.x; finalY = c.y;
    if (c.x > bestX) bestX = c.x;
    if (c.y < minY)  minY  = c.y;
    speedX = (c.x - prevX) / dt;
    speedY = (c.y - prevY) / dt;
    prevX = c.x; prevY = c.y;

    coreAngle = computeCoreAngle();
    tiltSum += std::abs(wrapAngle(coreAngle - startCoreAngle)) * dt;
    simTime += dt;
    tiltAvg = tiltSum / simTime;

    if (task == TASK_CUSTOM) {
        double dGoal = vecLen({c.x - goalX, c.y - goalY});
        if (dGoal < bestDistToGoal) bestDistToGoal = dGoal;
        if (dGoal <= 45.0 && timeToGoal < 0.0) {
            timeToGoal = simTime;
        }
    } else if (task == TASK_RUN) {
        if ((bestX - startX) >= 620.0 && timeToGoal < 0.0) {
            timeToGoal = simTime;
        }
    } else if (task == TASK_JUMP) {
        if ((startY - minY) >= 170.0 && timeToGoal < 0.0) {
            timeToGoal = simTime;
        }
    } else if (task == TASK_CLIMB) {
        double progress = (bestX - startX) * 0.30 + (finalX - startX) * 0.70 + (startY - minY) * 0.60;
        if (progress >= 680.0 && timeToGoal < 0.0) {
            timeToGoal = simTime;
        }
    }
}

SensorSignals PhysicsWorld::sensors() const {
    double diff = wrapAngle(coreAngle - startCoreAngle);
    return {
        clampVal(diff / 0.8, -1.0, 1.0),
        clampVal(speedX / 420.0, -1.0, 1.0),
        clampVal((double)contacts / 2.0, 0.0, 1.0),
        clampVal(speedY / 420.0, -1.0, 1.0)
    };
}
