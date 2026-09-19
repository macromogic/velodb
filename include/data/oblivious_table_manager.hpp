#pragma once

#include "common/copy_traits.hpp"
#include "data/oblivious_table.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <cuda_runtime.h>

namespace velodb {

// Forward declarations
class Catalog;
class Table;
class TaskManager;

class ObliviousTableManager : private NonCopyable {
public:
    explicit ObliviousTableManager(TaskManager& task_manager);
    ~ObliviousTableManager();

    // Move support
    ObliviousTableManager(ObliviousTableManager&& other) noexcept;
    ObliviousTableManager& operator=(ObliviousTableManager&& other) noexcept;

    // ========================================================================
    // Table Registration
    // ========================================================================

    void registerTable(const std::string& name, Table* source);

    bool hasTable(const std::string& name) const;

    ObliviousTable& getTable(const std::string& name);
    const ObliviousTable& getTable(const std::string& name) const;

    // ========================================================================
    // Query Lifecycle
    // ========================================================================

    void beginQuery(const std::vector<std::string>& table_names);

    // Complete the query-triggered background shuffles, then perform and
    // profile one controlled end-to-end shuffle for every table accessed by
    // the most recent query.
    void profileAccessedTableShuffles();

    void markAccessed(const std::string& name);

    void endQuery();

    // ========================================================================
    // Position Enrichment (GPU)
    // ========================================================================

    void enrichPositions(const std::string& table_name, const uint32_t* d_record_ids, uint32_t* d_positions, size_t n);

    // ========================================================================
    // Utility
    // ========================================================================

    void syncAllShuffles();

    struct Stats {
        size_t total_shuffles = 0;
        size_t tables_registered = 0;
    };
    Stats getStats() const;

private:
    TaskManager& task_manager_;

    // Oblivious tables
    std::unordered_map<std::string, std::unique_ptr<ObliviousTable>> tables_;
    mutable std::mutex tables_mutex_;

    // Current query state
    std::unordered_set<std::string> accessed_tables_;
    std::mutex query_mutex_;

    // Statistics
    mutable std::mutex stats_mutex_;
    Stats stats_;
};

} // namespace velodb
