#pragma once

#include "catalog/catalog.hpp"
#include "catalog/column.hpp"
#include "common/copy_traits.hpp"
#include "cuda/task_manager.hpp"

namespace velodb {

// Forward declarations
class Catalog;

// Execution context for operators
class ExecutionContext : private NonCopyable {
public:
    ExecutionContext(Catalog& catalog, TaskManager& task_manager);
    ~ExecutionContext() = default;

    // Add move constructor and assignment
    ExecutionContext(ExecutionContext&& other) noexcept = default;
    ExecutionContext& operator=(ExecutionContext&& other) noexcept = default;

    Catalog& getCatalog() const { return catalog_; }
    TaskManager& getTaskManager() const { return task_manager_; }

private:
    std::reference_wrapper<Catalog> catalog_;
    std::reference_wrapper<TaskManager> task_manager_;
};

} // namespace velodb
