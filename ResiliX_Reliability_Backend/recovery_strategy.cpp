#include "recovery_strategy.h"
#include <algorithm>
#include <chrono>
#include <thread>

RecoveryEngine::RecoveryEngine(RecoveryPolicy p) : policy_(p) {
    if (policy_.maxRetries < 1) policy_.maxRetries = 1;
    if (policy_.baseBackoffMs < 0) policy_.baseBackoffMs = 0;
    if (policy_.maxBackoffMs < policy_.baseBackoffMs) policy_.maxBackoffMs = policy_.baseBackoffMs;
}
const RecoveryPolicy& RecoveryEngine::policy() const { return policy_; }
RecoveryResult RecoveryEngine::recover(Service& service, const std::string& strategy,
 int& failures, int& successes, int& count) const {
    RecoveryResult r;
    auto start = std::chrono::steady_clock::now();
    if (strategy != "restart" && strategy != "retry" && strategy != "failover" && strategy != "fallback") {
        r.message = "Unknown strategy. Use restart, retry, failover, or fallback."; return r;
    }
    if (count >= policy_.maxRecoveriesPerService) {
        r.message = "Recovery limit reached for this service.";
        return r;
    }

    const bool circuitAlreadyOpen = failures >= policy_.circuitFailureThreshold;
    if (circuitAlreadyOpen && strategy != "failover" && strategy != "fallback") {
        service.setOpenCircuit();
        r.message = "Circuit breaker is OPEN; use failover/fallback or wait for operator reset.";
        return r;
    }

    service.markRecovering();
    ++count;
    if (policy_.cooldownMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(policy_.cooldownMs));
    }

    const Metrics initialMetrics = service.metrics();
    const Thresholds thresholds = service.thresholds();
    // Extremely bad signals model a restart/retry that cannot stabilize the service.
    // Failover/fallback remain available as alternate simulated recovery paths.
    const bool severeSignals =
        initialMetrics.latencyMs > thresholds.latencyMs * 3.0 ||
        initialMetrics.errorRatePct > thresholds.errorRatePct * 3.0 ||
        initialMetrics.cpuPct >= 99.0 || initialMetrics.memoryPct >= 99.0;

    for (int attempt = 1; attempt <= policy_.maxRetries; ++attempt) {
        r.attempts = attempt;
        // This is a simulator: strategies model outcomes, not actual process restart/failover.
        bool success = !severeSignals;
        if (strategy == "retry") success = !severeSignals && (attempt >= 2 || policy_.maxRetries == 1);
        if (strategy == "failover" || strategy == "fallback") success = true;
        if (success) {
            service.markHealthy();
            Metrics baseline = service.metrics();
            baseline.latencyMs = std::min(baseline.latencyMs, service.thresholds().latencyMs * 0.5);
            baseline.errorRatePct = std::min(baseline.errorRatePct, service.thresholds().errorRatePct * 0.2);
            baseline.cpuPct = std::min(baseline.cpuPct, 45.0);
            baseline.memoryPct = std::min(baseline.memoryPct, 50.0);
            service.setMetrics(baseline);
            service.evaluate();
            if (service.status() == ServiceStatus::Healthy) {
                r.success = true;
                ++successes;
                if (circuitAlreadyOpen) {
                    // Recovery through failover/fallback acts as a controlled circuit reset.
                    const int requiredSuccesses = std::max(1, policy_.circuitResetAfterSuccesses);
                    if (successes >= requiredSuccesses) {
                        failures = 0;
                        successes = 0;
                    }
                } else {
                    // A normal successful recovery clears the consecutive-failure streak.
                    failures = 0;
                    successes = 0;
                }
                r.message = strategy + " simulated; post-recovery health verification passed.";
                break;
            }
        }
        ++failures; successes = 0;
        if (attempt < policy_.maxRetries) {
            int delay = std::min(policy_.maxBackoffMs, policy_.baseBackoffMs * (1 << std::min(attempt-1, 10)));
            if (delay > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
    }
    if (!r.success) {
        if (failures >= policy_.circuitFailureThreshold) {
            service.setOpenCircuit();
            r.message = "Recovery attempts exhausted; circuit breaker is OPEN after repeated failures.";
        } else {
            r.message = "Recovery attempts exhausted; health verification failed.";
        }
    }
    auto end = std::chrono::steady_clock::now();
    r.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
    return r;
}
