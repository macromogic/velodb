#pragma once

#include "velodb.hpp"

#include "../schemas/tpch_schemas.hpp"
#include "catalog/catalog.hpp"
#include "catalog/table_builder.hpp"
#include "common/copy_traits.hpp"
#include "common/result.hpp"
#include "data/data_location.hpp"
#include "data/value.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace velodb::benchmark::tpch {

class TPCHDataLoader : public NonCopyable {
public:
    struct LoadConfig {
        std::string data_directory;
        double scale_factor;
        bool validate_data = true;
        size_t batch_size = 10000;
        bool verbose = false;
        // Use pageable memory for large tables to reduce pinned memory pressure
        DataLocation data_location = DataLocation::HOST_PAGEABLE;
    };

    struct LoadStatistics {
        size_t total_rows_loaded = 0;
        std::chrono::duration<double> total_load_time { 0 };
        std::unordered_map<std::string, size_t> table_row_counts;
        std::unordered_map<std::string, std::chrono::duration<double>> table_load_times;
        bool validation_passed = true;
        std::string error_message;
    };

    explicit TPCHDataLoader(Catalog& catalog);

    // Load all TPC-H tables
    Result<LoadStatistics> loadAllTables(const LoadConfig& config);

    // Load a specific table
    Result<size_t> loadTable(const std::string& table_name, const LoadConfig& config);

    // Validate loaded data against expected counts
    Result<bool> validateLoadedData(double scale_factor) const;

    // Check if data files exist for given scale factor
    static bool dataFilesExist(const std::string& data_dir, double scale_factor);

private:
    Catalog& catalog_;
    DataLocation current_location_ = DataLocation::HOST_PAGEABLE;

    // File parsing methods
    Result<std::vector<Value>> parseCSVLine(std::string_view line, const Schema& schema);
    Result<Value> parseValue(std::string_view str_value, const DataType& type);

    // Generic table loader
    Result<size_t> loadTableGeneric(const std::string& table_name, const std::string& file_path, const Schema& schema);

    // Helper methods
    std::string getTableFileName(const std::string& table_name) const;
    std::string formatLoadProgress(const std::string& table_name,
                                   size_t rows_loaded,
                                   std::chrono::duration<double> elapsed) const;
};

// Utility class for TPC-H data validation
class TPCHValidator {
public:
    struct ValidationResult {
        bool passed = true;
        std::vector<std::string> errors;
        std::unordered_map<std::string, size_t> actual_counts;
        std::unordered_map<std::string, size_t> expected_counts;
    };

    static ValidationResult validateTableCounts(const Catalog& catalog, double scale_factor);
    static ValidationResult validateTableSchema(const Catalog& catalog, const std::string& table_name);
    static ValidationResult validateForeignKeys(const Catalog& catalog);

private:
    static bool validateCustomerData(const Table& table);
    static bool validateOrdersData(const Table& table);
    static bool validateLineitemData(const Table& table);
    static bool validatePartData(const Table& table);
    static bool validatePartsuppData(const Table& table);
    static bool validateSupplierData(const Table& table);
    static bool validateNationData(const Table& table);
    static bool validateRegionData(const Table& table);
};

} // namespace velodb::benchmark::tpch
