#ifndef EVOWALK_EXTERNAL_HTTP_SERVER_HPP
#define EVOWALK_EXTERNAL_HTTP_SERVER_HPP

#include "../core/evolution.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <functional>
#include <cstdlib>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET SocketHandle;
  #define CLOSE_SOCK closesocket
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  typedef int SocketHandle;
  #define INVALID_SOCKET (-1)
  #define CLOSE_SOCK close
#endif

inline std::string readFileText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline std::string mimeFor(const std::string& path) {
    if (path.find(".html")  != std::string::npos) return "text/html; charset=utf-8";
    if (path.find(".css")   != std::string::npos) return "text/css; charset=utf-8";
    if (path.find(".js")    != std::string::npos) return "application/javascript; charset=utf-8";
    if (path.find(".woff2") != std::string::npos) return "font/woff2";
    return "application/json; charset=utf-8";
}

inline void parseStartPayload(const std::string& text, Simulation& sim) {
    std::istringstream ss(text);
    std::string taskStr;
    double dur = 7.0, mut = 0.18, gx = 750.0, gy = 480.0;
    int pop = 24;
    if (!(ss >> taskStr >> dur >> pop >> mut >> gx >> gy)) return;

    BodyBlueprint bp;
    int nJ = 0, nB = 0, nM = 0;
    ss >> nJ;
    for (int i = 0; i < nJ; i++) {
        double x, y, r, m;
        ss >> x >> y >> r >> m;
        bp.joints.push_back(makeJoint(x, y, r, m));
    }
    ss >> nB;
    for (int i = 0; i < nB; i++) {
        int a, b;
        ss >> a >> b;
        double len = vecLen(vecSub(bp.joints[a].pos, bp.joints[b].pos));
        bp.bones.push_back({a, b, len});
    }
    ss >> nM;
    for (int i = 0; i < nM; i++) {
        int a, b; double str;
        ss >> a >> b >> str;
        Vec2 midA = vecScale(vecAdd(bp.joints[bp.bones[a].jointA].pos, bp.joints[bp.bones[a].jointB].pos), 0.5);
        Vec2 midB = vecScale(vecAdd(bp.joints[bp.bones[b].jointA].pos, bp.joints[bp.bones[b].jointB].pos), 0.5);
        bp.muscles.push_back({a, b, vecLen(vecSub(midA, midB)), str, 0.0});
    }
    if (bp.joints.size() < 2) bp = BodyBlueprint::runnerPreset();
    sim.start(bp, parseTask(taskStr), dur, pop, mut, gx, gy, 42);
}

inline std::string serializeSimulationToJson(const Simulation& sim) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1);
    out << "{\"gen\":" << sim.generation
        << ",\"genTime\":" << std::setprecision(2) << sim.genTime
        << ",\"duration\":" << sim.duration
        << ",\"bestEver\":" << std::setprecision(1) << sim.bestEverFit
        << ",\"bestEverGen\":" << sim.bestEverGen
        << ",\"connected\":" << (sim.worlds.empty() || sim.worlds[0].graph.isConnected() ? "true" : "false")
        << ",\"history\":[";
    for (size_t i = 0; i < sim.history.size(); i++) {
        if (i) out << ",";
        out << "{\"gen\":" << sim.history[i].gen << ",\"best\":" << sim.history[i].best << ",\"avg\":" << sim.history[i].avg << "}";
    }
    out << "],\"creatures\":[";
    for (size_t i = 0; i < sim.worlds.size(); i++) {
        if (i) out << ",";
        const PhysicsWorld& w = sim.worlds[i];
        out << "{\"fit\":" << w.fitness << ",\"j\":[";
        for (size_t k = 0; k < w.joints.size(); k++) {
            if (k) out << ",";
            out << "[" << w.joints[k].pos.x << "," << w.joints[k].pos.y << ","
                << w.joints[k].radius << "," << (w.joints[k].contact ? 1 : 0) << "]";
        }
        out << "],\"a\":[";
        out << std::setprecision(2);
        for (size_t k = 0; k < w.muscles.size(); k++) {
            if (k) out << ",";
            out << w.muscles[k].activation;
        }
        out << std::setprecision(1) << "]}";
    }
    out << "]}";
    return out.str();
}

inline void runLocalServer(int port, const std::string& webRoot,
                           std::function<std::string(const std::string&, const std::string&)> onApi) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    SocketHandle srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((unsigned short)port);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) < 0 || listen(srv, 16) < 0) {
        std::cerr << "Error: could not start server on port " << port << "\n";
        return;
    }

    std::cout << "================================================================\n"
              << "  EVOWALK C++ ENGINE RUNNING!\n"
              << "  Open in browser: http://localhost:" << port << "\n"
              << "  Press Ctrl+C to stop.\n"
              << "================================================================\n";

    while (true) {
        SocketHandle client = accept(srv, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        std::string req;
        char buf[4096];
        int n = recv(client, buf, sizeof(buf) - 1, 0);
        if (n <= 0) { CLOSE_SOCK(client); continue; }
        req.append(buf, n);

        size_t headerEnd = req.find("\r\n\r\n");
        size_t contentLen = 0;
        size_t clPos = req.find("Content-Length:");
        if (clPos == std::string::npos) clPos = req.find("content-length:");
        if (clPos != std::string::npos) contentLen = (size_t)std::atoi(req.c_str() + clPos + 15);

        while (headerEnd != std::string::npos && req.size() < headerEnd + 4 + contentLen) {
            n = recv(client, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            req.append(buf, n);
        }

        std::istringstream rl(req);
        std::string method, rawPath;
        rl >> method >> rawPath;
        std::string body = (headerEnd != std::string::npos) ? req.substr(headerEnd + 4) : "";

        std::string status = "200 OK", ctype = "application/json", payload;
        if (rawPath.rfind("/api/", 0) == 0) {
            payload = onApi(rawPath, body);
        } else {
            std::string clean = rawPath.substr(0, rawPath.find('?'));
            if (clean == "/") clean = "/index.html";
            if (clean.find("..") != std::string::npos) { status = "403 Forbidden"; }
            else {
                payload = readFileText(webRoot + clean);
                if (payload.empty()) { status = "404 Not Found"; payload = "Not Found"; ctype = "text/plain"; }
                else ctype = mimeFor(clean);
            }
        }

        std::ostringstream resp;
        resp << "HTTP/1.1 " << status << "\r\n"
             << "Content-Type: " << ctype << "\r\n"
             << "Content-Length: " << payload.size() << "\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Connection: close\r\n\r\n" << payload;
        std::string out = resp.str();
        send(client, out.c_str(), (int)out.size(), 0);
        CLOSE_SOCK(client);
    }
}

#endif
