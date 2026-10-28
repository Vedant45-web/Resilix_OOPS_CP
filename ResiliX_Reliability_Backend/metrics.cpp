#include "metrics.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>
MetricsRecorder::MetricsRecorder(std::string p):path_(std::move(p)),evaluationPath_(path_+".evaluations.csv"){loadCounters();}
void MetricsRecorder::loadCounters() {
    { std::ifstream f(path_); std::string line; if(f){std::getline(f,line);while(std::getline(f,line)){if(line.empty())continue;++totalSamples_;if(line.find(",HEALTHY,")!=std::string::npos||line.find(",DEGRADED,")!=std::string::npos)++upSamples_;}} }
    std::ifstream e(evaluationPath_); std::string line; if(e){std::getline(e,line);while(std::getline(e,line)){std::istringstream row(line);std::string expected,detected;if(std::getline(row,expected,',')&&std::getline(row,detected,',')){recordEvaluationInMemory(expected=="1",detected=="1");}}}
}
void MetricsRecorder::recordEvaluationInMemory(bool expectedFailure,bool detectedFailure) {
    if(expectedFailure&&detectedFailure)++detections_["tp"];
    else if(!expectedFailure&&detectedFailure)++detections_["fp"];
    else if(!expectedFailure&&!detectedFailure)++detections_["tn"];
    else ++detections_["fn"];
}
void MetricsRecorder::recordSample(const std::string& service,const Metrics& m,const std::string& status) {
    bool exists=static_cast<bool>(std::ifstream(path_));std::ofstream f(path_,std::ios::app);
    if(!exists)f<<"timestamp,service,latency_ms,error_rate_pct,cpu_pct,memory_pct,status\n";
    f<<utcNow()<<','<<service<<','<<m.latencyMs<<','<<m.errorRatePct<<','<<m.cpuPct<<','<<m.memoryPct<<','<<status<<'\n';
    ++totalSamples_;if(status=="HEALTHY"||status=="DEGRADED")++upSamples_;
}
void MetricsRecorder::recordEvaluation(bool expectedFailure,bool detectedFailure) {
    bool exists=static_cast<bool>(std::ifstream(evaluationPath_));std::ofstream f(evaluationPath_,std::ios::app);
    if(!exists)f<<"expected_failure,detected_failure\n";
    f<<(expectedFailure?1:0)<<','<<(detectedFailure?1:0)<<'\n';
    recordEvaluationInMemory(expectedFailure,detectedFailure);
}
MetricsSummary MetricsRecorder::summarize(const std::vector<Incident>& incidents) const {
    MetricsSummary s;s.samples=totalSamples_;s.availabilityPct=totalSamples_?100.0*upSamples_/totalSamples_:100.0;
    s.incidents=incidents.size();long long total=0;unsigned long long n=0;
    for(const auto& i:incidents){if(i.status==IncidentStatus::Resolved){++s.resolved;if(i.recoveryMs>0){total+=i.recoveryMs;++n;}}if(i.status==IncidentStatus::RecoveryFailed)++s.failedRecoveries;}
    s.meanRecoveryMs=n?static_cast<double>(total)/n:0;
    s.truePositive=detections_.count("tp")?detections_.at("tp"):0;s.falsePositive=detections_.count("fp")?detections_.at("fp"):0;
    s.trueNegative=detections_.count("tn")?detections_.at("tn"):0;s.falseNegative=detections_.count("fn")?detections_.at("fn"):0;
    auto div=[](double a,double b){return b?a/b:0.0;};s.detectionPrecision=div(s.truePositive,s.truePositive+s.falsePositive);s.detectionRecall=div(s.truePositive,s.truePositive+s.falseNegative);return s;
}
