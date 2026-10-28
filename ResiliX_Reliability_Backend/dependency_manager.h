#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>

class DependencyManager {
    std::map<std::string, std::vector<std::string>> dependents_; // upstream -> downstream
    bool reaches(const std::string& from, const std::string& target) const;
public:
    void addService(const std::string& name);
    bool addDependency(const std::string& upstream, const std::string& downstream);
    std::vector<std::string> directDependents(const std::string& name) const;
    std::vector<std::string> allDownstream(const std::string& name) const;
    std::vector<std::pair<std::string,std::string>> edges() const;
};
