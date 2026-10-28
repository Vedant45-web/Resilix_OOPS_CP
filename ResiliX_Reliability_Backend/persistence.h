#pragma once
#include "incident.h"
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <memory>
#include "service.h"
#include "dependency_manager.h"
#include "recovery_strategy.h"

class Persistence {
    std::string path_;
public:
    explicit Persistence(std::string path = "incidents.csv");
    const std::string& path() const;
    std::vector<Incident> loadIncidents() const;
    void appendIncident(const Incident& incident) const;
    void rewriteIncidents(const std::vector<Incident>& incidents) const;
};
struct AppConfig {
    Thresholds defaultThresholds;
    RecoveryPolicy recovery;
    int apiPort = 8080;
    std::string historyFile = "incidents.csv";
    std::string metricsFile = "metrics.csv";
    std::vector<std::pair<std::string, std::string>> dependencies;
};
AppConfig loadConfig(const std::string& path);
