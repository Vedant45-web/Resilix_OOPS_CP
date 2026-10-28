#pragma once
#include "service.h"
#include "incident.h"
#include <string>
#include <vector>
#include <map>

struct MetricsSummary {
    unsigned long long samples = 0;
    unsigned long long incidents = 0;
    unsigned long long resolved = 0;
    unsigned long long failedRecoveries = 0;
    double meanRecoveryMs = 0;
    double availabilityPct = 100;
    double detectionPrecision = 0;
    double detectionRecall = 0;
    unsigned long long truePositive = 0, falsePositive = 0, trueNegative = 0, falseNegative = 0;
};
class MetricsRecorder {
    std::string path_;
    std::string evaluationPath_;
    unsigned long long totalSamples_ = 0, upSamples_ = 0;
    std::map<std::string, unsigned long long> detections_;
public:
    explicit MetricsRecorder(std::string path = "metrics.csv");
    void recordSample(const std::string& service, const Metrics& metrics, const std::string& status);
    void recordEvaluation(bool expectedFailure, bool detectedFailure);
    MetricsSummary summarize(const std::vector<Incident>& incidents) const;
    void loadCounters();
private:
    void recordEvaluationInMemory(bool expectedFailure, bool detectedFailure);
};
