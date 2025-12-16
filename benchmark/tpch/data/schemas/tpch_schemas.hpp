#pragma once

#include "catalog/schema.hpp"
#include "data/data_type.hpp"

#include <memory>
#include <unordered_map>

namespace velodb::benchmark::tpch {

// Scale factor configurations for TPC-H
struct ScaleConfig {
    double scale_factor;
    size_t expected_rows_customer;
    size_t expected_rows_orders;
    size_t expected_rows_lineitem;
    size_t expected_rows_part;
    size_t expected_rows_partsupp;
    size_t expected_rows_supplier;
    size_t expected_rows_nation;
    size_t expected_rows_region;
};

// Standard TPC-H scale configurations
class ScaleConfigurations {
public:
    static ScaleConfig getConfig(double scale_factor);
    static std::vector<double> getSupportedScaleFactors();

private:
    static const std::unordered_map<double, ScaleConfig> scale_configs_;
};

// TPC-H table schema creators
class TPCHSchemas {
public:
    // Customer table (8 columns)
    static Schema createCustomerSchema();

    // Orders table (9 columns)
    static Schema createOrdersSchema();

    // Lineitem table (16 columns) - largest table
    static Schema createLineitemSchema();

    // Part table (9 columns)
    static Schema createPartSchema();

    // Partsupp table (5 columns)
    static Schema createPartsuppSchema();

    // Supplier table (7 columns)
    static Schema createSupplierSchema();

    // Nation table (4 columns)
    static Schema createNationSchema();

    // Region table (3 columns)
    static Schema createRegionSchema();

    // Get all table names
    static std::vector<std::string> getAllTableNames();

    // Create schema by table name
    static Schema createSchemaByName(const std::string& table_name);

    // Validate schema compatibility
    static bool validateSchema(const std::string& table_name, const Schema& schema);
};

// TPC-H data type mappings
class TPCHTypes {
public:
    // Standard TPC-H types mapped to VelODB types
    static std::unique_ptr<DataType> createIdentifierType(); // For keys: BIGINT
    static std::unique_ptr<DataType> createIntegerType(); // For integers: INTEGER
    static std::unique_ptr<DataType> createDecimalType(); // For prices/amounts: DOUBLE
    static std::unique_ptr<DataType> createStringType(size_t max_len); // For strings: VARCHAR
    static std::unique_ptr<DataType> createDateType(); // For dates: VARCHAR (YYYY-MM-DD)

    // Common string lengths in TPC-H
    static constexpr size_t NAME_LENGTH = 25;
    static constexpr size_t ADDRESS_LENGTH = 40;
    static constexpr size_t COMMENT_LENGTH = 152;
    static constexpr size_t PHONE_LENGTH = 15;
    static constexpr size_t DATE_LENGTH = 10;
    static constexpr size_t MODE_LENGTH = 10;
    static constexpr size_t PRIORITY_LENGTH = 15;
    static constexpr size_t CLERK_LENGTH = 15;
    static constexpr size_t SHIP_LENGTH = 25;
    static constexpr size_t CATEGORY_LENGTH = 25;
    static constexpr size_t BRAND_LENGTH = 10;
    static constexpr size_t TYPE_LENGTH = 25;
    static constexpr size_t CONTAINER_LENGTH = 10;
};

} // namespace velodb::benchmark::tpch
