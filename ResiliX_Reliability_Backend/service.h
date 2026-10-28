#pragma once
#include <string>

enum class ServiceStatus { Healthy, Degraded, Failed, Recovering, OpenCircuit };

struct Metrics {
    double latencyMs = 80.0;
    double errorRatePct = 0.5;
    double cpuPct = 20.0;
    double memoryPct = 30.0;
    unsigned long long samples = 0;
};

struct Thresholds {
    double latencyMs = 500.0;
    double errorRatePct = 5.0;
    double cpuPct = 90.0;
    double memoryPct = 90.0;
};

class Service {
    std::string name_;
    Metrics metrics_;
    Thresholds thresholds_;
    ServiceStatus status_ = ServiceStatus::Healthy;
    bool manuallyFailed_ = false;
public:
    Service(std::string name, Metrics metrics, Thresholds thresholds);
    const std::string& name() const;
    const Metrics& metrics() const;
    const Thresholds& thresholds() const;
    ServiceStatus status() const;
    bool manuallyFailed() const;
    void setMetrics(const Metrics& metrics);
    void setThresholds(const Thresholds& thresholds);
    void markFailed();
    void markRecovering();
    void markHealthy();
    void setOpenCircuit();
    void evaluate();
    std::string statusString() const;
};
