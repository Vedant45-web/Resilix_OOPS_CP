#pragma once
#include <string>
#include <vector>
#include <chrono>

enum class Severity { Low, Medium, High, Critical };
enum class IncidentStatus { Open, Recovering, Resolved, RecoveryFailed };

struct Incident {
    unsigned long long id = 0;
    std::string service;
    std::string description;
    Severity severity = Severity::Low;
    double riskScore = 0;
    IncidentStatus status = IncidentStatus::Open;
    std::vector<std::string> directImpact;
    std::vector<std::string> indirectImpact;
    std::string strategy = "none";
    std::string createdAt;
    std::string resolvedAt;
    long long recoveryMs = -1;
};
std::string severityString(Severity s);
std::string incidentStatusString(IncidentStatus s);
std::string utcNow();
