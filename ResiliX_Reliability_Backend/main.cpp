#include "simulator.h"
#include "api_server.h"
#include <iostream>
#include <string>
int main(int argc,char** argv) {
    try {
        std::string mode=argc>1?argv[1]:"api";
        Simulator simulator("config.json");
        if(mode=="console") { simulator.runConsole(); return 0; }
        if(mode=="api") { AppConfig c=loadConfig("config.json");ApiServer server(simulator,c.apiPort);return server.run(); }
        std::cerr<<"Usage: ResiliX.exe [api|console]\n";return 2;
    } catch(const std::exception& e) { std::cerr<<"Fatal error: "<<e.what()<<'\n';return 1; }
}
