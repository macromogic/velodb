#pragma once

#include "execution/operator/abstract_operator.hpp"
#include <memory>
#include <vector>

namespace velodb {

// Forward declarations
class AbstractExpression;

// Projection operator
class ProjectionOperator : public AbstractOperator {
public:
    ProjectionOperator(std::unique_ptr<AbstractOperator> child,
        std::vector<std::unique_ptr<AbstractExpression>> expressions);
    ~ProjectionOperator() override = default;

    void init() override;
    void reset() override;

    // Late materialization interface
    bool nextRowId(RowId* row_id) override;
    void materializeRowIds(const std::vector<RowId>& row_ids,
        const std::vector<size_t>& column_indices,
        std::vector<Tuple>* tuples) override;

private:
    std::unique_ptr<AbstractOperator> child_;
    std::vector<std::unique_ptr<AbstractExpression>> expressions_;
};

} // namespace velodb
