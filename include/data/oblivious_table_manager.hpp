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

/**
 * @brief Manages oblivious tables for a database
 *
 * Responsibilities:
 * - Wraps regular Tables with ObliviousTable for security
 * - Tracks which tables were accessed during a query
 * - Triggers async shuffles after query completion
 * - Manages dedicated shuffle stream
 *
 * Usage pattern:
 *   manager.beginQuery({"lineitem", "orders"});  // Wait for pending shuffles
 *   ... execute query ...
 *   manager.markAccessed("lineitem");
 *   manager.markAccessed("orders");
 *   manager.endQuery();  // Trigger async shuffles (non-blocking)
 */
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

    /**
     * @brief Register a table for oblivious protection
     *
     * Creates an ObliviousTable wrapper with initial random shuffle.
     *
     * @param name    Table name
     * @param source  Pointer to the source table (non-owning)
     */
    void registerTable(const std::string& name, Table* source);

    /**
     * @brief Check if a table is registered
     */
    bool hasTable(const std::string& name) const;

    /**
     * @brief Get an oblivious table by name
     */
    ObliviousTable& getTable(const std::string& name);
    const ObliviousTable& getTable(const std::string& name) const;

    // ========================================================================
    // Query Lifecycle
    // ========================================================================

    /**
     * @brief Called before query execution
     *
     * Waits for any pending shuffles on tables that will be accessed,
     * and ensures position maps are synced to GPU.
     *
     * @param table_names  Names of tables that will be accessed
     */
    void beginQuery(const std::vector<std::string>& table_names);

    /**
     * @brief Mark a table as accessed during the current query
     *
     * Tables marked as accessed will be shuffled at endQuery().
     */
    void markAccessed(const std::string& name);

    /**
     * @brief Called after query execution
     *
     * Triggers async shuffles for all accessed tables. Non-blocking.
     */
    void endQuery();

    // ========================================================================
    // Position Enrichment (GPU)
    // ========================================================================

    /**
     * @brief Enrich record IDs with physical positions on GPU
     *
     * Launches a GPU kernel that maps logical record IDs to physical
     * row positions using the table's position map.
     *
     * @param table_name    Name of the table
     * @param d_record_ids  Device pointer to input record IDs
     * @param d_positions   Device pointer to output positions
     * @param n             Number of records
     * @param stream        CUDA stream for the operation
     */
    void enrichPositions(const std::string& table_name,
                         const uint32_t* d_record_ids,
                         uint32_t* d_positions,
                         size_t n,
                         cudaStream_t stream = 0);

    // ========================================================================
    // Utility
    // ========================================================================

    /**
     * @brief Wait for all pending shuffles to complete
     */
    void syncAllShuffles();

    /**
     * @brief Get statistics
     */
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
