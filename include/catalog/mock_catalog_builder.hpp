#pragma once

#include "catalog/catalog.hpp"
#include "catalog/table.hpp"
#include "catalog/schema.hpp"
#include "types/data_type.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>

// Forward declaration for SQL parser
namespace hsql {
struct SQLStatement;
struct SelectStatement;
struct TableRef;
struct Expr;
}

namespace velodb {

/**
 * @brief Utility class for creating mock catalog data for testing and visualization
 */
class MockCatalogBuilder {
public:
    /**
     * @brief Create a mock catalog with sample tables
     * @return Unique pointer to populated catalog
     */
    static std::unique_ptr<Catalog> createSampleCatalog();

    /**
     * @brief Create a dynamic catalog that can handle arbitrary queries
     * @return Unique pointer to adaptive catalog
     */
    static std::unique_ptr<Catalog> createAdaptiveCatalog();

    /**
     * @brief Analyze SQL query and create missing tables dynamically
     * @param catalog Catalog to modify
     * @param sql SQL query string
     * @return Success status
     */
    static bool ensureTablesForQuery(Catalog& catalog, const std::string& sql);

    /**
     * @brief Create table schema dynamically based on query analysis
     * @param catalog Catalog to add table to
     * @param table_name Name of the table to create
     * @param column_names Optional list of column names mentioned in query
     * @param inferred_types Optional type hints from query context
     */
    static void createDynamicTable(Catalog& catalog, 
                                  const std::string& table_name,
                                  const std::vector<std::string>& column_names = {},
                                  const std::map<std::string, DataTypeId>& inferred_types = {});

    /**
     * @brief Create a table with generic schema for testing
     * @param catalog Catalog to add table to
     * @param table_name Name of the table
     * @param column_count Number of columns (default: 5)
     */
    static void createGenericTable(Catalog& catalog, 
                                  const std::string& table_name,
                                  size_t column_count = 5);

    // Original predefined table creation methods
    static void createUsersTable(Catalog& catalog);
    static void createOrdersTable(Catalog& catalog);
    static void createProductsTable(Catalog& catalog);
    static void createComplexTable(Catalog& catalog);
    static void populateSampleData(Catalog& catalog);

private:
    // Helper methods for dynamic table creation
    static std::set<std::string> extractTableNames(const std::string& sql);
    static void extractTableNamesFromTableRef(const hsql::TableRef* table_ref, std::set<std::string>& table_names);
    static std::vector<std::string> extractColumnNames(const hsql::SelectStatement* select_stmt);
    static std::map<std::string, DataTypeId> inferColumnTypes(const hsql::SelectStatement* select_stmt);
    static DataTypeId inferTypeFromExpression(const hsql::Expr* expr);
    static DataTypeId inferTypeFromLiteral(const hsql::Expr* literal);
    static void createTableFromCommonSchema(Catalog& catalog, const std::string& table_name);
    
    // Helper methods for schema creation
    static std::unique_ptr<Schema> createUsersSchema();
    static std::unique_ptr<Schema> createOrdersSchema();
    static std::unique_ptr<Schema> createProductsSchema();
    static std::unique_ptr<Schema> createComplexSchema();
    static std::unique_ptr<Schema> createGenericSchema(const std::string& table_name, size_t column_count);
    static std::unique_ptr<Schema> createDynamicSchema(const std::string& table_name,
                                                      const std::vector<std::string>& column_names,
                                                      const std::map<std::string, DataTypeId>& inferred_types);
    
    // Default column types for common column names
    static std::map<std::string, DataTypeId> getDefaultColumnTypes();
    
    // Common table schemas that can be automatically created
    static std::map<std::string, std::vector<std::pair<std::string, DataTypeId>>> getCommonTableSchemas();
};

} // namespace velodb
