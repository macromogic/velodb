#include "catalog/tpch_catalog_builder.hpp"

#include "catalog/column.hpp"
#include "catalog/table_builder.hpp"
#include "data/data_type.hpp"

namespace velodb {

Catalog TPCHCatalogBuilder::createTPCHCatalog()
{
    Catalog catalog;

    // PART
    {
        Schema schema;
        schema.addColumnInfo(ColumnInfo("p_partkey", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("p_name", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("p_mfgr", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("p_brand", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("p_type", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("p_size", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("p_container", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("p_retailprice", DataType::createType(DataTypeId::DOUBLE)));
        schema.addColumnInfo(ColumnInfo("p_comment", DataType::createType(DataTypeId::VARCHAR)));
        catalog.addTable(TableBuilder("part", std::move(schema)).build());
    }

    // SUPPLIER
    {
        Schema schema;
        schema.addColumnInfo(ColumnInfo("s_suppkey", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("s_name", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("s_address", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("s_nationkey", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("s_phone", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("s_acctbal", DataType::createType(DataTypeId::DOUBLE)));
        schema.addColumnInfo(ColumnInfo("s_comment", DataType::createType(DataTypeId::VARCHAR)));
        catalog.addTable(TableBuilder("supplier", std::move(schema)).build());
    }

    // PARTSUPP
    {
        Schema schema;
        schema.addColumnInfo(ColumnInfo("ps_partkey", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("ps_suppkey", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("ps_availqty", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("ps_supplycost", DataType::createType(DataTypeId::DOUBLE)));
        schema.addColumnInfo(ColumnInfo("ps_comment", DataType::createType(DataTypeId::VARCHAR)));
        catalog.addTable(TableBuilder("partsupp", std::move(schema)).build());
    }

    // CUSTOMER
    {
        Schema schema;
        schema.addColumnInfo(ColumnInfo("c_custkey", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("c_name", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("c_address", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("c_nationkey", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("c_phone", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("c_acctbal", DataType::createType(DataTypeId::DOUBLE)));
        schema.addColumnInfo(ColumnInfo("c_mktsegment", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("c_comment", DataType::createType(DataTypeId::VARCHAR)));
        catalog.addTable(TableBuilder("customer", std::move(schema)).build());
    }

    // ORDERS
    {
        Schema schema;
        schema.addColumnInfo(ColumnInfo("o_orderkey", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("o_custkey", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("o_orderstatus", DataType::createType(DataTypeId::CHAR)));
        schema.addColumnInfo(ColumnInfo("o_totalprice", DataType::createType(DataTypeId::DOUBLE)));
        schema.addColumnInfo(ColumnInfo("o_orderdate", DataType::createType(DataTypeId::DATE)));
        schema.addColumnInfo(ColumnInfo("o_orderpriority", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("o_clerk", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("o_shippriority", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("o_comment", DataType::createType(DataTypeId::VARCHAR)));
        catalog.addTable(TableBuilder("orders", std::move(schema)).build());
    }

    // LINEITEM
    {
        Schema schema;
        schema.addColumnInfo(ColumnInfo("l_orderkey", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("l_partkey", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("l_suppkey", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("l_linenumber", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("l_quantity", DataType::createType(DataTypeId::DOUBLE)));
        schema.addColumnInfo(ColumnInfo("l_extendedprice", DataType::createType(DataTypeId::DOUBLE)));
        schema.addColumnInfo(ColumnInfo("l_discount", DataType::createType(DataTypeId::DOUBLE)));
        schema.addColumnInfo(ColumnInfo("l_tax", DataType::createType(DataTypeId::DOUBLE)));
        schema.addColumnInfo(ColumnInfo("l_returnflag", DataType::createType(DataTypeId::CHAR)));
        schema.addColumnInfo(ColumnInfo("l_linestatus", DataType::createType(DataTypeId::CHAR)));
        schema.addColumnInfo(ColumnInfo("l_shipdate", DataType::createType(DataTypeId::DATE)));
        schema.addColumnInfo(ColumnInfo("l_commitdate", DataType::createType(DataTypeId::DATE)));
        schema.addColumnInfo(ColumnInfo("l_receiptdate", DataType::createType(DataTypeId::DATE)));
        schema.addColumnInfo(ColumnInfo("l_shipinstruct", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("l_shipmode", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("l_comment", DataType::createType(DataTypeId::VARCHAR)));
        catalog.addTable(TableBuilder("lineitem", std::move(schema)).build());
    }

    // NATION
    {
        Schema schema;
        schema.addColumnInfo(ColumnInfo("n_nationkey", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("n_name", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("n_regionkey", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("n_comment", DataType::createType(DataTypeId::VARCHAR)));
        catalog.addTable(TableBuilder("nation", std::move(schema)).build());
    }

    // REGION
    {
        Schema schema;
        schema.addColumnInfo(ColumnInfo("r_regionkey", DataType::createType(DataTypeId::INTEGER)));
        schema.addColumnInfo(ColumnInfo("r_name", DataType::createType(DataTypeId::VARCHAR)));
        schema.addColumnInfo(ColumnInfo("r_comment", DataType::createType(DataTypeId::VARCHAR)));
        catalog.addTable(TableBuilder("region", std::move(schema)).build());
    }

    return catalog;
}

} // namespace velodb
