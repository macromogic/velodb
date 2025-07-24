#include "velodb.hpp"
#include <sstream>
#include <stdexcept>

namespace velodb {

// TODO: Implement full Database class functionality

// Database implementation
Database::Database()
{
    catalog_ = std::make_unique<Catalog>();
    execution_engine_ = std::make_unique<ExecutionEngine>(*catalog_);
}

bool Database::initialize()
{
    // TODO: Implement database initialization
    initialized_ = true;
    return true;
}

void Database::shutdown()
{
    // TODO: Implement database shutdown
    initialized_ = false;
}

bool Database::createTable(const std::string& table_name, std::unique_ptr<Schema> schema)
{
    if (!initialized_)
        return false;
    return catalog_->createTable(table_name, std::move(schema));
}

bool Database::dropTable(const std::string& table_name)
{
    if (!initialized_)
        return false;
    return catalog_->dropTable(table_name);
}

bool Database::hasTable(const std::string& table_name) const
{
    if (!initialized_)
        return false;
    return catalog_->hasTable(table_name);
}

std::optional<std::reference_wrapper<Table>> Database::getTable(const std::string& table_name) const
{
    if (!initialized_)
        return std::nullopt;
    return catalog_->getTable(table_name);
}

bool Database::insertTuple(const std::string& table_name, const Tuple& tuple)
{
    if (!initialized_)
        return false;
    auto table = catalog_->getTable(table_name);
    if (!table)
        return false;
    
    // Convert tuple to values vector
    std::vector<Value> values;
    values.reserve(tuple.getColumnCount());
    for (size_t i = 0; i < tuple.getColumnCount(); ++i) {
        values.push_back(tuple.getValue(i));
    }
    table->get().insertRow(values);
    return true;
}

bool Database::insertTuple(const std::string& table_name, Tuple&& tuple)
{
    if (!initialized_)
        return false;
    auto table = catalog_->getTable(table_name);
    if (!table)
        return false;
    
    // Convert tuple to values vector
    std::vector<Value> values;
    values.reserve(tuple.getColumnCount());
    for (size_t i = 0; i < tuple.getColumnCount(); ++i) {
        values.push_back(std::move(const_cast<Tuple&>(tuple).getValue(i)));
    }
    table->get().insertRow(std::move(values));
    return true;
}

Result<View> Database::executeQuery(const std::string& sql)
{
    if (!initialized_) {
        throw std::runtime_error("Database not initialized");
    }
    return execution_engine_->executeQuery(sql);
}

size_t Database::getTableCount() const
{
    if (!initialized_)
        return 0;
    return catalog_->getTableNames().size();
}

std::vector<std::string> Database::getTableNames() const
{
    if (!initialized_)
        return {};
    return catalog_->getTableNames();
}

std::string Database::getDatabaseInfo() const
{
    if (!initialized_)
        return "Database not initialized";

    std::stringstream ss;
    ss << "VeloDB Database Information:\n";
    ss << "  Tables: " << getTableCount() << "\n";
    ss << "\nCatalog Details:\n";
    ss << catalog_->toString();

    return ss.str();
}

namespace util {

std::unique_ptr<DataType> createIntegerType()
{
    return std::make_unique<IntegerType>();
}

std::unique_ptr<DataType> createBigIntType()
{
    return std::make_unique<BigIntType>();
}

std::unique_ptr<DataType> createDoubleType()
{
    return std::make_unique<DoubleType>();
}

std::unique_ptr<DataType> createBooleanType()
{
    return std::make_unique<BooleanType>();
}

std::unique_ptr<DataType> createVarcharType(size_t max_length)
{
    return std::make_unique<VarcharType>(max_length);
}

Value createIntegerValue(int32_t value)
{
    return Value::createInteger(value);
}

Value createBigIntValue(int64_t value)
{
    return Value::createBigInt(value);
}

Value createDoubleValue(double value)
{
    return Value::createDouble(value);
}

Value createBooleanValue(bool value)
{
    return Value::createBoolean(value);
}

Value createStringValue(const std::string& value)
{
    return Value::createString(value);
}

Value createNullValue(DataTypeId type_id)
{
    return Value::createNull(type_id);
}

std::unique_ptr<Schema> createSchema(std::vector<ColumnInfo> columns)
{
    return std::make_unique<Schema>(std::move(columns));
}


std::unique_ptr<Database> createSampleDatabase()
{
    // TODO: Create a more comprehensive sample database
    auto db = std::make_unique<Database>();

    // Create a sample table
    std::vector<ColumnInfo> columns;
    columns.emplace_back("id", createIntegerType(), false);
    columns.emplace_back("name", createVarcharType(100), true);
    columns.emplace_back("age", createIntegerType(), true);
    columns.emplace_back("salary", createDoubleType(), true);

    auto schema = createSchema(std::move(columns));
    db->createTable("employees", std::move(schema));

    return db;
}

void populateSampleData(Database* db)
{
    // TODO: Add more comprehensive sample data
    if ((db == nullptr) || !db->hasTable("employees")) {
        return;
    }

    auto table = db->getTable("employees");
    if (!table)
        return;

    const Schema& schema = table->get().getSchema();

    // Add sample employees
    std::vector<Value> values1 = {
        createIntegerValue(1),
        createStringValue("Alice Johnson"),
        createIntegerValue(30),
        createDoubleValue(75000.0)
    };
    Tuple tuple1(schema, std::move(values1));
    db->insertTuple("employees", std::move(tuple1));

    std::vector<Value> values2 = {
        createIntegerValue(2),
        createStringValue("Bob Smith"),
        createIntegerValue(25),
        createDoubleValue(65000.0)
    };
    Tuple tuple2(schema, std::move(values2));
    db->insertTuple("employees", std::move(tuple2));

    std::vector<Value> values3 = {
        createIntegerValue(3),
        createStringValue("Carol Davis"),
        createIntegerValue(35),
        createDoubleValue(85000.0)
    };
    Tuple tuple3(schema, std::move(values3));
    db->insertTuple("employees", std::move(tuple3));
}

} // namespace util

} // namespace velodb
