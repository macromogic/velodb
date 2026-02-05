#pragma once

#include <cstddef>
#include <cstdint>

#include <cuda_runtime.h>

namespace gpu_native {

// ============================================================================
// TPC-H Table Structures (Columnar Layout)
// ============================================================================

struct LineitemColumns {
    int32_t* l_orderkey;
    int32_t* l_partkey;
    int32_t* l_suppkey;
    int32_t* l_linenumber;
    double* l_quantity;
    double* l_extendedprice;
    double* l_discount;
    double* l_tax;
    int8_t* l_returnflag; // 'A', 'N', 'R' -> 0, 1, 2
    int8_t* l_linestatus; // 'F', 'O' -> 0, 1
    int32_t* l_shipdate; // YYYYMMDD as int
    int32_t* l_commitdate;
    int32_t* l_receiptdate;
    int32_t* l_shipinstruct; // encoded
    int32_t* l_shipmode; // encoded
    size_t num_rows;
};

struct OrdersColumns {
    int32_t* o_orderkey;
    int32_t* o_custkey;
    int8_t* o_orderstatus; // 'F', 'O', 'P' -> 0, 1, 2
    double* o_totalprice;
    int32_t* o_orderdate; // YYYYMMDD as int
    int32_t* o_orderpriority; // encoded: 1-URGENT, 2-HIGH, etc.
    int32_t* o_clerk; // encoded
    int32_t* o_shippriority;
    size_t num_rows;
};

struct CustomerColumns {
    int32_t* c_custkey;
    int32_t* c_name; // encoded or pointer
    int32_t* c_address; // encoded
    int32_t* c_nationkey;
    int32_t* c_phone; // encoded
    double* c_acctbal;
    int32_t* c_mktsegment; // encoded: BUILDING=0, AUTOMOBILE=1, etc.
    int32_t* c_comment; // encoded
    size_t num_rows;
};

struct PartColumns {
    int32_t* p_partkey;
    int32_t* p_name; // encoded
    int32_t* p_mfgr; // encoded
    int32_t* p_brand; // encoded: Brand#12 -> 12
    int32_t* p_type; // encoded
    int32_t* p_size;
    int32_t* p_container; // encoded
    double* p_retailprice;
    size_t num_rows;
};

struct SupplierColumns {
    int32_t* s_suppkey;
    int32_t* s_name; // encoded
    int32_t* s_address; // encoded
    int32_t* s_nationkey;
    int32_t* s_phone; // encoded
    double* s_acctbal;
    int32_t* s_comment; // encoded
    size_t num_rows;
};

struct NationColumns {
    int32_t* n_nationkey;
    int32_t* n_name; // encoded: CHINA=0, etc.
    int32_t* n_regionkey;
    size_t num_rows;
};

struct RegionColumns {
    int32_t* r_regionkey;
    int32_t* r_name; // encoded: ASIA=0, EUROPE=1, etc.
    size_t num_rows;
};

struct PartsuppColumns {
    int32_t* ps_partkey;
    int32_t* ps_suppkey;
    int32_t* ps_availqty;
    double* ps_supplycost;
    size_t num_rows;
};

// ============================================================================
// All TPC-H Tables
// ============================================================================

struct TPCHTables {
    LineitemColumns lineitem;
    OrdersColumns orders;
    CustomerColumns customer;
    PartColumns part;
    SupplierColumns supplier;
    NationColumns nation;
    RegionColumns region;
    PartsuppColumns partsupp;
};

// ============================================================================
// Hash Table Entry
// ============================================================================

constexpr uint32_t HASH_EMPTY = 0xFFFFFFFF;

struct HashEntry {
    int32_t key;
    int32_t rowid;
    uint32_t next;
};

struct HashTable {
    HashEntry* entries;
    uint32_t* heads;
    uint32_t* counter;
    uint32_t num_buckets;
    uint32_t capacity;
};

// ============================================================================
// String Encoding Maps
// ============================================================================

// Market segments
enum class MktSegment : int32_t {
    BUILDING = 0,
    AUTOMOBILE = 1,
    MACHINERY = 2,
    HOUSEHOLD = 3,
    FURNITURE = 4
};

// Ship modes
enum class ShipMode : int32_t {
    REG_AIR = 0,
    AIR = 1,
    RAIL = 2,
    SHIP = 3,
    TRUCK = 4,
    MAIL = 5,
    FOB = 6
};

// Ship instructions
enum class ShipInstruct : int32_t {
    DELIVER_IN_PERSON = 0,
    COLLECT_COD = 1,
    NONE = 2,
    TAKE_BACK_RETURN = 3
};

// Containers
enum class Container : int32_t {
    SM_CASE = 0,
    SM_BOX = 1,
    SM_PACK = 2,
    SM_PKG = 3,
    MED_BAG = 4,
    MED_BOX = 5,
    MED_PACK = 6,
    MED_PKG = 7,
    LG_CASE = 8,
    LG_BOX = 9,
    LG_PACK = 10,
    LG_PKG = 11,
    JUMBO_CASE = 12,
    JUMBO_BOX = 13,
    JUMBO_PACK = 14,
    JUMBO_PKG = 15,
    WRAP_CASE = 16,
    WRAP_BOX = 17,
    WRAP_PACK = 18,
    WRAP_PKG = 19
};

// Regions
enum class Region : int32_t {
    AFRICA = 0,
    AMERICA = 1,
    ASIA = 2,
    EUROPE = 3,
    MIDDLE_EAST = 4
};

// ============================================================================
// Date Utilities
// ============================================================================

__host__ __device__ inline int32_t make_date(int year, int month, int day)
{
    return year * 10000 + month * 100 + day;
}

__host__ __device__ inline int get_year(int32_t date)
{
    return date / 10000;
}

__host__ __device__ inline int get_month(int32_t date)
{
    return (date / 100) % 100;
}

__host__ __device__ inline int get_day(int32_t date)
{
    return date % 100;
}

} // namespace gpu_native
