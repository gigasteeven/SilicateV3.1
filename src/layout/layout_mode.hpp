#pragma once

#include <Geode/Geode.hpp>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <vector>

using namespace geode::prelude;

// ── Layout Mode object tables (ported from XDBotFork) ──────
// Forward-declare the tables so they can be shared between TUs

struct LevelObject {
    std::vector<std::string> vecString;
    std::map<int, std::string> props;
    size_t hiddenIndex = 0;
    std::string ogString;
};

class LayoutMode {
public:
    static std::string getModifiedString(std::string levelString);
    static std::string mergeVector(std::vector<std::string> vec, std::string separator = ",");
};

// ── Object ID tables ───────────────────────────────────────
extern const std::string newColors;
extern const std::unordered_set<int> robtopLevelIDs;
extern const std::unordered_set<int> excludedTriggerIDs;
extern const std::map<int, std::vector<int>> importantTriggerIDs;
extern const std::unordered_set<int> decoObjectIDs;
extern const std::unordered_set<int> solidObjectIDs;
