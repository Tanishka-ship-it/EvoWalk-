@echo off
g++ -O2 -std=c++17 -Wall -Wextra main.cpp core\fitness.cpp core\evolution.cpp external\verlet_physics.cpp -lws2_32 -o evowalk.exe
if %errorlevel% neq 0 (
    echo Build failed. Make sure g++ (MinGW) is in your PATH.
    pause
    exit /b %errorlevel%
)
echo Build succeeded! Opening http://localhost:8080 ...
start "" http://localhost:8080
evowalk.exe
