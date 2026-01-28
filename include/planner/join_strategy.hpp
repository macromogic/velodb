#pragma once

#include <stdexcept>
#include <string>

namespace velodb {

enum class JoinStrategy {
    SORT_MERGE_JOIN,
    HASH_JOIN
};

inline JoinStrategy parseJoinStrategy(const std::string& str)
{
    if (str == "sort_merge" || str == "sort-merge" || str == "smj") {
        return JoinStrategy::SORT_MERGE_JOIN;
    } else if (str == "hash" || str == "hj") {
        return JoinStrategy::HASH_JOIN;
    }
    throw std::invalid_argument("Unknown join strategy: " + str + ". Use 'sort_merge' or 'hash'.");
}

inline const char* joinStrategyToString(JoinStrategy strategy)
{
    switch (strategy) {
    case JoinStrategy::SORT_MERGE_JOIN:
        return "sort_merge";
    case JoinStrategy::HASH_JOIN:
        return "hash";
    }
    return "unknown";
}

} // namespace velodb
