#include "../common/test_warmup_utility.hpp"
#include "catalog/catalog.hpp"
#include "catalog/schema.hpp"
#include "catalog/table.hpp"
#include "catalog/table_builder.hpp"
#include "common/exception.hpp"
#include "data/data_type.hpp"
#include "planner/query_planner.hpp"

#include <SQLParser.h>

#include <gtest/gtest.h>

using namespace velodb;

class PlannerTest : public test::VeloDBTest {
public:
    PlannerTest()
        : catalog_()
        , planner_(catalog_)
    {
    }

protected:
    void SetUp() override
    {
        test::VeloDBTest::SetUp();
        auto schema = Schema();
        schema.addColumnInfo({ "id", std::make_unique<IntegerType>() });
        schema.addColumnInfo({ "name", std::make_unique<VarcharType>(100) });

        auto builder = TableBuilder("users", std::move(schema));
        catalog_.addTable(std::move(builder).build());
    }

    void TearDown() override
    {
        test::VeloDBTest::TearDown();
        // Cleanup code if needed
    }

    Catalog catalog_;
    QueryPlanner planner_;
};

TEST_F(PlannerTest, PlanSimpleSelect)
{
    std::string sql = "SELECT * FROM users";

    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);

    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);

    const hsql::SQLStatement* stmt = result.getStatement(0);
    ASSERT_EQ(stmt->type(), hsql::kStmtSelect);

    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

    auto plan = planner_.planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);

    EXPECT_EQ(plan->getPlanType(), PlanType::PROJECTION);
    auto& children = plan->getChildren();
    EXPECT_EQ(children.size(), 1);
    EXPECT_EQ(children[0]->getPlanType(), PlanType::COMPACTION);
    EXPECT_EQ(plan->getOutputSchema().getColumnCount(), 4); // TODO: remove $_rowid and $_mask
}

TEST_F(PlannerTest, PlanSelectWithWhere)
{
    std::string sql = "SELECT * FROM users WHERE id = 1";

    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);

    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);

    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

    auto plan = planner_.planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);

    EXPECT_EQ(plan->getPlanType(), PlanType::PROJECTION);
    auto& children = plan->getChildren();
    EXPECT_EQ(children.size(), 1);
    EXPECT_EQ(children[0]->getPlanType(), PlanType::COMPACTION);
    auto& grand_children = children[0]->getChildren();
    EXPECT_EQ(grand_children.size(), 1);
    EXPECT_EQ(grand_children[0]->getPlanType(), PlanType::SEQ_SCAN);
}

TEST_F(PlannerTest, PlanSelectWithProjection)
{
    std::string sql = "SELECT id FROM users";

    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);

    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);

    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

    auto plan = planner_.planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);

    // Should create a plan with projection
    EXPECT_NE(plan->getPlanType(), PlanType::INVALID);
}

TEST_F(PlannerTest, PlanInvalidTable)
{
    std::string sql = "SELECT * FROM nonexistent_table";

    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);

    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);

    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

    // Should throw an exception for non-existent table
    EXPECT_THROW(planner_.planSelect(select_stmt), CatalogError);
}

TEST_F(PlannerTest, PlanComplexWhere)
{
    std::string sql = "SELECT * FROM users WHERE id > 0 AND name = 'Alice'";

    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql, &result);

    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.size(), 1);

    const hsql::SQLStatement* stmt = result.getStatement(0);
    const hsql::SelectStatement* select_stmt = static_cast<const hsql::SelectStatement*>(stmt);

    auto plan = planner_.planSelect(select_stmt);
    ASSERT_NE(plan, nullptr);

    // Should create a valid plan with complex predicate
    EXPECT_NE(plan->getPlanType(), PlanType::INVALID);
}

// TODO: Add more comprehensive planner tests when additional features are
// implemented
// - JOIN planning tests
// - Subquery planning tests
// - Aggregation planning tests
// - ORDER BY planning tests
// - LIMIT planning tests
// - Complex expression planning tests
