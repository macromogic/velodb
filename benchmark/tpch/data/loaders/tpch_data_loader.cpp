#include "tpch_data_loader.hpp"

#include "common/fmt.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>
#include <unordered_set>

namespace velodb::benchmark::tpch {

TPCHDataLoader::TPCHDataLoader(Catalog& catalog)
    : catalog_(catalog)
{
}

Result<TPCHDataLoader::LoadStatistics> TPCHDataLoader::loadAllTables(const LoadConfig& config)
{
    LoadStatistics stats;
    auto start_time = std::chrono::steady_clock::now();

    // Get all table names in dependency order (no foreign key constraints for now)
    std::vector<std::string> table_names = { "region", "nation",   "customer", "supplier",
                                             "part",   "partsupp", "orders",   "lineitem" };

    if (config.verbose) {
        fmt::println("Loading TPC-H data (SF={}) from: {}", config.scale_factor, config.data_directory);
    }

    for (const auto& table_name : table_names) {
        auto table_start = std::chrono::steady_clock::now();

        auto result = loadTable(table_name, config);
        if (!result) {
            stats.error_message = fmt::format("Failed to load table {}: {}", table_name, result.error());
            return Result<LoadStatistics>::failure(stats.error_message);
        }

        auto table_end = std::chrono::steady_clock::now();
        std::chrono::duration<double> table_time = table_end - table_start;

        size_t rows_loaded = result.value();
        stats.total_rows_loaded += rows_loaded;
        stats.table_row_counts[table_name] = rows_loaded;
        stats.table_load_times[table_name] = table_time;

        if (config.verbose) {
            fmt::println("  {}: {:>8} rows in {:.3f}s", table_name, rows_loaded, table_time.count());
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    stats.total_load_time = end_time - start_time;

    // Validate data if requested
    if (config.validate_data) {
        auto validation_result = validateLoadedData(config.scale_factor);
        if (!validation_result) {
            stats.validation_passed = false;
            stats.error_message = validation_result.error();
        } else {
            stats.validation_passed = validation_result.value();
        }
    }

    if (config.verbose) {
        fmt::println("Total: {:>8} rows in {:.3f}s", stats.total_rows_loaded, stats.total_load_time.count());
    }

    return Result<LoadStatistics>::success(std::move(stats));
}

Result<size_t> TPCHDataLoader::loadTable(const std::string& table_name, const LoadConfig& config)
{
    std::string file_path = config.data_directory + "/" + getTableFileName(table_name);

    // Check if file exists
    if (!std::filesystem::exists(file_path)) {
        return Result<size_t>::failure(fmt::format("Data file not found: {}", file_path));
    }

    // Get schema from catalog if table exists, otherwise create it
    if (catalog_.hasTable(table_name)) {
        auto table_opt = catalog_.getTable(table_name);
        if (table_opt) {
            return loadTableGeneric(table_name, file_path, table_opt->get().getSchema());
        } else {
            return Result<size_t>::failure(fmt::format("Failed to access existing table: {}", table_name));
        }
    } else {
        try {
            auto schema = TPCHSchemas::createSchemaByName(table_name);
            return loadTableGeneric(table_name, file_path, schema);
        } catch (const std::exception& e) {
            return Result<size_t>::failure(fmt::format("Failed to create schema for {}: {}", table_name, e.what()));
        }
    }
}

Result<bool> TPCHDataLoader::validateLoadedData(double scale_factor) const
{
    auto validation = TPCHValidator::validateTableCounts(catalog_, scale_factor);

    if (!validation.passed) {
        std::string error_msg = "Validation failed:\n";
        for (const auto& error : validation.errors) {
            error_msg += "  - " + error + "\n";
        }
        return Result<bool>::failure(error_msg);
    }

    return Result<bool>::success(true);
}

bool TPCHDataLoader::dataFilesExist(const std::string& data_dir, [[maybe_unused]] double scale_factor)
{
    std::vector<std::string> required_files = { "region.tbl", "nation.tbl",   "customer.tbl", "supplier.tbl",
                                                "part.tbl",   "partsupp.tbl", "orders.tbl",   "lineitem.tbl" };

    for (const auto& file : required_files) {
        std::string full_path = data_dir + "/" + file;
        if (!std::filesystem::exists(full_path)) {
            return false;
        }
    }

    return true;
}

Result<std::vector<Value>> TPCHDataLoader::parseCSVLine(std::string_view line, const Schema& schema)
{
    std::vector<Value> values;
    values.reserve(schema.getColumnCount());

    size_t start = 0;
    size_t column_index = 0;
    size_t num_columns = schema.getColumnCount();

    while (column_index < num_columns) {
        size_t end = line.find('|', start);
        std::string_view field;

        if (end == std::string_view::npos) {
            field = line.substr(start);
        } else {
            field = line.substr(start, end - start);
        }

        // Trim whitespace
        size_t first = field.find_first_not_of(" \t");
        if (first == std::string_view::npos) {
            field = {};
        } else {
            size_t last = field.find_last_not_of(" \t");
            field = field.substr(first, (last - first + 1));
        }

        const auto& column_info = schema.getColumnInfo(column_index);
        auto value_result = parseValue(field, column_info.getType());

        if (!value_result) {
            return Result<std::vector<Value>>::failure(fmt::format("Failed to parse field {} in column {}: {}",
                                                                   std::string(field),
                                                                   column_info.getName(),
                                                                   value_result.error()));
        }

        values.push_back(std::move(value_result.value()));
        column_index++;

        if (end == std::string_view::npos)
            break;
        start = end + 1;
    }

    if (column_index != schema.getColumnCount()) {
        return Result<std::vector<Value>>::failure(
            fmt::format("Expected {} columns, found {}", schema.getColumnCount(), column_index));
    }

    return Result<std::vector<Value>>::success(std::move(values));
}

Result<Value> TPCHDataLoader::parseValue(std::string_view str_value, const DataType& type)
{
    try {
        switch (type.getTypeId()) {
        case DataTypeId::INTEGER: {
            int32_t val;
            auto [ptr, ec] = std::from_chars(str_value.data(), str_value.data() + str_value.size(), val);
            if (ec == std::errc()) {
                return Result<Value>::success(Value::createInteger(val));
            }
            return Result<Value>::failure(fmt::format("Parse error for INTEGER: '{}'", std::string(str_value)));
        }
        case DataTypeId::BIGINT: {
            int64_t val;
            auto [ptr, ec] = std::from_chars(str_value.data(), str_value.data() + str_value.size(), val);
            if (ec == std::errc()) {
                return Result<Value>::success(Value::createBigInt(val));
            }
            return Result<Value>::failure(fmt::format("Parse error for BIGINT: '{}'", std::string(str_value)));
        }
        case DataTypeId::DOUBLE: {
            std::string temp(str_value);
            double double_val = std::stod(temp);
            return Result<Value>::success(Value::createDouble(double_val));
        }
        case DataTypeId::VARCHAR: {
            return Result<Value>::success(Value::createString(std::string(str_value)));
        }
        case DataTypeId::BOOLEAN: {
            bool bool_val = (str_value == "true" || str_value == "1" || str_value == "t");
            return Result<Value>::success(Value::createBoolean(bool_val));
        }
        case DataTypeId::DATE: {
            return Result<Value>::success(Value::createDate(std::string(str_value)));
        }
        default:
            return Result<Value>::failure(fmt::format("Unsupported type: {}", type.toString()));
        }
    } catch (const std::exception& e) {
        return Result<Value>::failure(fmt::format("Parse error for value '{}': {}", std::string(str_value), e.what()));
    }
}

// Specific table loader implementations (simplified for brevity)
Result<size_t> TPCHDataLoader::loadTableGeneric(const std::string& table_name,
                                                const std::string& file_path,
                                                const Schema& schema)
{
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return Result<size_t>::failure(fmt::format("Cannot open file: {}", file_path));
    }

    TableBuilder builder(table_name, schema.clone());

    std::string line;
    size_t rows_loaded = 0;

    while (std::getline(file, line)) {
        if (line.empty())
            continue;

        auto values_result = parseCSVLine(line, schema);
        if (!values_result) {
            return Result<size_t>::failure(
                fmt::format("Failed to parse line {}: {}", rows_loaded + 1, values_result.error()));
        }

        builder.insertRow(std::move(values_result.value()));
        rows_loaded++;
    }

    auto table = std::move(builder).build();
    if (!catalog_.addTable(std::move(table))) {
        return Result<size_t>::failure(fmt::format("Failed to add table data"));
    }

    return Result<size_t>::success(rows_loaded);
}

std::string TPCHDataLoader::getTableFileName(const std::string& table_name) const
{
    return table_name + ".tbl";
}

std::string TPCHDataLoader::formatLoadProgress(const std::string& table_name,
                                               size_t rows_loaded,
                                               std::chrono::duration<double> elapsed) const
{
    double rate = rows_loaded / elapsed.count();
    return fmt::format("{}: {} rows ({:.1f} rows/sec)", table_name, rows_loaded, rate);
}

// TPCHValidator implementation
TPCHValidator::ValidationResult TPCHValidator::validateTableCounts(const Catalog& catalog, double scale_factor)
{
    ValidationResult result;
    ScaleConfig config = ScaleConfigurations::getConfig(scale_factor);

    std::unordered_map<std::string, size_t> expected_counts = {
        { "customer", config.expected_rows_customer }, { "orders", config.expected_rows_orders },
        { "lineitem", config.expected_rows_lineitem }, { "part", config.expected_rows_part },
        { "partsupp", config.expected_rows_partsupp }, { "supplier", config.expected_rows_supplier },
        { "nation", config.expected_rows_nation },     { "region", config.expected_rows_region }
    };

    result.expected_counts = expected_counts;

    for (const auto& [table_name, expected_count] : expected_counts) {
        if (catalog.hasTable(table_name)) {
            auto table_opt = catalog.getTable(table_name);
            if (table_opt) {
                size_t actual_count = (*table_opt).get().getRowCount();
                result.actual_counts[table_name] = actual_count;

                // Allow some tolerance for small scale factors
                double tolerance = scale_factor < 1.0 ? 0.1 : 0.05;
                size_t min_expected = static_cast<size_t>(expected_count * (1.0 - tolerance));
                size_t max_expected = static_cast<size_t>(expected_count * (1.0 + tolerance));

                if (actual_count < min_expected || actual_count > max_expected) {
                    result.passed = false;
                    result.errors.push_back(fmt::format("Table {}: expected ~{} rows, got {} rows",
                                                        table_name,
                                                        expected_count,
                                                        actual_count));
                }
            } else {
                result.passed = false;
                result.errors.push_back(fmt::format("Cannot access table: {}", table_name));
            }
        } else {
            result.passed = false;
            result.errors.push_back(fmt::format("Missing table: {}", table_name));
        }
    }

    return result;
}

TPCHValidator::ValidationResult TPCHValidator::validateTableSchema(const Catalog& catalog,
                                                                   const std::string& table_name)
{
    ValidationResult result;

    if (!catalog.hasTable(table_name)) {
        result.passed = false;
        result.errors.push_back(fmt::format("Table {} does not exist", table_name));
        return result;
    }

    auto table_opt = catalog.getTable(table_name);
    if (!table_opt) {
        result.passed = false;
        result.errors.push_back(fmt::format("Cannot access table: {}", table_name));
        return result;
    }

    const auto& table = (*table_opt).get();
    if (!TPCHSchemas::validateSchema(table_name, table.getSchema())) {
        result.passed = false;
        result.errors.push_back(fmt::format("Schema validation failed for table: {}", table_name));
    }

    return result;
}

TPCHValidator::ValidationResult TPCHValidator::validateForeignKeys(const Catalog& catalog)
{
    ValidationResult result;
    result.passed = true;

    auto checkFK = [&](const std::string& from_table,
                       const std::string& from_col,
                       const std::string& to_table,
                       const std::string& to_col) {
        if (!result.passed)
            return;

        if (!catalog.hasTable(from_table) || !catalog.hasTable(to_table)) {
            return;
        }

        const auto& table_from = catalog.getTable(from_table)->get();
        const auto& table_to = catalog.getTable(to_table)->get();

        size_t col_idx_from = table_from.getColumnIndex(from_col);
        size_t col_idx_to = table_to.getColumnIndex(to_col);

        std::unordered_set<int64_t> keys;
        size_t to_rows = table_to.getRowCount();
        for (size_t i = 0; i < to_rows; ++i) {
            Value val = table_to.getValue(i, col_idx_to);
            if (!val.isNull()) {
                if (val.getTypeId() == DataTypeId::INTEGER) {
                    keys.insert(val.getInteger());
                } else if (val.getTypeId() == DataTypeId::BIGINT) {
                    keys.insert(val.getBigInt());
                }
            }
        }

        size_t from_rows = table_from.getRowCount();
        for (size_t i = 0; i < from_rows; ++i) {
            Value val = table_from.getValue(i, col_idx_from);
            if (!val.isNull()) {
                int64_t key_val = 0;
                if (val.getTypeId() == DataTypeId::INTEGER) {
                    key_val = val.getInteger();
                } else if (val.getTypeId() == DataTypeId::BIGINT) {
                    key_val = val.getBigInt();
                }

                if (keys.find(key_val) == keys.end()) {
                    result.passed = false;
                    result.errors.push_back(fmt::format("FK violation: {}.{} -> {}.{} (key: {})",
                                                        from_table,
                                                        from_col,
                                                        to_table,
                                                        to_col,
                                                        key_val));
                    return;
                }
            }
        }
    };

    checkFK("orders", "o_custkey", "customer", "c_custkey");
    checkFK("lineitem", "l_orderkey", "orders", "o_orderkey");
    checkFK("customer", "c_nationkey", "nation", "n_nationkey");
    checkFK("supplier", "s_nationkey", "nation", "n_nationkey");
    checkFK("nation", "n_regionkey", "region", "r_regionkey");

    return result;
}

} // namespace velodb::benchmark::tpch
