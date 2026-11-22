CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall -Wextra

CORE_SRCS = core/fitness.cpp core/evolution.cpp
EXT_SRCS  = external/verlet_physics.cpp
HEADERS   = core/anatomy.hpp core/dsa.hpp core/fitness.hpp core/evolution.hpp \
            external/verlet_physics.hpp external/http_server.hpp

evowalk: main.cpp $(CORE_SRCS) $(EXT_SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) main.cpp $(CORE_SRCS) $(EXT_SRCS) -o evowalk

clean:
	rm -f evowalk evowalk.exe
