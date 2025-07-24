#include "catalog/mock_catalog_builder.hpp"
#include "SQLParser.h"
#include "catalog/column.hpp"
#include "types/data_type.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>

namespace velodb {

std::unique_ptr<Catalog> MockCatalogBuilder::createSampleCatalog()
{
    auto catalog = std::make_unique<Catalog>();

    // Add various sample tables
    createUsersTable(*catalog);
    createOrdersTable(*catalog);
    createProductsTable(*catalog);
    createComplexTable(*catalog);

    // Populate with sample data (when implemented)
    // populateSampleData(*catalog);

    return catalog;
}

void MockCatalogBuilder::createUsersTable(Catalog& catalog)
{
    auto schema = createUsersSchema();
    catalog.createTable("users", std::move(schema));
}

void MockCatalogBuilder::createOrdersTable(Catalog& catalog)
{
    auto schema = createOrdersSchema();
    catalog.createTable("orders", std::move(schema));
}

void MockCatalogBuilder::createProductsTable(Catalog& catalog)
{
    auto schema = createProductsSchema();
    catalog.createTable("products", std::move(schema));
}

void MockCatalogBuilder::createComplexTable(Catalog& catalog)
{
    auto schema = createComplexSchema();
    catalog.createTable("complex_table", std::move(schema));
}

void MockCatalogBuilder::populateSampleData(Catalog& catalog)
{
    // TODO: Implement sample data population
    // This would add actual rows to the tables for testing
    // For now, we just have schema definitions
    (void)catalog; // Suppress unused parameter warning
}

// Private helper methods for schema creation
std::unique_ptr<Schema> MockCatalogBuilder::createUsersSchema()
{
    auto schema = std::make_unique<Schema>();

    // Add columns for users table
    schema->addColumnInfo({ "id", std::make_unique<IntegerType>(), false} ); // NOT NULL
    schema->addColumnInfo({ "name", std::make_unique<VarcharType>(100), false} );
    schema->addColumnInfo({ "email", std::make_unique<VarcharType>(255), false} );
    schema->addColumnInfo({ "age", std::make_unique<IntegerType>(), true} ); // Nullable
    schema->addColumnInfo({ "salary", std::make_unique<DoubleType>(), true} );

    return schema;
}

std::unique_ptr<Schema> MockCatalogBuilder::createOrdersSchema()
{
    auto schema = std::make_unique<Schema>();

    // Add columns for orders table
    schema->addColumnInfo({ "order_id", std::make_unique<IntegerType>(), false });
    schema->addColumnInfo({ "user_id", std::make_unique<IntegerType>(), false });
    schema->addColumnInfo({ "product_id", std::make_unique<IntegerType>(), false });
    schema->addColumnInfo({ "quantity", std::make_unique<IntegerType>(), false });
    schema->addColumnInfo({ "price", std::make_unique<DoubleType>(), false });
    schema->addColumnInfo({ "order_date", std::make_unique<VarcharType>(20), false }); // Simplified date as string

    return schema;
}

std::unique_ptr<Schema> MockCatalogBuilder::createProductsSchema()
{
    auto schema = std::make_unique<Schema>();

    // Add columns for products table
    schema->addColumnInfo({ "product_id", std::make_unique<IntegerType>(), false });
    schema->addColumnInfo({ "name", std::make_unique<VarcharType>(200), false });
    schema->addColumnInfo({ "description", std::make_unique<VarcharType>(1000), true });
    schema->addColumnInfo({ "price", std::make_unique<DoubleType>(), false });
    schema->addColumnInfo({ "stock_quantity", std::make_unique<IntegerType>(), false });
    schema->addColumnInfo({ "category", std::make_unique<VarcharType>(50), true });

    return schema;
}

std::unique_ptr<Schema> MockCatalogBuilder::createComplexSchema()
{
    auto schema = std::make_unique<Schema>();

    // Add columns with various data types for comprehensive testing
    schema->addColumnInfo({ "id", std::make_unique<IntegerType>(), false });
    schema->addColumnInfo({ "int_col", std::make_unique<IntegerType>(), true });
    schema->addColumnInfo({ "double_col", std::make_unique<DoubleType>(), true });
    schema->addColumnInfo({ "varchar_small", std::make_unique<VarcharType>(50), true });
    schema->addColumnInfo({ "varchar_large", std::make_unique<VarcharType>(1000), true });
    schema->addColumnInfo({ "bool_col", std::make_unique<BooleanType>(), true });
    schema->addColumnInfo({ "timestamp_col", std::make_unique<VarcharType>(30), true }); // Simplified timestamp

    return schema;
}

std::unique_ptr<Catalog> MockCatalogBuilder::createAdaptiveCatalog()
{
    auto catalog = std::make_unique<Catalog>();
    // Start with empty catalog - tables will be created on demand
    return catalog;
}

bool MockCatalogBuilder::ensureTablesForQuery(Catalog& catalog, const std::string& sql)
{
    try {
        // Parse the SQL to extract table names
        hsql::SQLParserResult result;
        hsql::SQLParser::parse(sql, &result);

        if (!result.isValid() || result.size() == 0) {
            std::cerr << "Invalid SQL query: " << sql << std::endl;
            return false;
        }

        const hsql::SQLStatement* stmt = result.getStatement(0);
        if (stmt->type() != hsql::kStmtSelect) {
            // For non-SELECT statements, just extract table names using regex
            auto table_names = extractTableNames(sql);
            for (const auto& table_name : table_names) {
                if (!catalog.hasTable(table_name)) {
                    createGenericTable(catalog, table_name);
                }
            }
            return true;
        }

        const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

        // Extract table names from FROM clause
        std::set<std::string> table_names;
        if (select_stmt->fromTable) {
            extractTableNamesFromTableRef(select_stmt->fromTable, table_names);
        }

        // Create missing tables
        for (const auto& table_name : table_names) {
            if (!catalog.hasTable(table_name)) {
                // Try to create a common table schema first
                auto common_schemas = getCommonTableSchemas();
                if (common_schemas.find(table_name) != common_schemas.end()) {
                    createTableFromCommonSchema(catalog, table_name);
                } else {
                    // Extract column information from SELECT clause
                    auto column_names = extractColumnNames(select_stmt);
                    auto inferred_types = inferColumnTypes(select_stmt);
                    createDynamicTable(catalog, table_name, column_names, inferred_types);
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error analyzing query: " << e.what() << std::endl;
        return false;
    }
}

void MockCatalogBuilder::createDynamicTable(Catalog& catalog,
    const std::string& table_name,
    const std::vector<std::string>& column_names,
    const std::map<std::string, DataTypeId>& inferred_types)
{
    auto schema = createDynamicSchema(table_name, column_names, inferred_types);
    catalog.createTable(table_name, std::move(schema));
}

void MockCatalogBuilder::createGenericTable(Catalog& catalog,
    const std::string& table_name,
    size_t column_count)
{
    auto schema = createGenericSchema(table_name, column_count);
    catalog.createTable(table_name, std::move(schema));
}

// Private helper methods for dynamic table creation
std::set<std::string> MockCatalogBuilder::extractTableNames(const std::string& sql)
{
    std::set<std::string> table_names;

    // Simple regex-based table name extraction (fallback method)
    std::regex from_regex(R"(\bFROM\s+(\w+))", std::regex_constants::icase);
    std::regex join_regex(R"(\bJOIN\s+(\w+))", std::regex_constants::icase);

    std::smatch match;
    std::string::const_iterator search_start(sql.cbegin());

    // Extract FROM tables
    while (std::regex_search(search_start, sql.cend(), match, from_regex)) {
        table_names.insert(match[1].str());
        search_start = match.suffix().first;
    }

    // Extract JOIN tables
    search_start = sql.cbegin();
    while (std::regex_search(search_start, sql.cend(), match, join_regex)) {
        table_names.insert(match[1].str());
        search_start = match.suffix().first;
    }

    return table_names;
}

void MockCatalogBuilder::extractTableNamesFromTableRef(const hsql::TableRef* table_ref, std::set<std::string>& table_names)
{
    if (!table_ref)
        return;

    switch (table_ref->type) {
    case hsql::kTableName:
        if (table_ref->name) {
            table_names.insert(table_ref->name);
        }
        break;
    case hsql::kTableSelect:
        // Handle subqueries - for now, just ignore
        break;
    case hsql::kTableJoin:
        extractTableNamesFromTableRef(table_ref->join->left, table_names);
        extractTableNamesFromTableRef(table_ref->join->right, table_names);
        break;
    case hsql::kTableCrossProduct:
        for (const auto& table : *table_ref->list) {
            extractTableNamesFromTableRef(table, table_names);
        }
        break;
    }
}

std::vector<std::string> MockCatalogBuilder::extractColumnNames(const hsql::SelectStatement* select_stmt)
{
    std::vector<std::string> column_names;

    if (!select_stmt->selectList) {
        return column_names;
    }

    for (const auto* expr : *select_stmt->selectList) {
        if (expr->type == hsql::kExprStar) {
            // SELECT * - we'll create generic columns
            continue;
        } else if (expr->type == hsql::kExprColumnRef && expr->name) {
            column_names.push_back(expr->name);
        }
    }

    return column_names;
}

std::map<std::string, DataTypeId> MockCatalogBuilder::inferColumnTypes(const hsql::SelectStatement* select_stmt)
{
    std::map<std::string, DataTypeId> inferred_types;

    if (!select_stmt->selectList) {
        return inferred_types;
    }

    for (const auto* expr : *select_stmt->selectList) {
        if (expr->type == hsql::kExprColumnRef && expr->name) {
            std::string column_name = expr->name;

            // Try to infer type from column name patterns
            auto default_types = getDefaultColumnTypes();
            for (const auto& [pattern, type] : default_types) {
                if (column_name.find(pattern) != std::string::npos) {
                    inferred_types[column_name] = type;
                    break;
                }
            }

            // If no pattern matched, use default
            if (inferred_types.find(column_name) == inferred_types.end()) {
                inferred_types[column_name] = DataTypeId::VARCHAR;
            }
        }
    }

    return inferred_types;
}

DataTypeId MockCatalogBuilder::inferTypeFromExpression(const hsql::Expr* expr)
{
    if (!expr)
        return DataTypeId::VARCHAR;

    switch (expr->type) {
    case hsql::kExprLiteralInt:
        return DataTypeId::INTEGER;
    case hsql::kExprLiteralFloat:
        return DataTypeId::DOUBLE;
    case hsql::kExprLiteralString:
        return DataTypeId::VARCHAR;
    case hsql::kExprColumnRef:
        // Try to infer from column name
        if (expr->name) {
            auto default_types = getDefaultColumnTypes();
            for (const auto& [pattern, type] : default_types) {
                if (std::string(expr->name).find(pattern) != std::string::npos) {
                    return type;
                }
            }
        }
        return DataTypeId::VARCHAR;
    default:
        return DataTypeId::VARCHAR;
    }
}

DataTypeId MockCatalogBuilder::inferTypeFromLiteral(const hsql::Expr* literal)
{
    if (!literal)
        return DataTypeId::VARCHAR;

    switch (literal->type) {
    case hsql::kExprLiteralInt:
        return DataTypeId::INTEGER;
    case hsql::kExprLiteralFloat:
        return DataTypeId::DOUBLE;
    case hsql::kExprLiteralString:
        return DataTypeId::VARCHAR;
    case hsql::kExprLiteralNull:
        return DataTypeId::VARCHAR; // Default for NULL
    default:
        return DataTypeId::VARCHAR;
    }
}

// Helper methods for schema creation
std::unique_ptr<Schema> MockCatalogBuilder::createGenericSchema(const std::string& table_name, size_t column_count)
{
    auto schema = std::make_unique<Schema>();

    // Create generic columns with predictable names
    for (size_t i = 0; i < column_count; ++i) {
        std::string column_name = "col_" + std::to_string(i);

        // Vary the types to make it more realistic
        DataTypeId type;
        switch (i % 4) {
        case 0:
            type = DataTypeId::INTEGER;
            break;
        case 1:
            type = DataTypeId::VARCHAR;
            break;
        case 2:
            type = DataTypeId::DOUBLE;
            break;
        case 3:
            type = DataTypeId::BOOLEAN;
            break;
        default:
            type = DataTypeId::VARCHAR;
            break;
        }

        auto data_type = DataType::createType(type);
        schema->addColumnInfo({ column_name, std::move(data_type), true });
    }

    // Add some common columns based on table name
    if (table_name.find("user") != std::string::npos) {
        schema->addColumnInfo({ "id", DataType::createType(DataTypeId::INTEGER), false });
        schema->addColumnInfo({ "name", DataType::createType(DataTypeId::VARCHAR), false });
        schema->addColumnInfo({ "email", DataType::createType(DataTypeId::VARCHAR), true });
    } else if (table_name.find("order") != std::string::npos) {
        schema->addColumnInfo({ "order_id", DataType::createType(DataTypeId::INTEGER), false });
        schema->addColumnInfo({ "user_id", DataType::createType(DataTypeId::INTEGER), false });
        schema->addColumnInfo({ "total", DataType::createType(DataTypeId::DOUBLE), false });
    } else if (table_name.find("product") != std::string::npos) {
        schema->addColumnInfo({ "product_id", DataType::createType(DataTypeId::INTEGER), false });
        schema->addColumnInfo({ "name", DataType::createType(DataTypeId::VARCHAR), false });
        schema->addColumnInfo({ "price", DataType::createType(DataTypeId::DOUBLE), false });
    }

    return schema;
}

std::unique_ptr<Schema> MockCatalogBuilder::createDynamicSchema(const std::string& table_name,
    const std::vector<std::string>& column_names,
    const std::map<std::string, DataTypeId>& inferred_types)
{
    auto schema = std::make_unique<Schema>();

    // Always add an ID column
    schema->addColumnInfo({ "id", DataType::createType(DataTypeId::INTEGER), false });

    // Add columns based on the query
    for (const auto& column_name : column_names) {
        if (column_name == "id")
            continue; // Skip duplicate ID

        DataTypeId type = DataTypeId::VARCHAR;
        if (inferred_types.find(column_name) != inferred_types.end()) {
            type = inferred_types.at(column_name);
        }

        auto data_type = DataType::createType(type);
        schema->addColumnInfo({ column_name, std::move(data_type), true });
    }

    // If no columns were specified, create a generic schema
    if (column_names.empty()) {
        return createGenericSchema(table_name, 5);
    }

    return schema;
}

std::map<std::string, DataTypeId> MockCatalogBuilder::getDefaultColumnTypes()
{
    return {
        { "id", DataTypeId::INTEGER },
        { "_id", DataTypeId::INTEGER },
        { "count", DataTypeId::INTEGER },
        { "num", DataTypeId::INTEGER },
        { "quantity", DataTypeId::INTEGER },
        { "age", DataTypeId::INTEGER },
        { "year", DataTypeId::INTEGER },
        { "month", DataTypeId::INTEGER },
        { "day", DataTypeId::INTEGER },

        { "price", DataTypeId::DOUBLE },
        { "cost", DataTypeId::DOUBLE },
        { "total", DataTypeId::DOUBLE },
        { "amount", DataTypeId::DOUBLE },
        { "salary", DataTypeId::DOUBLE },
        { "weight", DataTypeId::DOUBLE },
        { "height", DataTypeId::DOUBLE },
        { "rate", DataTypeId::DOUBLE },
        { "percentage", DataTypeId::DOUBLE },

        { "active", DataTypeId::BOOLEAN },
        { "enabled", DataTypeId::BOOLEAN },
        { "deleted", DataTypeId::BOOLEAN },
        { "visible", DataTypeId::BOOLEAN },
        { "public", DataTypeId::BOOLEAN },

        { "name", DataTypeId::VARCHAR },
        { "email", DataTypeId::VARCHAR },
        { "address", DataTypeId::VARCHAR },
        { "phone", DataTypeId::VARCHAR },
        { "description", DataTypeId::VARCHAR },
        { "title", DataTypeId::VARCHAR },
        { "category", DataTypeId::VARCHAR },
        { "status", DataTypeId::VARCHAR },
        { "type", DataTypeId::VARCHAR },
        { "code", DataTypeId::VARCHAR },
        { "date", DataTypeId::VARCHAR }, // Simplified as string
        { "time", DataTypeId::VARCHAR }, // Simplified as string
        { "timestamp", DataTypeId::VARCHAR } // Simplified as string
    };
}

std::map<std::string, std::vector<std::pair<std::string, DataTypeId>>> MockCatalogBuilder::getCommonTableSchemas()
{
    return {
        { "users", { { "id", DataTypeId::INTEGER }, { "name", DataTypeId::VARCHAR }, { "email", DataTypeId::VARCHAR }, { "age", DataTypeId::INTEGER }, { "created_at", DataTypeId::VARCHAR } } },
        { "orders", { { "order_id", DataTypeId::INTEGER }, { "user_id", DataTypeId::INTEGER }, { "product_id", DataTypeId::INTEGER }, { "quantity", DataTypeId::INTEGER }, { "price", DataTypeId::DOUBLE }, { "order_date", DataTypeId::VARCHAR } } },
        { "products", { { "product_id", DataTypeId::INTEGER }, { "name", DataTypeId::VARCHAR }, { "description", DataTypeId::VARCHAR }, { "price", DataTypeId::DOUBLE }, { "stock_quantity", DataTypeId::INTEGER }, { "category", DataTypeId::VARCHAR } } },
        { "customers", { { "customer_id", DataTypeId::INTEGER }, { "name", DataTypeId::VARCHAR }, { "email", DataTypeId::VARCHAR }, { "phone", DataTypeId::VARCHAR }, { "address", DataTypeId::VARCHAR } } },
        { "employees", { { "employee_id", DataTypeId::INTEGER }, { "name", DataTypeId::VARCHAR }, { "department", DataTypeId::VARCHAR }, { "salary", DataTypeId::DOUBLE }, { "hire_date", DataTypeId::VARCHAR } } }
    };
}

void MockCatalogBuilder::createTableFromCommonSchema(Catalog& catalog, const std::string& table_name)
{
    auto common_schemas = getCommonTableSchemas();
    if (common_schemas.find(table_name) == common_schemas.end()) {
        createGenericTable(catalog, table_name);
        return;
    }

    auto schema = std::make_unique<Schema>();
    for (const auto& [column_name, type_id] : common_schemas[table_name]) {
        auto data_type = DataType::createType(type_id);
        bool nullable = (column_name.find("id") == std::string::npos); // IDs are typically not nullable
        schema->addColumnInfo({ column_name, std::move(data_type), nullable });
    }

    catalog.createTable(table_name, std::move(schema));
}

} // namespace velodb
