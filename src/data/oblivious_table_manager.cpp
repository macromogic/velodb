#include "data/oblivious_table_manager.hpp"

#include "common/profiler.hpp"
#include "cuda/commands.hpp"
#include "cuda/stream_pool.hpp"
#include "cuda/task_manager.hpp"

namespace velodb {

// ============================================================================
// Constructor / Destructor
// ============================================================================

ObliviousTableManager::ObliviousTableManager(TaskManager& task_manager)
    : task_manager_(task_manager)
{
}

ObliviousTableManager::~ObliviousTableManager()
{
    // Wait for all shuffles to complete
    syncAllShuffles();
}

ObliviousTableManager::ObliviousTableManager(ObliviousTableManager&& other) noexcept
    : task_manager_(other.task_manager_)
    , tables_(std::move(other.tables_))
    , accessed_tables_(std::move(other.accessed_tables_))
    , stats_(other.stats_)
{
}

ObliviousTableManager& ObliviousTableManager::operator=(ObliviousTableManager&& other) noexcept
{
    if (this != &other) {
        syncAllShuffles();

        // Note: task_manager_ is a reference, can't reassign
        tables_ = std::move(other.tables_);
        accessed_tables_ = std::move(other.accessed_tables_);
        stats_ = other.stats_;
    }
    return *this;
}

// ============================================================================
// Table Registration
// ============================================================================

void ObliviousTableManager::registerTable(const std::string& name, Table* source)
{
    std::lock_guard<std::mutex> lock(tables_mutex_);

    auto table = std::make_unique<ObliviousTable>(name, source, task_manager_);
    tables_[name] = std::move(table);

    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.tables_registered++;
}

bool ObliviousTableManager::hasTable(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(tables_mutex_);
    return tables_.find(name) != tables_.end();
}

ObliviousTable& ObliviousTableManager::getTable(const std::string& name)
{
    std::lock_guard<std::mutex> lock(tables_mutex_);
    auto it = tables_.find(name);
    if (it == tables_.end()) {
        throw std::runtime_error("Oblivious table not found: " + name);
    }
    return *it->second;
}

const ObliviousTable& ObliviousTableManager::getTable(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(tables_mutex_);
    auto it = tables_.find(name);
    if (it == tables_.end()) {
        throw std::runtime_error("Oblivious table not found: " + name);
    }
    return *it->second;
}

// ============================================================================
// Query Lifecycle
// ============================================================================

void ObliviousTableManager::beginQuery(const std::vector<std::string>& table_names)
{
    std::lock_guard<std::mutex> lock(query_mutex_);

    PROFILE_SCOPE("ObliviousTableManager::beginQuery");

    // Clear accessed tables from previous query
    accessed_tables_.clear();

    // Wait for any pending shuffles on tables we'll use
    for (const auto& name : table_names) {
        if (hasTable(name)) {
            auto& table = getTable(name);
            if (table.isShuffling()) {
                PROFILE_SCOPE("Wait for shuffle: " + name);
                table.waitForShuffle();
            }
            // Ensure position map is on GPU
            table.syncPositionMapToGPU();
        }
    }
}

void ObliviousTableManager::markAccessed(const std::string& name)
{
    std::lock_guard<std::mutex> lock(query_mutex_);
    accessed_tables_.insert(name);
}

void ObliviousTableManager::endQuery()
{
    std::lock_guard<std::mutex> lock(query_mutex_);

    PROFILE_SCOPE("ObliviousTableManager::endQuery");

    // Trigger async shuffle for all accessed tables
    for (const auto& name : accessed_tables_) {
        if (hasTable(name)) {
            auto& table = getTable(name);
            table.startAsyncShuffle();

            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.total_shuffles++;
        }
    }

    // Don't clear accessed_tables_ here - let beginQuery do it
    // This allows inspection of which tables were accessed
}

// ============================================================================
// Position Enrichment
// ============================================================================

void ObliviousTableManager::enrichPositions(const std::string& table_name,
                                            const uint32_t* d_record_ids,
                                            uint32_t* d_positions,
                                            size_t n,
                                            cudaStream_t stream)
{
    PROFILE_SCOPE("ObliviousTableManager::enrichPositions");

    const auto& table = getTable(table_name);
    uint32_t* d_pos_map = table.getPositionMapDevice();

    // Submit command to persistent kernel
    Command cmd = {};
    cmd.opcode = OpCode::OP_ENRICH_POSITIONS;
    cmd.args.enrich_positions
        = { .record_ids = d_record_ids, .position_map = d_pos_map, .positions = d_positions, .n = n };

    auto cmd_id = task_manager_.submitCommand(cmd);
    task_manager_.waitCommand(cmd_id);
}

// ============================================================================
// Utility
// ============================================================================

void ObliviousTableManager::syncAllShuffles()
{
    std::lock_guard<std::mutex> lock(tables_mutex_);

    for (auto& [name, table] : tables_) {
        if (table->isShuffling()) {
            table->waitForShuffle();
        }
    }
}

ObliviousTableManager::Stats ObliviousTableManager::getStats() const
{
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

} // namespace velodb
