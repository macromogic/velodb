#pragma once

#include "catalog/catalog.hpp"
#include "catalog/column.hpp"
#include "common/copy_traits.hpp"

namespace velodb {

// Forward declarations
class Catalog;

// Execution context for operators
class ExecutionContext : private NonCopyable {
public:
    explicit ExecutionContext(Catalog& catalog);
    ~ExecutionContext() = default;

    // Add move constructor and assignment
    ExecutionContext(ExecutionContext&& other) noexcept = default;
    ExecutionContext& operator=(ExecutionContext&& other) noexcept = default;

    Catalog& getCatalog() const { return catalog_; }

private:
    std::reference_wrapper<Catalog> catalog_;
    std::vector<std::unique_ptr<Column>> temporary_columns_;
};

} // namespace velodb
