#include "dependency_manager.h"
#include <algorithm>

void DependencyManager::addService(const std::string& n) { dependents_[n]; }
bool DependencyManager::reaches(const std::string& from, const std::string& target) const {
    if (from == target) return true;
    auto it = dependents_.find(from);
    if (it == dependents_.end()) return false;
    for (const auto& next : it->second) if (reaches(next, target)) return true;
    return false;
}
bool DependencyManager::addDependency(const std::string& up, const std::string& down) {
    addService(up); addService(down);
    if (up == down || reaches(down, up)) return false;
    auto& v = dependents_[up];
    if (std::find(v.begin(), v.end(), down) == v.end()) v.push_back(down);
    return true;
}
std::vector<std::string> DependencyManager::directDependents(const std::string& n) const {
    auto it = dependents_.find(n); return it == dependents_.end() ? std::vector<std::string>{} : it->second;
}
std::vector<std::string> DependencyManager::allDownstream(const std::string& n) const {
    std::set<std::string> seen;
    std::vector<std::string> todo = directDependents(n);
    while (!todo.empty()) {
        auto x = todo.back(); todo.pop_back();
        if (seen.insert(x).second) {
            auto more = directDependents(x); todo.insert(todo.end(), more.begin(), more.end());
        }
    }
    return {seen.begin(), seen.end()};
}
std::vector<std::pair<std::string,std::string>> DependencyManager::edges() const {
    std::vector<std::pair<std::string,std::string>> out;
    for (const auto& p : dependents_) for (const auto& d : p.second) out.emplace_back(p.first,d);
    return out;
}
