#ifndef EVOWALK_CORE_DSA_HPP
#define EVOWALK_CORE_DSA_HPP

#include "anatomy.hpp"
#include <vector>
#include <queue>
#include <stdexcept>
#include <iostream>
#include <utility>

template <typename T>
class MaxHeap {
    std::vector<T> data;

    void heapifyUp(int i) {
        while (i > 0) {
            int parent = (i - 1) / 2;
            if (data[parent] < data[i]) {
                std::swap(data[parent], data[i]);
                i = parent;
            } else {
                break;
            }
        }
    }

    void heapifyDown(int i) {
        int n = (int)data.size();
        while (true) {
            int largest = i;
            int left  = 2 * i + 1;
            int right = 2 * i + 2;
            if (left  < n && data[largest] < data[left])  largest = left;
            if (right < n && data[largest] < data[right]) largest = right;
            if (largest != i) {
                std::swap(data[i], data[largest]);
                i = largest;
            } else {
                break;
            }
        }
    }

public:
    void push(const T& val) {
        data.push_back(val);
        heapifyUp((int)data.size() - 1);
    }

    T pop() {
        if (data.empty()) throw std::runtime_error("MaxHeap is empty");
        T root = data[0];
        data[0] = data.back();
        data.pop_back();
        if (!data.empty()) heapifyDown(0);
        return root;
    }

    const T& top() const {
        if (data.empty()) throw std::runtime_error("MaxHeap is empty");
        return data[0];
    }

    bool empty() const { return data.empty(); }
    int  size()  const { return (int)data.size(); }
    void clear()       { data.clear(); }
};

struct BoneEdge {
    int    neighbor;
    int    boneIndex;
    double restLength;
};

class SkeletalGraph {
    std::vector<std::vector<BoneEdge>> adj;

public:
    void reset(int numJoints) {
        adj.assign(numJoints, std::vector<BoneEdge>());
    }

    void addBoneEdge(int u, int v, int boneIdx, double restLen) {
        int n = (int)adj.size();
        if (u < 0 || u >= n || v < 0 || v >= n) return;
        adj[u].push_back({v, boneIdx, restLen});
        adj[v].push_back({u, boneIdx, restLen});
    }

    const std::vector<BoneEdge>& neighbors(int joint) const {
        return adj[joint];
    }

    bool isConnected() const {
        int n = (int)adj.size();
        if (n <= 1) return true;

        std::vector<bool> visited(n, false);
        std::queue<int> q;
        q.push(0);
        visited[0] = true;
        int count = 1;

        while (!q.empty()) {
            int curr = q.front();
            q.pop();
            for (size_t i = 0; i < adj[curr].size(); i++) {
                int nxt = adj[curr][i].neighbor;
                if (!visited[nxt]) {
                    visited[nxt] = true;
                    count++;
                    q.push(nxt);
                }
            }
        }
        return count == n;
    }

    void printAdjacency() const {
        std::cout << "--- SKELETAL GRAPH (ADJACENCY LIST) ---\n";
        for (size_t i = 0; i < adj.size(); i++) {
            std::cout << "Joint [" << i << "]: ";
            for (size_t j = 0; j < adj[i].size(); j++) {
                std::cout << "-> " << adj[i][j].neighbor
                          << " (Bone #" << adj[i][j].boneIndex
                          << ", L=" << (int)adj[i][j].restLength << ") ";
            }
            std::cout << "\n";
        }
    }
};

inline void buildSkeletalGraphAndBraces(const std::vector<Joint>& joints,
                                        const std::vector<Bone>& bones,
                                        SkeletalGraph& graph,
                                        std::vector<Brace>& braces) {
    graph.reset((int)joints.size());
    for (size_t i = 0; i < bones.size(); i++) {
        graph.addBoneEdge(bones[i].jointA, bones[i].jointB, (int)i, bones[i].restLength);
    }

    braces.clear();
    for (int pivot = 0; pivot < (int)joints.size(); pivot++) {
        const std::vector<BoneEdge>& nbrs = graph.neighbors(pivot);
        for (size_t i = 0; i < nbrs.size(); i++) {
            for (size_t j = i + 1; j < nbrs.size(); j++) {
                int a = nbrs[i].neighbor, b = nbrs[j].neighbor;
                if (a == b) continue;
                double rest = vecLen(vecSub(joints[a].pos, joints[b].pos));
                braces.push_back({a, b, rest, 0.085});
            }
        }
    }
}

#endif
