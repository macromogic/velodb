#pragma once

namespace velodb {

// Forward declarations
class Catalog;

// Execution context for operators
class ExecutionContext {
public:
    explicit ExecutionContext(Catalog& catalog);
    ~ExecutionContext() = default;

    Catalog& getCatalog() const { return catalog_; }

private:
    Catalog& catalog_;
};

} // namespace velodb
