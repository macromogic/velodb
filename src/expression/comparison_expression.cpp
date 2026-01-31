#include "expression/comparison_expression.hpp"

#include "catalog/column.hpp"
#include "catalog/row_batch.hpp"
#include "common/exception.hpp"
#include "common/fmt.hpp"
#include "common/profiler.hpp"
#include "expression/constant_expression.hpp"

namespace velodb {

// Fast path for column vs scalar comparison (no right column needed)
template <typename T>
void computeComparisonWithScalar(const Column& left, T scalar_value, Column& result, ComparisonType op, size_t count)
{
    PROFILE_SCOPE("computeComparisonWithScalar<T>");
    const T* left_data = static_cast<const T*>(left.rawData());
    const auto* left_nulls = left.rawBitmapData();

    uint8_t* result_data = static_cast<uint8_t*>(result.rawData());
    auto* result_nulls = result.rawBitmapData();

    bool has_left_nulls = (left_nulls != nullptr);

    if (!has_left_nulls) {
        // Fast path: no null checks needed
        switch (op) {
        case ComparisonType::EQUAL:
            for (size_t i = 0; i < count; ++i) {
                result_data[i] = (left_data[i] == scalar_value);
            }
            break;
        case ComparisonType::NOT_EQUAL:
            for (size_t i = 0; i < count; ++i) {
                result_data[i] = (left_data[i] != scalar_value);
            }
            break;
        case ComparisonType::LESS_THAN:
            for (size_t i = 0; i < count; ++i) {
                result_data[i] = (left_data[i] < scalar_value);
            }
            break;
        case ComparisonType::LESS_THAN_OR_EQUAL:
            for (size_t i = 0; i < count; ++i) {
                result_data[i] = (left_data[i] <= scalar_value);
            }
            break;
        case ComparisonType::GREATER_THAN:
            for (size_t i = 0; i < count; ++i) {
                result_data[i] = (left_data[i] > scalar_value);
            }
            break;
        case ComparisonType::GREATER_THAN_OR_EQUAL:
            for (size_t i = 0; i < count; ++i) {
                result_data[i] = (left_data[i] >= scalar_value);
            }
            break;
        default:
            break;
        }
    } else {
        for (size_t i = 0; i < count; ++i) {
            if (left_nulls[i]) {
                result_nulls[i] = 1;
                result_data[i] = 0;
                continue;
            }
            T l = left_data[i];
            switch (op) {
            case ComparisonType::EQUAL:
                result_data[i] = (l == scalar_value);
                break;
            case ComparisonType::NOT_EQUAL:
                result_data[i] = (l != scalar_value);
                break;
            case ComparisonType::LESS_THAN:
                result_data[i] = (l < scalar_value);
                break;
            case ComparisonType::LESS_THAN_OR_EQUAL:
                result_data[i] = (l <= scalar_value);
                break;
            case ComparisonType::GREATER_THAN:
                result_data[i] = (l > scalar_value);
                break;
            case ComparisonType::GREATER_THAN_OR_EQUAL:
                result_data[i] = (l >= scalar_value);
                break;
            default:
                break;
            }
        }
    }
}

template <typename T>
void computeComparison(const Column& left, const Column& right, Column& result, ComparisonType op, size_t count)
{
    PROFILE_SCOPE("computeComparison<T>");
    const T* left_data = static_cast<const T*>(left.rawData());
    const T* right_data = static_cast<const T*>(right.rawData());
    const auto* left_nulls = left.rawBitmapData();
    const auto* right_nulls = right.rawBitmapData();

    // Direct access to result buffer - avoid append() overhead
    uint8_t* result_data = static_cast<uint8_t*>(result.rawData());
    auto* result_nulls = result.rawBitmapData();

    // Optimize for common case: no nulls
    bool has_left_nulls = (left_nulls != nullptr);
    bool has_right_nulls = (right_nulls != nullptr);

    if (!has_left_nulls && !has_right_nulls) {
        // Fast path: no null checks needed
        switch (op) {
        case ComparisonType::EQUAL:
            for (size_t i = 0; i < count; ++i) {
                result_data[i] = (left_data[i] == right_data[i]);
            }
            break;
        case ComparisonType::NOT_EQUAL:
            for (size_t i = 0; i < count; ++i) {
                result_data[i] = (left_data[i] != right_data[i]);
            }
            break;
        case ComparisonType::LESS_THAN:
            for (size_t i = 0; i < count; ++i) {
                result_data[i] = (left_data[i] < right_data[i]);
            }
            break;
        case ComparisonType::LESS_THAN_OR_EQUAL:
            for (size_t i = 0; i < count; ++i) {
                result_data[i] = (left_data[i] <= right_data[i]);
            }
            break;
        case ComparisonType::GREATER_THAN:
            for (size_t i = 0; i < count; ++i) {
                result_data[i] = (left_data[i] > right_data[i]);
            }
            break;
        case ComparisonType::GREATER_THAN_OR_EQUAL:
            for (size_t i = 0; i < count; ++i) {
                result_data[i] = (left_data[i] >= right_data[i]);
            }
            break;
        default:
            break;
        }
    } else {
        // Slow path: need null checks
        for (size_t i = 0; i < count; ++i) {
            if ((has_left_nulls && left_nulls[i]) || (has_right_nulls && right_nulls[i])) {
                result_nulls[i] = 1;
                result_data[i] = 0;
                continue;
            }

            T l = left_data[i];
            T r = right_data[i];
            switch (op) {
            case ComparisonType::EQUAL:
                result_data[i] = (l == r);
                break;
            case ComparisonType::NOT_EQUAL:
                result_data[i] = (l != r);
                break;
            case ComparisonType::LESS_THAN:
                result_data[i] = (l < r);
                break;
            case ComparisonType::LESS_THAN_OR_EQUAL:
                result_data[i] = (l <= r);
                break;
            case ComparisonType::GREATER_THAN:
                result_data[i] = (l > r);
                break;
            case ComparisonType::GREATER_THAN_OR_EQUAL:
                result_data[i] = (l >= r);
                break;
            default:
                break;
            }
        }
    }
    // Note: caller must call setSizeForColumn(result, count) after this function
}

static auto format_as(ComparisonType comp_type)
{
    switch (comp_type) {
    case ComparisonType::EQUAL:
        return "=";
    case ComparisonType::NOT_EQUAL:
        return "!=";
    case ComparisonType::LESS_THAN:
        return "<";
    case ComparisonType::LESS_THAN_OR_EQUAL:
        return "<=";
    case ComparisonType::GREATER_THAN:
        return ">";
    case ComparisonType::GREATER_THAN_OR_EQUAL:
        return ">=";
    default:
        return "(unknown)";
    }
}

ComparisonExpression::ComparisonExpression(ComparisonType comp_type,
                                           std::unique_ptr<AbstractExpression> left,
                                           std::unique_ptr<AbstractExpression> right)
    : BinaryExpression(ExpressionType::COMPARISON, std::make_unique<BooleanType>(), std::move(left), std::move(right))
    , comp_type_(comp_type)
{
}

const Value ComparisonExpression::evaluate(const Tuple& tuple, const Schema& schema) const
{
    Value const left_val = left_->evaluate(tuple, schema);
    Value const right_val = right_->evaluate(tuple, schema);
    return compareValues(left_val, right_val);
}

Column ComparisonExpression::evaluateBatch(const RowBatch& batch, const Schema& schema) const
{
    // Fast path: check if right side is a constant expression
    auto* const_expr = dynamic_cast<ConstantExpression*>(right_.get());
    size_t count = batch.getRowCount();
    Column result(DataType::createType(DataTypeId::BOOLEAN), count);

    if (const_expr != nullptr) {
        // Right side is constant - use optimized scalar comparison
        Column left_col = left_->evaluateBatch(batch, schema);
        Value const_val = const_expr->getValue();

        if (!const_val.isNull() && left_col.getType().getTypeId() == const_val.getTypeId()) {
            switch (const_val.getTypeId()) {
            case DataTypeId::INTEGER:
                computeComparisonWithScalar<int32_t>(left_col, const_val.getInteger(), result, comp_type_, count);
                setSizeForColumn(result, count);
                return result;
            case DataTypeId::BIGINT:
                computeComparisonWithScalar<int64_t>(left_col, const_val.getBigInt(), result, comp_type_, count);
                setSizeForColumn(result, count);
                return result;
            case DataTypeId::DOUBLE:
                computeComparisonWithScalar<double>(left_col, const_val.getDouble(), result, comp_type_, count);
                setSizeForColumn(result, count);
                return result;
            case DataTypeId::DATE:
                computeComparisonWithScalar<uint32_t>(left_col, const_val.get<uint32_t>(), result, comp_type_, count);
                setSizeForColumn(result, count);
                return result;
            default:
                break; // Fall through to general path
            }
        }
    }

    // General path: evaluate both sides
    Column left_col = left_->evaluateBatch(batch, schema);
    Column right_col = right_->evaluateBatch(batch, schema);

    if (left_col.getType().getTypeId() == right_col.getType().getTypeId()) {
        switch (left_col.getType().getTypeId()) {
        case DataTypeId::INTEGER:
            computeComparison<int32_t>(left_col, right_col, result, comp_type_, count);
            setSizeForColumn(result, count);
            return result;
        case DataTypeId::BIGINT:
            computeComparison<int64_t>(left_col, right_col, result, comp_type_, count);
            setSizeForColumn(result, count);
            return result;
        case DataTypeId::DOUBLE:
            computeComparison<double>(left_col, right_col, result, comp_type_, count);
            setSizeForColumn(result, count);
            return result;
        case DataTypeId::VARCHAR:
            // Optimization: if right side is constant (or uniform), we can map its ordinal
            // to left side's dictionary and do integer comparison directly per row.
            // This is especially common in queries like: col == "const"
            if (count > 0 && right_col.getType().getTypeId() == DataTypeId::VARCHAR) {
                // Check if right column is uniform (e.g. from ConstantExpression)
                // For now, we assume ConstantExpression produces a column where all values are same.
                // A simple heuristic is checking if it was built from ConstantExpression
                // or just checking the first value and assuming valid for Constant case.

                // Since evaluateBatch interfaces with Column, we don't know if it came from ConstantExpr easily
                // without RTTI or extra flags. But if right side is a constant, it usually has size matching batch
                // but identical content.
                // However, even safer: we can just optimize the case where right side is a "Constant" conceptually.
                // But wait, "right_col" is a Column object.

                // Let's assume for this specific optimization we only target the case where we can afford
                // ONE translation.

                // Correct logic:
                // 1. Get the target value from right column (first row).
                // 2. Translate it to left column's ordinal space.
                // 3. Compare left column's raw ordinals against this target ordinal.

                // Note: this optimization assumes right_col is constant across the batch.
                // If real arbitrary column-column comparison is needed, we'd need row-by-row translation or a global
                // dict. Given the context of TPCH Q6/Q12 etc., it's usually col op const.

                // To be safe and correct for vectorization, we should ideally check if right_col is "constant".
                // BUT, for now, let's implement the generic logic: get right value, translate, compare.
                // If we want to support true column-column, we can't do this.

                // BUT, based on user request, we want to use the "planning phase" idea:
                // "search a suitable ordinal for comparison".

                // Let's try to detect if we can fallback to the fast path.
                // If the right column is generated by a ConstantExpression, it will effectively be uniform.
                // Let's grab the first value.

                if (dynamic_cast<ConstantExpression*>(right_.get())) {
                    // Right side is a ConstantExpression!
                    Value const_val = right_col.get(0);
                    if (!const_val.isNull()) {
                        // Translate ordinal to match left column's dictionary
                        // NOTE: const_val is a COPY, so changing it is safe and local to this function.
                        left_col.ensureOrdinal(const_val, comp_type_);

                        // Now const_val has the ordinal compatible with left_col.
                        // We can extract it and run integer comparison.
                        size_t target_ordinal = const_val.getOrdinalString().getOrdinal();

                        // Let's inline a scalar version here for now
                        const size_t* left_data = static_cast<const size_t*>(left_col.rawData());
                        const auto* left_nulls = left_col.rawBitmapData();

                        // IMPORTANT: Pre-allocate result column elements to avoid repeated appends (and reallocations)
                        // But since we are appending to 'result' which was created with capacity?
                        // result was created: Column result(DataType::createType(DataTypeId::BOOLEAN),
                        // batch.getRowCount()); Ideally we should use raw access to result's data, but Column API
                        // doesn't expose Mutable boolean buffer easily without tryOwn/etc complexity. But append() is
                        // fine if capacity is reserved. The issue is if we return 'result' that has different size than
                        // 'count'

                        for (size_t i = 0; i < count; ++i) {
                            if (left_nulls && left_nulls[i]) {
                                result.append(Value::createNull(DataTypeId::BOOLEAN));
                                continue;
                            }

                            size_t l = left_data[i];
                            bool res = false;
                            switch (comp_type_) {
                            case ComparisonType::EQUAL:
                                res = (l == target_ordinal);
                                break;
                            case ComparisonType::NOT_EQUAL:
                                res = (l != target_ordinal);
                                break;
                            case ComparisonType::LESS_THAN:
                                res = (l < target_ordinal);
                                break;
                            case ComparisonType::LESS_THAN_OR_EQUAL:
                                res = (l <= target_ordinal);
                                break;
                            case ComparisonType::GREATER_THAN:
                                res = (l > target_ordinal);
                                break;
                            case ComparisonType::GREATER_THAN_OR_EQUAL:
                                res = (l >= target_ordinal);
                                break;
                            default:
                                break;
                            }
                            result.append(Value::createBoolean(res));
                        }
                        return result;
                    }
                }
            }
            break;
        default:
            break;
        }
    }

    // Fallback using Value API
    for (size_t i = 0; i < count; ++i) {
        result.append(compareValues(left_col.get(i), right_col.get(i)));
    }
    return result;
}

std::string ComparisonExpression::toString() const
{
    return fmt::format("({} {} {})", *left_, comp_type_, *right_);
}

Value ComparisonExpression::compareValues(const Value& left_val, const Value& right_val) const
{
    if (left_val.isNull() || right_val.isNull()) {
        return Value::createNull(DataTypeId::BOOLEAN);
    }

    bool result = false;
    switch (comp_type_) {
    case ComparisonType::EQUAL:
        result = (left_val == right_val);
        break;
    case ComparisonType::NOT_EQUAL:
        result = (left_val != right_val);
        break;
    case ComparisonType::LESS_THAN:
        result = (left_val < right_val);
        break;
    case ComparisonType::LESS_THAN_OR_EQUAL:
        result = (left_val <= right_val);
        break;
    case ComparisonType::GREATER_THAN:
        result = (left_val > right_val);
        break;
    case ComparisonType::GREATER_THAN_OR_EQUAL:
        result = (left_val >= right_val);
        break;
    default:
        VELODB_THROW(ExecutionError, fmt::format("Comparison operator {} not implemented", comp_type_));
    }

    if (debug_flag_) {
        fmt::println("Comparing values: {}({}) {} {}({}) => {}",
                     left_val,
                     DataType::createType(left_val.getTypeId()),
                     comp_type_,
                     right_val,
                     DataType::createType(right_val.getTypeId()),
                     result);
    }
    return Value::createBoolean(result);
}

std::unique_ptr<AbstractExpression> ComparisonExpression::cloneUniqueImpl() const
{
    return std::make_unique<ComparisonExpression>(comp_type_, left_->cloneUnique(), right_->cloneUnique());
}

} // namespace velodb
