#include "expression/logical_expression.hpp"

#include "catalog/column.hpp"
#include "catalog/row_batch.hpp"
#include "common/fmt.hpp"
#include "common/profiler.hpp"
#include "expression/comparison_expression.hpp"

#include <fmt/format.h>

#include <algorithm>

namespace velodb {

// Helper to flatten AND chain into a list of leaf expressions
static void flattenAndChain(const AbstractExpression* expr, std::vector<const AbstractExpression*>& leaves)
{
    if (auto* logical = dynamic_cast<const BinaryLogicalExpression*>(expr)) {
        if (logical->getConnectiveType() == ConnectiveType::AND) {
            flattenAndChain(&logical->getLeftExpression(), leaves);
            flattenAndChain(&logical->getRightExpression(), leaves);
            return;
        }
    }
    leaves.push_back(expr);
}

BinaryLogicalExpression::BinaryLogicalExpression(ConnectiveType connective_type,
                                                 std::unique_ptr<AbstractExpression> left,
                                                 std::unique_ptr<AbstractExpression> right)
    : BinaryExpression(ExpressionType::LOGICAL, std::make_unique<BooleanType>(), std::move(left), std::move(right))
    , connective_type_(connective_type)
{
}

const Value BinaryLogicalExpression::evaluate(const Tuple& tuple, const Schema& schema) const
{
    Value left_value = left_->evaluate(tuple, schema);
    Value right_value = right_->evaluate(tuple, schema);

    if (left_value.isNull() || right_value.isNull()) {
        return Value::createNull(DataTypeId::BOOLEAN);
    }

    bool left_bool = left_value.getBoolean();
    bool right_bool = right_value.getBoolean();

    if (connective_type_ == ConnectiveType::AND) {
        if (debug_flag_) {
            fmt::println("Evaluating AND: {} AND {} => {}", left_bool, right_bool, left_bool && right_bool);
        }
        return Value::createBoolean(left_bool && right_bool);
    } else { // OR
        if (debug_flag_) {
            fmt::println("Evaluating OR: {} OR {} => {}", left_bool, right_bool, left_bool || right_bool);
        }
        return Value::createBoolean(left_bool || right_bool);
    }
}

Column BinaryLogicalExpression::evaluateBatch(const RowBatch& batch, const Schema& schema) const
{
    size_t count = batch.getRowCount();

    // Optimization: flatten AND chains to evaluate all conditions into a single result
    // This avoids creating intermediate Column objects for (A AND B AND C AND D)
    if (connective_type_ == ConnectiveType::AND) {
        std::vector<const AbstractExpression*> leaves;
        flattenAndChain(this, leaves);

        if (leaves.size() > 2) {
            // Fused AND evaluation: evaluate all leaves and AND them together
            Column result(DataType::createType(DataTypeId::BOOLEAN), count);
            uint8_t* res_data = static_cast<uint8_t*>(result.rawData());

            // Initialize result to all 1s (true)
            std::fill_n(res_data, count, static_cast<uint8_t>(1));

            // Evaluate each leaf and AND into result
            for (const auto* leaf : leaves) {
                Column leaf_col = leaf->evaluateBatch(batch, schema);
                const uint8_t* leaf_data = static_cast<const uint8_t*>(leaf_col.rawData());
                const auto* leaf_nulls = leaf_col.rawBitmapData();

                if (leaf_nulls == nullptr) {
                    // Fast path: AND without null checks
                    for (size_t i = 0; i < count; ++i) {
                        res_data[i] &= leaf_data[i];
                    }
                } else {
                    // With nulls - any null makes result null (for AND, we set to 0)
                    for (size_t i = 0; i < count; ++i) {
                        if (leaf_nulls[i]) {
                            res_data[i] = 0;
                        } else {
                            res_data[i] &= leaf_data[i];
                        }
                    }
                }
            }
            setSizeForColumn(result, count);
            return result;
        }
    }

    // Standard path for OR or simple AND
    Column left_col = left_->evaluateBatch(batch, schema);
    Column right_col = right_->evaluateBatch(batch, schema);

    Column result(DataType::createType(DataTypeId::BOOLEAN), count);

    // Assume boolean inputs
    VELODB_ASSERT_MSG(left_col.getType().isBoolean() && right_col.getType().isBoolean(),
                      "BinaryLogicalExpression only supports BOOLEAN type");

    const uint8_t* l_data = static_cast<const uint8_t*>(left_col.rawData());
    const uint8_t* r_data = static_cast<const uint8_t*>(right_col.rawData());
    const auto* l_nulls = left_col.rawBitmapData();
    const auto* r_nulls = right_col.rawBitmapData();
    uint8_t* res_data = static_cast<uint8_t*>(result.rawData());
    BitVector::Element* res_nulls = result.rawBitmapData();

    bool has_l_nulls = (l_nulls != nullptr);
    bool has_r_nulls = (r_nulls != nullptr);

    if (!has_l_nulls && !has_r_nulls) {
        // Fast path: no null checks needed - use tight loop for auto-vectorization
        if (connective_type_ == ConnectiveType::AND) {
            for (size_t i = 0; i < count; ++i) {
                res_data[i] = l_data[i] & r_data[i];
            }
        } else {
            for (size_t i = 0; i < count; ++i) {
                res_data[i] = l_data[i] | r_data[i];
            }
        }
    } else {
        // Slow path: need null checks
        for (size_t i = 0; i < count; ++i) {
            if ((has_l_nulls && l_nulls[i]) || (has_r_nulls && r_nulls[i])) {
                res_nulls[i] = 1; // Set NULL
                res_data[i] = 0;
                continue;
            }
            if (connective_type_ == ConnectiveType::AND) {
                res_data[i] = l_data[i] & r_data[i];
            } else {
                res_data[i] = l_data[i] | r_data[i];
            }
        }
    }
    setSizeForColumn(result, count);
    return result;
}

std::string BinaryLogicalExpression::toString() const
{
    std::string op_str = (connective_type_ == ConnectiveType::AND) ? " AND " : " OR ";
    return fmt::format("({} {} {})", left_->toString(), op_str, right_->toString());
}

std::unique_ptr<AbstractExpression> BinaryLogicalExpression::cloneUniqueImpl() const
{
    return std::make_unique<BinaryLogicalExpression>(connective_type_, left_->cloneUnique(), right_->cloneUnique());
}

// LogicalNotExpression implementation

LogicalNotExpression::LogicalNotExpression(std::unique_ptr<AbstractExpression> operand)
    : UnaryExpression(ExpressionType::LOGICAL, std::make_unique<BooleanType>(), std::move(operand))
{
}

const Value LogicalNotExpression::evaluate(const Tuple& tuple, const Schema& schema) const
{
    Value operand_value = operand_->evaluate(tuple, schema);
    if (operand_value.isNull()) {
        return Value::createNull(DataTypeId::BOOLEAN);
    }
    return Value::createBoolean(!operand_value.getBoolean());
}

Column LogicalNotExpression::evaluateBatch(const RowBatch& batch, const Schema& schema) const
{
    Column op_col = operand_->evaluateBatch(batch, schema);
    size_t count = batch.getRowCount();
    Column result(DataType::createType(DataTypeId::BOOLEAN), count);

    if (op_col.getType().getTypeId() == DataTypeId::BOOLEAN) {
        const bool* d = static_cast<const bool*>(op_col.rawData());
        const auto* nulls = op_col.rawBitmapData();
        for (size_t i = 0; i < count; ++i) {
            if (nulls && nulls[i]) {
                result.append(Value::createNull(DataTypeId::BOOLEAN));
            } else {
                result.append(Value::createBoolean(!d[i]));
            }
        }
    }
    return result;
}

std::string LogicalNotExpression::toString() const
{
    return fmt::format("NOT ({})", *operand_);
}

std::unique_ptr<AbstractExpression> LogicalNotExpression::cloneUniqueImpl() const
{
    return std::make_unique<LogicalNotExpression>(operand_->cloneUnique());
}

} // namespace velodb
