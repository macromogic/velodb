#pragma once

#include "catalog/catalog.hpp"
#include "catalog/column.hpp"

namespace velodb {

// Forward declarations
class Catalog;

// Execution context for operators
class ExecutionContext {
public:
    explicit ExecutionContext(Catalog& catalog);
    ~ExecutionContext() = default;

    Catalog& getCatalog() const { return catalog_; }

    Column& createTemporaryColumn(const std::string& column_name, std::unique_ptr<DataType> type);

private:
    Catalog& catalog_;
    std::vector<std::unique_ptr<Column>> temporary_columns_;
};

} // namespace velodb
