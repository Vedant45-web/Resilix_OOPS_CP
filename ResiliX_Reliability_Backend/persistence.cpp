#include "persistence.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <stdexcept>
#include <algorithm>

namespace {
std::string csv(const std::string& s) {
    std::string o="\""; for(char c:s) { if(c=='\"') o += "\"\""; else o += c; } return o+"\"";
}
std::vector<std::string> parseCsv(const std::string& line) {
    std::vector<std::string> out; std::string cur; bool q=false;
    for(size_t i=0;i<line.size();++i) {
        char c=line[i];
        if(c=='\"' && q && i+1<line.size() && line[i+1]=='\"') { cur+='\"'; ++i; }
        else if(c=='\"') q=!q;
        else if(c==',' && !q) { out.push_back(cur); cur.clear(); }
        else cur+=c;
    } out.push_back(cur); return out;
}
std::string val(const std::string& s, const std::string& key, const std::string& fallback) {
    std::regex re("\""+key+"\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch m; return std::regex_search(s,m,re) ? m[1].str() : fallback;
}
double num(const std::string& s, const std::string& key, double fallback) {
    std::regex re("\""+key+"\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
    std::smatch m; return std::regex_search(s,m,re) ? std::stod(m[1].str()) : fallback;
}
}
Persistence::Persistence(std::string p):path_(std::move(p)){}
const std::string& Persistence::path() const { return path_; }
std::vector<Incident> Persistence::loadIncidents() const {
    std::vector<Incident> out; std::ifstream f(path_); std::string line;
    if(!f) return out;
    std::getline(f,line);
    while(std::getline(f,line)) {
        auto c=parseCsv(line); if(c.size()<15) continue;
        try {
            Incident i; i.id=std::stoull(c[0]); i.service=c[1]; i.description=c[2];
            i.severity=c[3]=="CRITICAL"?Severity::Critical:c[3]=="HIGH"?Severity::High:c[3]=="MEDIUM"?Severity::Medium:Severity::Low;
            i.riskScore=std::stod(c[4]);
            i.status=c[5]=="RESOLVED"?IncidentStatus::Resolved:c[5]=="RECOVERING"?IncidentStatus::Recovering:c[5]=="RECOVERY_FAILED"?IncidentStatus::RecoveryFailed:IncidentStatus::Open;
            i.strategy=c[6]; i.createdAt=c[7]; i.resolvedAt=c[8]; i.recoveryMs=std::stoll(c[9]);
            auto split=[](const std::string& s){std::vector<std::string> v; std::stringstream ss(s); std::string x; while(std::getline(ss,x,'|')) if(!x.empty()) v.push_back(x); return v;};
            i.directImpact=split(c[10]); i.indirectImpact=split(c[11]); out.push_back(i);
        } catch(...) {}
    }
    return out;
}
void Persistence::appendIncident(const Incident& i) const {
    bool exists=static_cast<bool>(std::ifstream(path_));
    std::ofstream f(path_,std::ios::app);
    if(!f) throw std::runtime_error("Cannot write incident history: "+path_);
    if(!exists) f<<"id,service,description,severity,risk,status,strategy,created_at,resolved_at,recovery_ms,direct_impact,indirect_impact,unused1,unused2,unused3\n";
    auto join=[](const std::vector<std::string>& v){std::string s; for(auto& x:v){if(!s.empty())s+='|';s+=x;}return s;};
    f<<i.id<<','<<csv(i.service)<<','<<csv(i.description)<<','<<severityString(i.severity)<<','<<i.riskScore<<','<<incidentStatusString(i.status)<<','<<csv(i.strategy)<<','<<csv(i.createdAt)<<','<<csv(i.resolvedAt)<<','<<i.recoveryMs<<','<<csv(join(i.directImpact))<<','<<csv(join(i.indirectImpact))<<",,,\n";
}
void Persistence::rewriteIncidents(const std::vector<Incident>& v) const {
    std::ofstream f(path_,std::ios::trunc);
    if(!f) throw std::runtime_error("Cannot rewrite incident history: "+path_);
    f<<"id,service,description,severity,risk,status,strategy,created_at,resolved_at,recovery_ms,direct_impact,indirect_impact,unused1,unused2,unused3\n";
    auto join=[](const std::vector<std::string>& a){std::string s; for(auto& x:a){if(!s.empty())s+='|';s+=x;}return s;};
    for(const auto& i:v) f<<i.id<<','<<csv(i.service)<<','<<csv(i.description)<<','<<severityString(i.severity)<<','<<i.riskScore<<','<<incidentStatusString(i.status)<<','<<csv(i.strategy)<<','<<csv(i.createdAt)<<','<<csv(i.resolvedAt)<<','<<i.recoveryMs<<','<<csv(join(i.directImpact))<<','<<csv(join(i.indirectImpact))<<",,,\n";
}
AppConfig loadConfig(const std::string& path) {
    AppConfig c; std::ifstream f(path); if(!f) return c;
    std::ostringstream ss; ss<<f.rdbuf(); auto s=ss.str();
    c.defaultThresholds.latencyMs=num(s,"latencyMs",c.defaultThresholds.latencyMs);
    c.defaultThresholds.errorRatePct=num(s,"errorRatePct",c.defaultThresholds.errorRatePct);
    c.defaultThresholds.cpuPct=num(s,"cpuPct",c.defaultThresholds.cpuPct);
    c.defaultThresholds.memoryPct=num(s,"memoryPct",c.defaultThresholds.memoryPct);
    c.recovery.maxRetries=static_cast<int>(num(s,"maxRetries",c.recovery.maxRetries));
    c.recovery.baseBackoffMs=static_cast<int>(num(s,"baseBackoffMs",c.recovery.baseBackoffMs));
    c.recovery.maxBackoffMs=static_cast<int>(num(s,"maxBackoffMs",c.recovery.maxBackoffMs));
    c.recovery.maxRecoveriesPerService=static_cast<int>(num(s,"maxRecoveriesPerService",c.recovery.maxRecoveriesPerService));
    c.recovery.circuitFailureThreshold=static_cast<int>(num(s,"circuitFailureThreshold",c.recovery.circuitFailureThreshold));
    c.recovery.circuitResetAfterSuccesses=static_cast<int>(num(s,"circuitResetAfterSuccesses",c.recovery.circuitResetAfterSuccesses));
    c.recovery.cooldownMs=static_cast<int>(num(s,"cooldownMs",c.recovery.cooldownMs));
    c.apiPort=static_cast<int>(num(s,"apiPort",c.apiPort));
    c.historyFile=val(s,"historyFile",c.historyFile);
    c.metricsFile=val(s,"metricsFile",c.metricsFile);
    std::regex depRe("\\{\\s*\"upstream\"\\s*:\\s*\"([^\"]+)\"\\s*,\\s*\"dependent\"\\s*:\\s*\"([^\"]+)\"\\s*\\}");
    for(std::sregex_iterator it(s.begin(),s.end(),depRe), end; it!=end; ++it)
        c.dependencies.emplace_back((*it)[1].str(), (*it)[2].str());
    return c;
}
