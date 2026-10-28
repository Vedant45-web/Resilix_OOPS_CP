#include "simulator.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

static void writeTestConfig() {
    std::ofstream f("test_config.json");
    f << R"({
      "thresholds":{"latencyMs":500,"errorRatePct":5,"cpuPct":90,"memoryPct":90},
      "recovery":{"maxRetries":3,"baseBackoffMs":0,"maxBackoffMs":0,
        "maxRecoveriesPerService":5,"circuitFailureThreshold":3,
        "circuitResetAfterSuccesses":2,"cooldownMs":7},
      "dependencies":[
        {"upstream":"Database","dependent":"Payment"},
        {"upstream":"Payment","dependent":"Order"},
        {"upstream":"Inventory","dependent":"Order"},
        {"upstream":"Cache","dependent":"Order"},
        {"upstream":"Order","dependent":"API Gateway"},
        {"upstream":"Auth","dependent":"API Gateway"}],
      "apiPort":8080,"historyFile":"test_incidents.csv","metricsFile":"test_metrics.csv"
    })";
}

static void cleanTestFiles() {
    std::remove("test_config.json");
    std::remove("test_incidents.csv");
    std::remove("test_metrics.csv");
    std::remove("test_metrics.csv.evaluations.csv");
}

int main() {
    cleanTestFiles();
    writeTestConfig();

    // Dependency graph: downstream traversal and cycle rejection.
    DependencyManager d;
    assert(d.addDependency("DB", "Payment"));
    assert(d.addDependency("Payment", "Order"));
    assert(!d.addDependency("Order", "DB"));
    const auto downstream = d.allDownstream("DB");
    assert(downstream.size() == 2);

    // Recovery strategies and bounded retry/backoff.
    Thresholds thresholds;
    Service service("test", Metrics{}, thresholds);
    RecoveryPolicy policy;
    policy.maxRetries = 3;
    policy.baseBackoffMs = 0;
    policy.maxBackoffMs = 0;
    policy.maxRecoveriesPerService = 5;
    policy.circuitFailureThreshold = 3;
    policy.circuitResetAfterSuccesses = 2;
    policy.cooldownMs = 0;
    RecoveryEngine engine(policy);

    int failures = 0, successes = 0, count = 0;
    service.markFailed();
    auto restart = engine.recover(service, "restart", failures, successes, count);
    assert(restart.success && service.status() == ServiceStatus::Healthy);

    service.markFailed();
    failures = 0; successes = 0; count = 0;
    auto retry = engine.recover(service, "retry", failures, successes, count);
    assert(retry.success && retry.attempts == 2);

    service.markFailed();
    failures = 0; successes = 0; count = 0;
    auto failover = engine.recover(service, "failover", failures, successes, count);
    assert(failover.success && service.status() == ServiceStatus::Healthy);

    service.markFailed();
    failures = 0; successes = 0; count = 0;
    auto fallback = engine.recover(service, "fallback", failures, successes, count);
    assert(fallback.success && service.status() == ServiceStatus::Healthy);

    // Circuit-breaker guard blocks restart/retry when threshold is reached.
    service.markFailed();
    failures = policy.circuitFailureThreshold;
    successes = 0; count = 0;
    auto blocked = engine.recover(service, "restart", failures, successes, count);
    assert(!blocked.success);
    assert(blocked.message.find("Circuit breaker is OPEN") != std::string::npos);
    assert(service.status() == ServiceStatus::OpenCircuit);

    // Two successful failover/fallback recoveries are required to reset an open circuit.
    auto recovery1 = engine.recover(service, "failover", failures, successes, count);
    assert(recovery1.success && failures == policy.circuitFailureThreshold);
    service.markFailed();
    auto recovery2 = engine.recover(service, "fallback", failures, successes, count);
    assert(recovery2.success && failures == 0);

    // Unknown strategies and per-service recovery limits are rejected.
    auto unknown = engine.recover(service, "unknown", failures, successes, count);
    assert(!unknown.success && unknown.message.find("Unknown strategy") != std::string::npos);
    count = policy.maxRecoveriesPerService;
    auto limited = engine.recover(service, "restart", failures, successes, count);
    assert(!limited.success && limited.message.find("Recovery limit") != std::string::npos);

    // Severe signals can produce a failed recovery and open the circuit breaker.
    Service unstable("unstable", Metrics{2000, 50, 100, 100, 0}, thresholds);
    unstable.evaluate();
    failures = 0; successes = 0; count = 0;
    auto failedRestart = engine.recover(unstable, "restart", failures, successes, count);
    assert(!failedRestart.success && failedRestart.attempts == policy.maxRetries);
    assert(failures >= policy.circuitFailureThreshold);
    assert(unstable.status() == ServiceStatus::OpenCircuit);
    auto blockedRestart = engine.recover(unstable, "restart", failures, successes, count);
    assert(!blockedRestart.success && blockedRestart.message.find("Circuit breaker is OPEN") != std::string::npos);
    auto failoverAfterOpen = engine.recover(unstable, "failover", failures, successes, count);
    assert(failoverAfterOpen.success && unstable.status() == ServiceStatus::Healthy);

    // Confirm cooldown is parsed from configuration.
    AppConfig loadedConfig = loadConfig("test_config.json");
    assert(loadedConfig.recovery.cooldownMs == 7);
    assert(loadedConfig.recovery.circuitResetAfterSuccesses == 2);

    // Full simulator integration uses isolated test CSV paths.
    Simulator sim("test_config.json");
    auto failure = sim.injectFailure("Payment", "automated test");
    assert(failure.find("\"ok\":true") != std::string::npos);
    assert(sim.incidentsJson().find("Payment") != std::string::npos);
    auto recovery = sim.recoverService("Payment", "restart");
    assert(recovery.find("\"ok\":true") != std::string::npos);
    assert(sim.servicesJson().find("API Gateway") != std::string::npos);

    auto validMetrics = sim.updateMetrics("API Gateway", Metrics{100, 1, 40, 40, 0});
    assert(validMetrics.find("\"ok\":true") != std::string::npos);
    auto invalidMetrics = sim.updateMetrics("API Gateway", Metrics{-1, 1, 40, 40, 0});
    assert(invalidMetrics.find("\"error\"") != std::string::npos);

    sim.evaluateAll();
    auto failedMetrics = sim.updateMetrics("Payment", Metrics{900, 12, 95, 92, 0});
    assert(failedMetrics.find("FAILED") != std::string::npos);
    assert(sim.incidentsJson().find("Automatic incident: health metrics crossed failure thresholds") != std::string::npos);

    sim.recordEvaluation(true, true);
    sim.recordEvaluation(false, true);
    sim.recordEvaluation(false, false);
    sim.recordEvaluation(true, false);
    const auto metrics = sim.metricsJson();
    assert(metrics.find("\"tp\":1") != std::string::npos);
    assert(metrics.find("\"fp\":1") != std::string::npos);
    assert(metrics.find("\"tn\":1") != std::string::npos);
    assert(metrics.find("\"fn\":1") != std::string::npos);

    // Restart the simulator against the same isolated files to verify persistence.
    Simulator reloaded("test_config.json");
    const auto reloadedIncidents = reloaded.incidentsJson();
    const auto reloadedMetrics = reloaded.metricsJson();
    assert(reloadedIncidents.find("Automatic incident: health metrics crossed failure thresholds") != std::string::npos);
    assert(reloadedMetrics.find("\"tp\":1") != std::string::npos);
    assert(reloadedMetrics.find("\"fp\":1") != std::string::npos);
    assert(reloadedMetrics.find("\"tn\":1") != std::string::npos);
    assert(reloadedMetrics.find("\"fn\":1") != std::string::npos);
    assert(reloadedMetrics.find("\"samples\":9") != std::string::npos);

    std::cout << "All ResiliX backend regression tests passed.\n";
    cleanTestFiles();
    return 0;
}
