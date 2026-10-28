#include "incident.h"
#include <ctime>
#include <iomanip>
#include <sstream>
std::string severityString(Severity s) {
    switch(s) { case Severity::Low:return "LOW"; case Severity::Medium:return "MEDIUM";
        case Severity::High:return "HIGH"; case Severity::Critical:return "CRITICAL"; }
    return "LOW";
}
std::string incidentStatusString(IncidentStatus s) {
    switch(s) { case IncidentStatus::Open:return "OPEN"; case IncidentStatus::Recovering:return "RECOVERING";
        case IncidentStatus::Resolved:return "RESOLVED"; case IncidentStatus::RecoveryFailed:return "RECOVERY_FAILED"; }
    return "OPEN";
}
std::string utcNow() {
    std::time_t t = std::time(nullptr); std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream o; o << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ"); return o.str();
}
