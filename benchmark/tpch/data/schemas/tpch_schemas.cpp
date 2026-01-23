#include "tpch_schemas.hpp"

namespace velodb::benchmark::tpch {

const std::unordered_map<double, ScaleConfig> ScaleConfigurations::scale_configs_ = {
    { 0.01, { 0.01, 150, 1500, 6000, 2000, 8000, 100, 25, 5 } },
    { 0.1, { 0.1, 1500, 15000, 60000, 20000, 80000, 1000, 25, 5 } },
    { 1.0, { 1.0, 150000, 1500000, 6000000, 200000, 800000, 10000, 25, 5 } },
    { 10.0, { 10.0, 1500000, 15000000, 60000000, 2000000, 8000000, 100000, 25, 5 } }
};

ScaleConfig ScaleConfigurations::getConfig(double scale_factor)
{
    auto it = scale_configs_.find(scale_factor);
    if (it != scale_configs_.end()) {
        return it->second;
    }

    // For non-standard scale factors, calculate approximate values
    ScaleConfig config;
    config.scale_factor = scale_factor;
    config.expected_rows_customer = static_cast<size_t>(150000 * scale_factor);
    config.expected_rows_orders = static_cast<size_t>(1500000 * scale_factor);
    config.expected_rows_lineitem = static_cast<size_t>(6000000 * scale_factor);
    config.expected_rows_part = static_cast<size_t>(200000 * scale_factor);
    config.expected_rows_partsupp = static_cast<size_t>(800000 * scale_factor);
    config.expected_rows_supplier = static_cast<size_t>(10000 * scale_factor);
    config.expected_rows_nation = 25;
    config.expected_rows_region = 5;

    return config;
}

std::vector<double> ScaleConfigurations::getSupportedScaleFactors()
{
    return { 0.01, 0.1, 1.0, 10.0 };
}

Schema TPCHSchemas::createCustomerSchema()
{
    Schema schema;

    // c_custkey (identifier)
    schema.addColumnInfo({ "c_custkey", TPCHTypes::createIdentifierType() });

    // c_name (string)
    schema.addColumnInfo({ "c_name", TPCHTypes::createStringType(TPCHTypes::NAME_LENGTH) });

    // c_address (string)
    schema.addColumnInfo({ "c_address", TPCHTypes::createStringType(TPCHTypes::ADDRESS_LENGTH) });

    // c_nationkey (integer)
    schema.addColumnInfo({ "c_nationkey", TPCHTypes::createIntegerType() });

    // c_phone (string)
    schema.addColumnInfo({ "c_phone", TPCHTypes::createStringType(TPCHTypes::PHONE_LENGTH) });

    // c_acctbal (decimal)
    schema.addColumnInfo({ "c_acctbal", TPCHTypes::createDecimalType() });

    // c_mktsegment (string)
    schema.addColumnInfo({ "c_mktsegment", TPCHTypes::createStringType(TPCHTypes::CATEGORY_LENGTH) });

    // c_comment (string)
    schema.addColumnInfo({ "c_comment", TPCHTypes::createStringType(TPCHTypes::COMMENT_LENGTH) });

    return schema;
}

Schema TPCHSchemas::createOrdersSchema()
{
    Schema schema;

    // o_orderkey (identifier)
    schema.addColumnInfo({ "o_orderkey", TPCHTypes::createIdentifierType() });

    // o_custkey (identifier)
    schema.addColumnInfo({ "o_custkey", TPCHTypes::createIdentifierType() });

    // o_orderstatus (string)
    schema.addColumnInfo({ "o_orderstatus", TPCHTypes::createStringType(1) });

    // o_totalprice (decimal)
    schema.addColumnInfo({ "o_totalprice", TPCHTypes::createDecimalType() });

    // o_orderdate (date)
    schema.addColumnInfo({ "o_orderdate", TPCHTypes::createDateType() });

    // o_orderpriority (string)
    schema.addColumnInfo({ "o_orderpriority", TPCHTypes::createStringType(TPCHTypes::PRIORITY_LENGTH) });

    // o_clerk (string)
    schema.addColumnInfo({ "o_clerk", TPCHTypes::createStringType(TPCHTypes::CLERK_LENGTH) });

    // o_shippriority (integer)
    schema.addColumnInfo({ "o_shippriority", TPCHTypes::createIntegerType() });

    // o_comment (string)
    schema.addColumnInfo({ "o_comment", TPCHTypes::createStringType(TPCHTypes::COMMENT_LENGTH) });

    return schema;
}

Schema TPCHSchemas::createLineitemSchema()
{
    Schema schema;

    // l_orderkey (identifier)
    schema.addColumnInfo({ "l_orderkey", TPCHTypes::createIdentifierType() });

    // l_partkey (identifier)
    schema.addColumnInfo({ "l_partkey", TPCHTypes::createIdentifierType() });

    // l_suppkey (identifier)
    schema.addColumnInfo({ "l_suppkey", TPCHTypes::createIdentifierType() });

    // l_linenumber (integer)
    schema.addColumnInfo({ "l_linenumber", TPCHTypes::createIntegerType() });

    // l_quantity (decimal)
    schema.addColumnInfo({ "l_quantity", TPCHTypes::createDecimalType() });

    // l_extendedprice (decimal)
    schema.addColumnInfo({ "l_extendedprice", TPCHTypes::createDecimalType() });

    // l_discount (decimal)
    schema.addColumnInfo({ "l_discount", TPCHTypes::createDecimalType() });

    // l_tax (decimal)
    schema.addColumnInfo({ "l_tax", TPCHTypes::createDecimalType() });

    // l_returnflag (string)
    schema.addColumnInfo({ "l_returnflag", TPCHTypes::createStringType(1) });

    // l_linestatus (string)
    schema.addColumnInfo({ "l_linestatus", TPCHTypes::createStringType(1) });

    // l_shipdate (date)
    schema.addColumnInfo({ "l_shipdate", TPCHTypes::createDateType() });

    // l_commitdate (date)
    schema.addColumnInfo({ "l_commitdate", TPCHTypes::createDateType() });

    // l_receiptdate (date)
    schema.addColumnInfo({ "l_receiptdate", TPCHTypes::createDateType() });

    // l_shipinstruct (string)
    schema.addColumnInfo({ "l_shipinstruct", TPCHTypes::createStringType(TPCHTypes::SHIP_LENGTH) });

    // l_shipmode (string)
    schema.addColumnInfo({ "l_shipmode", TPCHTypes::createStringType(TPCHTypes::MODE_LENGTH) });

    // l_comment (string)
    schema.addColumnInfo({ "l_comment", TPCHTypes::createStringType(TPCHTypes::COMMENT_LENGTH) });

    return schema;
}

Schema TPCHSchemas::createPartSchema()
{
    Schema schema;

    // p_partkey (identifier)
    schema.addColumnInfo({ "p_partkey", TPCHTypes::createIdentifierType() });

    // p_name (string)
    schema.addColumnInfo({ "p_name", TPCHTypes::createStringType(TPCHTypes::NAME_LENGTH) });

    // p_mfgr (string)
    schema.addColumnInfo({ "p_mfgr", TPCHTypes::createStringType(TPCHTypes::NAME_LENGTH) });

    // p_brand (string)
    schema.addColumnInfo({ "p_brand", TPCHTypes::createStringType(TPCHTypes::BRAND_LENGTH) });

    // p_type (string)
    schema.addColumnInfo({ "p_type", TPCHTypes::createStringType(TPCHTypes::TYPE_LENGTH) });

    // p_size (integer)
    schema.addColumnInfo({ "p_size", TPCHTypes::createIntegerType() });

    // p_container (string)
    schema.addColumnInfo({ "p_container", TPCHTypes::createStringType(TPCHTypes::CONTAINER_LENGTH) });

    // p_retailprice (decimal)
    schema.addColumnInfo({ "p_retailprice", TPCHTypes::createDecimalType() });

    // p_comment (string)
    schema.addColumnInfo({ "p_comment", TPCHTypes::createStringType(TPCHTypes::COMMENT_LENGTH) });

    return schema;
}

Schema TPCHSchemas::createPartsuppSchema()
{
    Schema schema;

    // ps_partkey (identifier)
    schema.addColumnInfo({ "ps_partkey", TPCHTypes::createIdentifierType() });

    // ps_suppkey (identifier)
    schema.addColumnInfo({ "ps_suppkey", TPCHTypes::createIdentifierType() });

    // ps_availqty (integer)
    schema.addColumnInfo({ "ps_availqty", TPCHTypes::createIntegerType() });

    // ps_supplycost (decimal)
    schema.addColumnInfo({ "ps_supplycost", TPCHTypes::createDecimalType() });

    // ps_comment (string)
    schema.addColumnInfo({ "ps_comment", TPCHTypes::createStringType(TPCHTypes::COMMENT_LENGTH) });

    return schema;
}

Schema TPCHSchemas::createSupplierSchema()
{
    Schema schema;

    // s_suppkey (identifier)
    schema.addColumnInfo({ "s_suppkey", TPCHTypes::createIdentifierType() });

    // s_name (string)
    schema.addColumnInfo({ "s_name", TPCHTypes::createStringType(TPCHTypes::NAME_LENGTH) });

    // s_address (string)
    schema.addColumnInfo({ "s_address", TPCHTypes::createStringType(TPCHTypes::ADDRESS_LENGTH) });

    // s_nationkey (integer)
    schema.addColumnInfo({ "s_nationkey", TPCHTypes::createIntegerType() });

    // s_phone (string)
    schema.addColumnInfo({ "s_phone", TPCHTypes::createStringType(TPCHTypes::PHONE_LENGTH) });

    // s_acctbal (decimal)
    schema.addColumnInfo({ "s_acctbal", TPCHTypes::createDecimalType() });

    // s_comment (string)
    schema.addColumnInfo({ "s_comment", TPCHTypes::createStringType(TPCHTypes::COMMENT_LENGTH) });

    return schema;
}

Schema TPCHSchemas::createNationSchema()
{
    Schema schema;

    // n_nationkey (integer)
    schema.addColumnInfo({ "n_nationkey", TPCHTypes::createIntegerType() });

    // n_name (string)
    schema.addColumnInfo({ "n_name", TPCHTypes::createStringType(TPCHTypes::NAME_LENGTH) });

    // n_regionkey (integer)
    schema.addColumnInfo({ "n_regionkey", TPCHTypes::createIntegerType() });

    // n_comment (string)
    schema.addColumnInfo({ "n_comment", TPCHTypes::createStringType(TPCHTypes::COMMENT_LENGTH) });

    return schema;
}

Schema TPCHSchemas::createRegionSchema()
{
    Schema schema;

    // r_regionkey (integer)
    schema.addColumnInfo({ "r_regionkey", TPCHTypes::createIntegerType() });

    // r_name (string)
    schema.addColumnInfo({ "r_name", TPCHTypes::createStringType(TPCHTypes::NAME_LENGTH) });

    // r_comment (string)
    schema.addColumnInfo({ "r_comment", TPCHTypes::createStringType(TPCHTypes::COMMENT_LENGTH) });

    return schema;
}

std::vector<std::string> TPCHSchemas::getAllTableNames()
{
    return { "customer", "orders", "lineitem", "part", "partsupp", "supplier", "nation", "region" };
}

Schema TPCHSchemas::createSchemaByName(const std::string& table_name)
{
    if (table_name == "customer")
        return createCustomerSchema();
    if (table_name == "orders")
        return createOrdersSchema();
    if (table_name == "lineitem")
        return createLineitemSchema();
    if (table_name == "part")
        return createPartSchema();
    if (table_name == "partsupp")
        return createPartsuppSchema();
    if (table_name == "supplier")
        return createSupplierSchema();
    if (table_name == "nation")
        return createNationSchema();
    if (table_name == "region")
        return createRegionSchema();

    throw std::invalid_argument("Unknown TPC-H table name: " + table_name);
}

bool TPCHSchemas::validateSchema(const std::string& table_name, const Schema& schema)
{
    try {
        Schema expected = createSchemaByName(table_name);

        // Check column count
        if (schema.getColumnCount() != expected.getColumnCount()) {
            return false;
        }

        // Check each column name and type
        for (size_t i = 0; i < schema.getColumnCount(); ++i) {
            const auto& actual_col = schema.getColumnInfo(i);
            const auto& expected_col = expected.getColumnInfo(i);

            if (actual_col.getName() != expected_col.getName()) {
                return false;
            }

            if (actual_col.getType().getTypeId() != expected_col.getType().getTypeId()) {
                return false;
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

std::unique_ptr<DataType> TPCHTypes::createIdentifierType()
{
    return std::make_unique<BigIntType>();
}

std::unique_ptr<DataType> TPCHTypes::createIntegerType()
{
    return std::make_unique<IntegerType>();
}

std::unique_ptr<DataType> TPCHTypes::createDecimalType()
{
    return std::make_unique<DoubleType>();
}

std::unique_ptr<DataType> TPCHTypes::createStringType(size_t max_len)
{
    return std::make_unique<VarcharType>(max_len);
}

std::unique_ptr<DataType> TPCHTypes::createDateType()
{
    return std::make_unique<DateType>();
}

} // namespace velodb::benchmark::tpch
