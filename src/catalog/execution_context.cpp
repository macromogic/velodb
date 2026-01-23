#include "catalog/execution_context.hpp"

namespace velodb {

ExecutionContext::ExecutionContext(Catalog& catalog, TaskManager& task_manager)
    : catalog_(catalog)
    , task_manager_(task_manager)
{
}

} // namespace velodb
