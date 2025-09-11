#pragma once

#include "expression.hpp"

#include <string>
#include <vector>

namespace velodb {

// Function call expression (placeholder for future extension)
class FunctionCallExpression : public AbstractExpression {
public:
    FunctionCallExpression(std::string function_name,
                           std::vector<std::unique_ptr<AbstractExpression>> arguments,
                           std::unique_ptr<DataType> return_type);
    ~FunctionCallExpression() override = default;

    const Value evaluate(const Tuple& tuple, const Schema& schema) const override;
    std::string toString() const override;

protected:
    std::unique_ptr<AbstractExpression> cloneUniqueImpl() const override;

private:
    std::string function_name_;
    std::vector<std::unique_ptr<AbstractExpression>> arguments_;
};

} // namespace velodb
