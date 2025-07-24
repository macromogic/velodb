#include "planner/execution_context.hpp"

namespace velodb {

// ExecutionContext implementation
ExecutionContext::ExecutionContext(Catalog& catalog)
    : catalog_(catalog)
{
}

} // namespace velodb
