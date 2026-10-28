#pragma once
#include "service.h"
#include "incident.h"
#include "dependency_manager.h"
#include "recovery_strategy.h"
#include "persistence.h"
#include "metrics.h"
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <string>

class Simulator {
    std::vector<std::unique_ptr<Service>> services_;
    std::vector<Incident> incidents_;
    DependencyManager dependencies_;
    AppConfig config_;
    Persistence persistence_;
    MetricsRecorder metrics_;
    RecoveryEngine recovery_;
    unsigned long long nextId_ = 1;
    std::map<std::string,int> consecutiveFailures_, consecutiveSuccesses_, recoveryCounts_;
    mutable std::mutex mutex_;
    Service* findServiceUnlocked(const std::string& name) const;
    double riskScore(const Service& service, const std::vector<std::string>& direct,
                     const std::vector<std::string>& indirect) const;
    Severity severityFor(double score) const;
    void persistIncident(const Incident& incident);
public:
    explicit Simulator(const std::string& configPath = "config.json");
    std::vector<std::string> serviceNames() const;
    std::string servicesJson() const;
    std::string dependenciesJson() const;
    std::string incidentsJson() const;
    std::string metricsJson() const;
    std::string healthJson() const;
    std::string injectFailure(const std::string& service, const std::string& description = "Manual failure injection");
    std::string recoverService(const std::string& service, const std::string& strategy = "restart");
    std::string updateMetrics(const std::string& service, const Metrics& metrics);
    std::string evaluateAll();
    std::string recordEvaluation(bool expectedFailure, bool detectedFailure);
    void runConsole();
};
