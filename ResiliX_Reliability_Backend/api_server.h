#pragma once
#include "simulator.h"
class ApiServer {
    Simulator& simulator_;
    int port_;
public:
    ApiServer(Simulator& simulator, int port);
    int run();
};
