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

    Column& createTemporaryColumn(const std::string& column_name, std::unique_ptr<DataType> type);

private:
    std::reference_wrapper<Catalog> catalog_;
    std::vector<std::unique_ptr<Column>> temporary_columns_;
};

} // namespace velodb
