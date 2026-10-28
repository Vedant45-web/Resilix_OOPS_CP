#pragma once
#include "service.h"
#include <string>

struct RecoveryPolicy {
    int maxRetries = 3;
    int baseBackoffMs = 100;
    int maxBackoffMs = 1000;
    int maxRecoveriesPerService = 5;
    int circuitFailureThreshold = 3;
    int circuitResetAfterSuccesses = 2;
    int cooldownMs = 0;
};
struct RecoveryResult {
    bool success = false;
    int attempts = 0;
    long long elapsedMs = 0;
    std::string message;
};
class RecoveryEngine {
    RecoveryPolicy policy_;
public:
    explicit RecoveryEngine(RecoveryPolicy policy = {});
    const RecoveryPolicy& policy() const;
    RecoveryResult recover(Service& service, const std::string& strategy,
                           int& consecutiveFailures, int& consecutiveSuccesses,
                           int& recoveryCount) const;
};
