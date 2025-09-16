#include "tpch_data_loader.hpp"

#include "common/fmt.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

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
        auto table_time = table_end - table_start;

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

    // Create table schema if it doesn't exist
    if (!catalog_.hasTable(table_name)) {
        try {
            Schema schema = TPCHSchemas::createSchemaByName(table_name);
            auto builder = TableBuilder(table_name, std::move(schema));
            if (!catalog_.addTable(std::move(builder).build())) {
                return Result<size_t>::failure(fmt::format("Failed to create table: {}", table_name));
            }
        } catch (const std::exception& e) {
            return Result<size_t>::failure(fmt::format("Failed to create schema for {}: {}", table_name, e.what()));
        }
    }

    // Load data using specific table loader
    if (table_name == "customer")
        return loadCustomerTable(file_path);
    if (table_name == "orders")
        return loadOrdersTable(file_path);
    if (table_name == "lineitem")
        return loadLineitemTable(file_path);
    if (table_name == "part")
        return loadPartTable(file_path);
    if (table_name == "partsupp")
        return loadPartsuppTable(file_path);
    if (table_name == "supplier")
        return loadSupplierTable(file_path);
    if (table_name == "nation")
        return loadNationTable(file_path);
    if (table_name == "region")
        return loadRegionTable(file_path);

    return Result<size_t>::failure(fmt::format("Unknown table name: {}", table_name));
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

Result<void> TPCHDataLoader::generateData(double scale_factor, const std::string& output_dir)
{
    // TODO: Implement TPC-H data generation using dbgen
    // For now, return a helpful error message
    return Result<void>::failure(fmt::format("Data generation not yet implemented. Please provide TPC-H data files "
                                             "for scale factor {} in directory: {}",
                                             scale_factor,
                                             output_dir));
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

Result<std::vector<Value>> TPCHDataLoader::parseCSVLine(const std::string& line, const Schema& schema)
{
    std::vector<Value> values;
    std::stringstream ss(line);
    std::string field;

    size_t column_index = 0;
    while (std::getline(ss, field, '|') && column_index < schema.getColumnCount()) {
        // Trim whitespace
        field.erase(0, field.find_first_not_of(" \t"));
        field.erase(field.find_last_not_of(" \t") + 1);

        const auto& column_info = schema.getColumnInfo(column_index);
        auto value_result = parseValue(field, column_info.getType());

        if (!value_result) {
            return Result<std::vector<Value>>::failure(fmt::format("Failed to parse field {} in column {}: {}",
                                                                   field,
                                                                   column_info.getName(),
                                                                   value_result.error()));
        }

        values.push_back(std::move(value_result.value()));
        column_index++;
    }

    if (column_index != schema.getColumnCount()) {
        return Result<std::vector<Value>>::failure(
            fmt::format("Expected {} columns, found {}", schema.getColumnCount(), column_index));
    }

    return Result<std::vector<Value>>::success(std::move(values));
}

Result<Value> TPCHDataLoader::parseValue(const std::string& str_value, const DataType& type)
{
    try {
        switch (type.getTypeId()) {
        case DataTypeId::INTEGER: {
            int32_t int_val = std::stoi(str_value);
            return Result<Value>::success(Value::createInteger(int_val));
        }
        case DataTypeId::BIGINT: {
            int64_t bigint_val = std::stoll(str_value);
            return Result<Value>::success(Value::createBigInt(bigint_val));
        }
        case DataTypeId::DOUBLE: {
            double double_val = std::stod(str_value);
            return Result<Value>::success(Value::createDouble(double_val));
        }
        case DataTypeId::VARCHAR: {
            return Result<Value>::success(Value::createString(str_value));
        }
        case DataTypeId::BOOLEAN: {
            bool bool_val = (str_value == "true" || str_value == "1" || str_value == "t");
            return Result<Value>::success(Value::createBoolean(bool_val));
        }
        default:
            return Result<Value>::failure(fmt::format("Unsupported type: {}", type.toString()));
        }
    } catch (const std::exception& e) {
        return Result<Value>::failure(fmt::format("Parse error for value '{}': {}", str_value, e.what()));
    }
}

// Specific table loader implementations (simplified for brevity)
Result<size_t> TPCHDataLoader::loadCustomerTable(const std::string& file_path)
{
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return Result<size_t>::failure(fmt::format("Cannot open file: {}", file_path));
    }

    Schema schema = TPCHSchemas::createCustomerSchema();
    TableBuilder builder("customer", schema.clone()); // Clone for builder

    std::string line;
    size_t rows_loaded = 0;

    while (std::getline(file, line)) {
        if (line.empty())
            continue;

        auto values_result = parseCSVLine(line, schema); // Use original schema
        if (!values_result) {
            return Result<size_t>::failure(
                fmt::format("Failed to parse line {}: {}", rows_loaded + 1, values_result.error()));
        }

        builder.insertRow(std::move(values_result.value()));
        rows_loaded++;
    }

    // Replace existing table
    // TODO: Implement table replacement in Catalog
    // For now, this will fail if table already exists with data
    auto table = std::move(builder).build();
    if (!catalog_.addTable(std::move(table))) {
        return Result<size_t>::failure(fmt::format("Failed to add table data"));
    }

    return Result<size_t>::success(rows_loaded);
}

// Similar implementations for other tables (abbreviated for space)
Result<size_t> TPCHDataLoader::loadOrdersTable([[maybe_unused]] const std::string& file_path)
{
    // TODO: Implement similar to loadCustomerTable
    return Result<size_t>::failure("Orders table loading not yet implemented");
}

Result<size_t> TPCHDataLoader::loadLineitemTable([[maybe_unused]] const std::string& file_path)
{
    // TODO: Implement similar to loadCustomerTable
    return Result<size_t>::failure("Lineitem table loading not yet implemented");
}

Result<size_t> TPCHDataLoader::loadPartTable([[maybe_unused]] const std::string& file_path)
{
    // TODO: Implement similar to loadCustomerTable
    return Result<size_t>::failure("Part table loading not yet implemented");
}

Result<size_t> TPCHDataLoader::loadPartsuppTable([[maybe_unused]] const std::string& file_path)
{
    // TODO: Implement similar to loadCustomerTable
    return Result<size_t>::failure("Partsupp table loading not yet implemented");
}

Result<size_t> TPCHDataLoader::loadSupplierTable([[maybe_unused]] const std::string& file_path)
{
    // TODO: Implement similar to loadCustomerTable
    return Result<size_t>::failure("Supplier table loading not yet implemented");
}

Result<size_t> TPCHDataLoader::loadNationTable([[maybe_unused]] const std::string& file_path)
{
    // TODO: Implement similar to loadCustomerTable
    return Result<size_t>::failure("Nation table loading not yet implemented");
}

Result<size_t> TPCHDataLoader::loadRegionTable([[maybe_unused]] const std::string& file_path)
{
    // TODO: Implement similar to loadCustomerTable
    return Result<size_t>::failure("Region table loading not yet implemented");
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

TPCHValidator::ValidationResult TPCHValidator::validateForeignKeys([[maybe_unused]] const Catalog& catalog)
{
    ValidationResult result;

    // TODO: Implement foreign key validation
    // For now, just return success
    result.passed = true;

    return result;
}

} // namespace velodb::benchmark::tpch
