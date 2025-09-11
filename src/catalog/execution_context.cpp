#include "catalog/execution_context.hpp"

namespace velodb {

// ExecutionContext implementation
ExecutionContext::ExecutionContext(Catalog& catalog)
    : catalog_(catalog)
{
}

Column& ExecutionContext::createTemporaryColumn([[maybe_unused]] const std::string& column_name,
                                                std::unique_ptr<DataType> type)
{
    auto column = std::make_unique<Column>(std::move(type));
    auto& ref = *column;
    temporary_columns_.push_back(std::move(column));
    return ref;
}

} // namespace velodb
