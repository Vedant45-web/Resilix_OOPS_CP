#include "service.h"
#include <stdexcept>
#include <utility>

Service::Service(std::string name, Metrics metrics, Thresholds thresholds)
    : name_(std::move(name)), metrics_(metrics), thresholds_(thresholds) {
    if (name_.empty()) throw std::invalid_argument("Service name cannot be empty");
    setMetrics(metrics);
    setThresholds(thresholds);
}
const std::string& Service::name() const { return name_; }
const Metrics& Service::metrics() const { return metrics_; }
const Thresholds& Service::thresholds() const { return thresholds_; }
ServiceStatus Service::status() const { return status_; }
bool Service::manuallyFailed() const { return manuallyFailed_; }

void Service::setMetrics(const Metrics& m) {
    if (m.latencyMs < 0 || m.errorRatePct < 0 || m.errorRatePct > 100 ||
        m.cpuPct < 0 || m.cpuPct > 100 || m.memoryPct < 0 || m.memoryPct > 100)
        throw std::invalid_argument("Metrics out of range (latency >= 0; percentages 0..100)");
    metrics_ = m;
    ++metrics_.samples;
}
void Service::setThresholds(const Thresholds& t) {
    if (t.latencyMs <= 0 || t.errorRatePct <= 0 || t.errorRatePct > 100 ||
        t.cpuPct <= 0 || t.cpuPct > 100 || t.memoryPct <= 0 || t.memoryPct > 100)
        throw std::invalid_argument("Thresholds must be positive; percentage thresholds <= 100");
    thresholds_ = t;
}
void Service::markFailed() { status_ = ServiceStatus::Failed; manuallyFailed_ = true; }
void Service::markRecovering() { status_ = ServiceStatus::Recovering; manuallyFailed_ = false; }
void Service::markHealthy() { status_ = ServiceStatus::Healthy; manuallyFailed_ = false; }
void Service::setOpenCircuit() { status_ = ServiceStatus::OpenCircuit; }
void Service::evaluate() {
    if (manuallyFailed_) { status_ = ServiceStatus::Failed; return; }
    int bad = 0;
    if (metrics_.latencyMs > thresholds_.latencyMs) ++bad;
    if (metrics_.errorRatePct > thresholds_.errorRatePct) ++bad;
    if (metrics_.cpuPct > thresholds_.cpuPct) ++bad;
    if (metrics_.memoryPct > thresholds_.memoryPct) ++bad;
    status_ = bad >= 2 ? ServiceStatus::Failed : (bad == 1 ? ServiceStatus::Degraded : ServiceStatus::Healthy);
}
std::string Service::statusString() const {
    switch (status_) {
        case ServiceStatus::Healthy: return "HEALTHY";
        case ServiceStatus::Degraded: return "DEGRADED";
        case ServiceStatus::Failed: return "FAILED";
        case ServiceStatus::Recovering: return "RECOVERING";
        case ServiceStatus::OpenCircuit: return "OPEN_CIRCUIT";
    }
    return "UNKNOWN";
}
