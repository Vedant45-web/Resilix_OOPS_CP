#include "simulator.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>
#include <regex>

namespace {
std::string esc(const std::string& s) { std::string o; for(char c:s) { if(c=='"') o += "\\\""; else if(c=='\\') o += "\\\\"; else if(c=='\n') o += "\\n"; else o += c; } return o; }
std::string quote(const std::string& s){return "\""+esc(s)+"\"";}
std::string arr(const std::vector<std::string>& v){std::string o="[";for(size_t i=0;i<v.size();++i){if(i)o+=',';o+=quote(v[i]);}return o+"]";}
std::string number(double n){std::ostringstream o;o<<std::fixed<<std::setprecision(2)<<n;return o.str();}
}
Simulator::Simulator(const std::string& path):config_(loadConfig(path)),persistence_(config_.historyFile),metrics_(config_.metricsFile),recovery_(config_.recovery) {
    incidents_=persistence_.loadIncidents();
    for(const auto& i:incidents_) nextId_=std::max(nextId_,i.id+1);
    struct Seed {const char* name; double lat,err,cpu,mem;};
    Seed seeds[]={{"API Gateway",70,.2,20,25},{"Auth",90,.5,25,30},{"Order",130,1.0,35,40},{"Payment",180,1.2,45,50},{"Inventory",100,.6,30,35},{"Database",160,.8,50,55},{"Cache",35,.1,15,20}};
    for(const auto& s:seeds) {
        Metrics m; m.latencyMs=s.lat;m.errorRatePct=s.err;m.cpuPct=s.cpu;m.memoryPct=s.mem;
        services_.push_back(std::make_unique<Service>(s.name,m,config_.defaultThresholds));
        dependencies_.addService(s.name);
    }
    if (!config_.dependencies.empty()) {
        for (const auto& edge : config_.dependencies) dependencies_.addDependency(edge.first, edge.second);
    } else {
        dependencies_.addDependency("Database","Payment");
        dependencies_.addDependency("Payment","Order");
        dependencies_.addDependency("Inventory","Order");
        dependencies_.addDependency("Cache","Order");
        dependencies_.addDependency("Order","API Gateway");
        dependencies_.addDependency("Auth","API Gateway");
    }
}
Service* Simulator::findServiceUnlocked(const std::string& n) const {for(const auto& s:services_)if(s->name()==n)return s.get();return nullptr;}
std::vector<std::string> Simulator::serviceNames() const {std::lock_guard<std::mutex> l(mutex_);std::vector<std::string> o;for(auto& s:services_)o.push_back(s->name());return o;}
double Simulator::riskScore(const Service& s,const std::vector<std::string>& d,const std::vector<std::string>& in) const {
    const auto& m=s.metrics(); double score=10;
    score += std::min(25.0, (m.latencyMs/s.thresholds().latencyMs)*8.0);
    score += std::min(25.0, (m.errorRatePct/s.thresholds().errorRatePct)*8.0);
    score += std::min(20.0, (m.cpuPct/100.0)*20.0);
    score += std::min(20.0, (m.memoryPct/100.0)*20.0);
    score += std::min(20.0, d.size()*7.0 + in.size()*3.0);
    return std::min(100.0,score);
}
Severity Simulator::severityFor(double s) const {return s>=80?Severity::Critical:s>=60?Severity::High:s>=40?Severity::Medium:Severity::Low;}
void Simulator::persistIncident(const Incident&) { persistence_.rewriteIncidents(incidents_); }
std::string Simulator::servicesJson() const {
    std::lock_guard<std::mutex> l(mutex_);std::string o="{\"services\":[";
    for(size_t i=0;i<services_.size();++i){auto& s=*services_[i];if(i)o+=',';
        o+="{\"name\":"+quote(s.name())+",\"status\":"+quote(s.statusString())+
        ",\"latencyMs\":"+number(s.metrics().latencyMs)+",\"errorRatePct\":"+number(s.metrics().errorRatePct)+
        ",\"cpuPct\":"+number(s.metrics().cpuPct)+",\"memoryPct\":"+number(s.metrics().memoryPct)+
        ",\"samples\":"+std::to_string(s.metrics().samples)+"}";}
    return o+"]}";
}
std::string Simulator::dependenciesJson() const {
    auto e=dependencies_.edges();std::string o="{\"convention\":\"upstream -> dependent\",\"edges\":[";
    for(size_t i=0;i<e.size();++i){if(i)o+=',';o+="{\"upstream\":"+quote(e[i].first)+",\"dependent\":"+quote(e[i].second)+"}";}return o+"]}";
}
std::string Simulator::incidentsJson() const {
    std::lock_guard<std::mutex> l(mutex_);std::string o="{\"incidents\":[";
    for(size_t x=0;x<incidents_.size();++x){const auto& i=incidents_[x];if(x)o+=',';
        o+="{\"id\":"+std::to_string(i.id)+",\"service\":"+quote(i.service)+",\"description\":"+quote(i.description)+
        ",\"severity\":"+quote(severityString(i.severity))+",\"riskScore\":"+number(i.riskScore)+
        ",\"status\":"+quote(incidentStatusString(i.status))+",\"strategy\":"+quote(i.strategy)+
        ",\"createdAt\":"+quote(i.createdAt)+",\"resolvedAt\":"+quote(i.resolvedAt)+
        ",\"recoveryMs\":"+std::to_string(i.recoveryMs)+",\"directImpact\":"+arr(i.directImpact)+
        ",\"indirectImpact\":"+arr(i.indirectImpact)+"}";}
    return o+"]}";
}
std::string Simulator::metricsJson() const {
    std::lock_guard<std::mutex> l(mutex_);auto s=metrics_.summarize(incidents_);
    return "{\"samples\":"+std::to_string(s.samples)+",\"incidents\":"+std::to_string(s.incidents)+
    ",\"resolved\":"+std::to_string(s.resolved)+",\"failedRecoveries\":"+std::to_string(s.failedRecoveries)+
    ",\"meanRecoveryMs\":"+number(s.meanRecoveryMs)+",\"availabilityPct\":"+number(s.availabilityPct)+
    ",\"detectionPrecision\":"+number(s.detectionPrecision)+",\"detectionRecall\":"+number(s.detectionRecall)+
    ",\"tp\":"+std::to_string(s.truePositive)+",\"fp\":"+std::to_string(s.falsePositive)+
    ",\"tn\":"+std::to_string(s.trueNegative)+",\"fn\":"+std::to_string(s.falseNegative)+"}";
}
std::string Simulator::healthJson() const {return "{\"ok\":true,\"name\":\"ResiliX\",\"mode\":\"simulation\",\"apiVersion\":\"1.0\"}";}
std::string Simulator::injectFailure(const std::string& name,const std::string& desc) {
    std::lock_guard<std::mutex> l(mutex_);auto* s=findServiceUnlocked(name);if(!s)return "{\"error\":\"Unknown service\"}";
    s->markFailed();auto direct=dependencies_.directDependents(name);auto indirect=dependencies_.allDownstream(name);
    indirect.erase(std::remove_if(indirect.begin(),indirect.end(),[&](const std::string& n){return std::find(direct.begin(),direct.end(),n)!=direct.end();}),indirect.end());
    Incident i;i.id=nextId_++;i.service=name;i.description=desc;i.directImpact=direct;i.indirectImpact=indirect;
    i.riskScore=riskScore(*s,direct,indirect);i.severity=severityFor(i.riskScore);i.createdAt=utcNow();i.status=IncidentStatus::Open;
    incidents_.push_back(i);persistence_.rewriteIncidents(incidents_);
    return "{\"ok\":true,\"incidentId\":"+std::to_string(i.id)+",\"severity\":"+quote(severityString(i.severity))+
    ",\"riskScore\":"+number(i.riskScore)+",\"directImpact\":"+arr(direct)+",\"indirectImpact\":"+arr(indirect)+"}";
}
std::string Simulator::recoverService(const std::string& name,const std::string& strategy) {
    std::lock_guard<std::mutex> l(mutex_);auto* s=findServiceUnlocked(name);if(!s)return "{\"error\":\"Unknown service\"}";
    auto it=std::find_if(incidents_.rbegin(),incidents_.rend(),[&](const Incident& i){return i.service==name&&(i.status==IncidentStatus::Open||i.status==IncidentStatus::RecoveryFailed);});
    if(it==incidents_.rend())return "{\"error\":\"No open incident for this service\"}";
    auto idx=static_cast<size_t>(std::distance(it,incidents_.rend())-1);
    auto& incident=incidents_[idx];incident.status=IncidentStatus::Recovering;incident.strategy=strategy;
    int& f=consecutiveFailures_[name];int& ok=consecutiveSuccesses_[name];int& c=recoveryCounts_[name];
    auto result=recovery_.recover(*s,strategy,f,ok,c);
    incident.recoveryMs=result.elapsedMs > 0 ? result.elapsedMs : (result.success ? 1 : 0);
    if(result.success){incident.status=IncidentStatus::Resolved;incident.resolvedAt=utcNow();}
    else incident.status=IncidentStatus::RecoveryFailed;
    persistence_.rewriteIncidents(incidents_);
    return "{\"ok\":"+std::string(result.success?"true":"false")+",\"attempts\":"+std::to_string(result.attempts)+
    ",\"elapsedMs\":"+std::to_string(result.elapsedMs)+",\"message\":"+quote(result.message)+
    ",\"status\":"+quote(s->statusString())+"}";
}
std::string Simulator::updateMetrics(const std::string& name,const Metrics& m) {
    std::lock_guard<std::mutex> l(mutex_);auto* s=findServiceUnlocked(name);if(!s)return "{\"error\":\"Unknown service\"}";
    const std::string previousStatus=s->statusString();
    try{s->setMetrics(m);s->evaluate();metrics_.recordSample(name,s->metrics(),s->statusString());}
    catch(const std::exception& e){return "{\"error\":"+quote(e.what())+"}";}
    bool hasOpen=std::any_of(incidents_.begin(),incidents_.end(),[&](const Incident& i){return i.service==name&&(i.status==IncidentStatus::Open||i.status==IncidentStatus::Recovering||i.status==IncidentStatus::RecoveryFailed);});
    if(s->status()==ServiceStatus::Failed && previousStatus!="FAILED" && !hasOpen){
        auto direct=dependencies_.directDependents(name);auto indirect=dependencies_.allDownstream(name);
        indirect.erase(std::remove_if(indirect.begin(),indirect.end(),[&](const std::string& n){return std::find(direct.begin(),direct.end(),n)!=direct.end();}),indirect.end());
        Incident i;i.id=nextId_++;i.service=name;i.description="Automatic incident: health metrics crossed failure thresholds";i.directImpact=direct;i.indirectImpact=indirect;
        i.riskScore=riskScore(*s,direct,indirect);i.severity=severityFor(i.riskScore);i.createdAt=utcNow();i.status=IncidentStatus::Open;incidents_.push_back(i);persistence_.rewriteIncidents(incidents_);
    }
    return "{\"ok\":true,\"service\":"+quote(name)+",\"status\":"+quote(s->statusString())+"}";
}
std::string Simulator::evaluateAll() {
    std::lock_guard<std::mutex> l(mutex_);std::string o="{\"evaluated\":[";bool first=true;
    for(auto& p:services_){p->evaluate();metrics_.recordSample(p->name(),p->metrics(),p->statusString());if(!first)o+=',';first=false;
        o+="{\"service\":"+quote(p->name())+",\"status\":"+quote(p->statusString())+"}";}
    return o+"]}";
}
std::string Simulator::recordEvaluation(bool expected,bool detected) {
    // Explicit test/evaluation hook; accuracy counters are session-local.
    metrics_.recordEvaluation(expected,detected);return metricsJson();
}
void Simulator::runConsole() {
    while(true) {
        std::cout<<"\n=== ResiliX Reliability Backend ===\n1. List services\n2. Inject failure\n3. Recover service\n4. Update metrics\n5. Evaluate all services\n6. Show incidents JSON\n7. Show metrics JSON\n8. Exit\nChoice: ";
        int c;if(!(std::cin>>c))return;
        if(c==8)return;
        if(c==1)std::cout<<servicesJson()<<'\n';
        else if(c==2){std::string n;std::cout<<"Service name: ";std::getline(std::cin>>std::ws,n);std::cout<<injectFailure(n)<<'\n';}
        else if(c==3){std::string n,s;std::cout<<"Service name: ";std::getline(std::cin>>std::ws,n);std::cout<<"Strategy (restart/retry/failover/fallback): ";std::getline(std::cin,s);std::cout<<recoverService(n,s)<<'\n';}
        else if(c==4){std::string n;Metrics m;std::cout<<"Service name: ";std::getline(std::cin>>std::ws,n);std::cout<<"Latency ms, error %, CPU %, memory %: ";std::cin>>m.latencyMs>>m.errorRatePct>>m.cpuPct>>m.memoryPct;std::cout<<updateMetrics(n,m)<<'\n';}
        else if(c==5)std::cout<<evaluateAll()<<'\n';
        else if(c==6)std::cout<<incidentsJson()<<'\n';
        else if(c==7)std::cout<<metricsJson()<<'\n';
        else std::cout<<"Invalid choice.\n";
    }
}
