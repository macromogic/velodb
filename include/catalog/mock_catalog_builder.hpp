#pragma once

#include "catalog/catalog.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "data/data_type.hpp"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// Forward declaration for SQL parser
namespace hsql {
struct SQLStatement;
struct SelectStatement;
struct TableRef;
struct Expr;
}

namespace velodb {

class MockCatalogBuilder {
public:
    static std::unique_ptr<Catalog> createSampleCatalog();
    static std::unique_ptr<Catalog> createAdaptiveCatalog();

    static bool ensureTablesForQuery(Catalog& catalog, const std::string& sql);
    static void createDynamicTable(Catalog& catalog,
                                   const std::string& table_name,
                                   const std::vector<std::string>& column_names = {},
                                   const std::map<std::string, DataTypeId>& inferred_types = {});
    static void createGenericTable(Catalog& catalog, const std::string& table_name, size_t column_count = 5);

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
