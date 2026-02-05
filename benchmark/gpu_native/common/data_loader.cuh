#pragma once

#include "types.hpp"

#include "launcher.cuh"

#include <fmt/core.h>

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace gpu_native {

// ============================================================================
// String Encoding Utilities
// ============================================================================

inline int32_t encode_mktsegment(const std::string& s)
{
    static const std::unordered_map<std::string, int32_t> map
        = { { "BUILDING", 0 }, { "AUTOMOBILE", 1 }, { "MACHINERY", 2 }, { "HOUSEHOLD", 3 }, { "FURNITURE", 4 } };
    auto it = map.find(s);
    return it != map.end() ? it->second : -1;
}

inline int32_t encode_shipmode(const std::string& s)
{
    static const std::unordered_map<std::string, int32_t> map = { { "REG AIR", 0 }, { "AIR", 1 },   { "RAIL", 2 },
                                                                  { "SHIP", 3 },    { "TRUCK", 4 }, { "MAIL", 5 },
                                                                  { "FOB", 6 } };
    auto it = map.find(s);
    return it != map.end() ? it->second : -1;
}

inline int32_t encode_shipinstruct(const std::string& s)
{
    static const std::unordered_map<std::string, int32_t> map
        = { { "DELIVER IN PERSON", 0 }, { "COLLECT COD", 1 }, { "NONE", 2 }, { "TAKE BACK RETURN", 3 } };
    auto it = map.find(s);
    return it != map.end() ? it->second : -1;
}

inline int32_t encode_container(const std::string& s)
{
    static const std::unordered_map<std::string, int32_t> map = {
        { "SM CASE", 0 },    { "SM BOX", 1 },     { "SM PACK", 2 },     { "SM PKG", 3 },     { "MED BAG", 4 },
        { "MED BOX", 5 },    { "MED PACK", 6 },   { "MED PKG", 7 },     { "LG CASE", 8 },    { "LG BOX", 9 },
        { "LG PACK", 10 },   { "LG PKG", 11 },    { "JUMBO CASE", 12 }, { "JUMBO BOX", 13 }, { "JUMBO PACK", 14 },
        { "JUMBO PKG", 15 }, { "WRAP CASE", 16 }, { "WRAP BOX", 17 },   { "WRAP PACK", 18 }, { "WRAP PKG", 19 }
    };
    auto it = map.find(s);
    return it != map.end() ? it->second : -1;
}

inline int32_t encode_region(const std::string& s)
{
    static const std::unordered_map<std::string, int32_t> map
        = { { "AFRICA", 0 }, { "AMERICA", 1 }, { "ASIA", 2 }, { "EUROPE", 3 }, { "MIDDLE EAST", 4 } };
    auto it = map.find(s);
    return it != map.end() ? it->second : -1;
}

inline int32_t encode_brand(const std::string& s)
{
    // Brand#12 -> 12
    if (s.substr(0, 6) == "Brand#") {
        return std::stoi(s.substr(6));
    }
    return -1;
}

inline int32_t parse_date(const std::string& s)
{
    // YYYY-MM-DD -> YYYYMMDD
    int year = std::stoi(s.substr(0, 4));
    int month = std::stoi(s.substr(5, 2));
    int day = std::stoi(s.substr(8, 2));
    return make_date(year, month, day);
}

// ============================================================================
// TBL File Parsing Helpers
// ============================================================================

inline std::vector<std::string> split_line(const std::string& line, char delim = '|')
{
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

// ============================================================================
// Lineitem Loader
// ============================================================================

inline LineitemColumns load_lineitem(const std::string& path, cudaStream_t stream = 0)
{
    fmt::println("Loading lineitem from {}...", path);

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open: " + path);
    }

    // First pass: count lines
    std::string line;
    size_t num_rows = 0;
    while (std::getline(file, line)) {
        if (!line.empty())
            num_rows++;
    }

    fmt::println("  {} rows", num_rows);

    // Allocate host buffers
    std::vector<int32_t> h_orderkey(num_rows);
    std::vector<int32_t> h_partkey(num_rows);
    std::vector<int32_t> h_suppkey(num_rows);
    std::vector<int32_t> h_linenumber(num_rows);
    std::vector<double> h_quantity(num_rows);
    std::vector<double> h_extendedprice(num_rows);
    std::vector<double> h_discount(num_rows);
    std::vector<double> h_tax(num_rows);
    std::vector<int8_t> h_returnflag(num_rows);
    std::vector<int8_t> h_linestatus(num_rows);
    std::vector<int32_t> h_shipdate(num_rows);
    std::vector<int32_t> h_commitdate(num_rows);
    std::vector<int32_t> h_receiptdate(num_rows);
    std::vector<int32_t> h_shipinstruct(num_rows);
    std::vector<int32_t> h_shipmode(num_rows);

    // Second pass: parse data
    file.clear();
    file.seekg(0);
    size_t idx = 0;
    while (std::getline(file, line)) {
        if (line.empty())
            continue;
        auto tokens = split_line(line);
        if (tokens.size() < 16)
            continue;

        h_orderkey[idx] = std::stoi(tokens[0]);
        h_partkey[idx] = std::stoi(tokens[1]);
        h_suppkey[idx] = std::stoi(tokens[2]);
        h_linenumber[idx] = std::stoi(tokens[3]);
        h_quantity[idx] = std::stod(tokens[4]);
        h_extendedprice[idx] = std::stod(tokens[5]);
        h_discount[idx] = std::stod(tokens[6]);
        h_tax[idx] = std::stod(tokens[7]);
        h_returnflag[idx] = tokens[8][0];
        h_linestatus[idx] = tokens[9][0];
        h_shipdate[idx] = parse_date(tokens[10]);
        h_commitdate[idx] = parse_date(tokens[11]);
        h_receiptdate[idx] = parse_date(tokens[12]);
        h_shipinstruct[idx] = encode_shipinstruct(tokens[13]);
        h_shipmode[idx] = encode_shipmode(tokens[14]);
        idx++;
    }

    // Allocate and copy to GPU
    LineitemColumns cols;
    cols.num_rows = num_rows;

    GPU_CHECK(cudaMalloc(&cols.l_orderkey, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.l_partkey, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.l_suppkey, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.l_linenumber, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.l_quantity, num_rows * sizeof(double)));
    GPU_CHECK(cudaMalloc(&cols.l_extendedprice, num_rows * sizeof(double)));
    GPU_CHECK(cudaMalloc(&cols.l_discount, num_rows * sizeof(double)));
    GPU_CHECK(cudaMalloc(&cols.l_tax, num_rows * sizeof(double)));
    GPU_CHECK(cudaMalloc(&cols.l_returnflag, num_rows * sizeof(int8_t)));
    GPU_CHECK(cudaMalloc(&cols.l_linestatus, num_rows * sizeof(int8_t)));
    GPU_CHECK(cudaMalloc(&cols.l_shipdate, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.l_commitdate, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.l_receiptdate, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.l_shipinstruct, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.l_shipmode, num_rows * sizeof(int32_t)));

    GPU_CHECK(cudaMemcpyAsync(cols.l_orderkey,
                              h_orderkey.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(
        cudaMemcpyAsync(cols.l_partkey, h_partkey.data(), num_rows * sizeof(int32_t), cudaMemcpyHostToDevice, stream));
    GPU_CHECK(
        cudaMemcpyAsync(cols.l_suppkey, h_suppkey.data(), num_rows * sizeof(int32_t), cudaMemcpyHostToDevice, stream));
    GPU_CHECK(cudaMemcpyAsync(cols.l_linenumber,
                              h_linenumber.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(
        cudaMemcpyAsync(cols.l_quantity, h_quantity.data(), num_rows * sizeof(double), cudaMemcpyHostToDevice, stream));
    GPU_CHECK(cudaMemcpyAsync(cols.l_extendedprice,
                              h_extendedprice.data(),
                              num_rows * sizeof(double),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(
        cudaMemcpyAsync(cols.l_discount, h_discount.data(), num_rows * sizeof(double), cudaMemcpyHostToDevice, stream));
    GPU_CHECK(cudaMemcpyAsync(cols.l_tax, h_tax.data(), num_rows * sizeof(double), cudaMemcpyHostToDevice, stream));
    GPU_CHECK(cudaMemcpyAsync(cols.l_returnflag,
                              h_returnflag.data(),
                              num_rows * sizeof(int8_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(cudaMemcpyAsync(cols.l_linestatus,
                              h_linestatus.data(),
                              num_rows * sizeof(int8_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(cudaMemcpyAsync(cols.l_shipdate,
                              h_shipdate.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(cudaMemcpyAsync(cols.l_commitdate,
                              h_commitdate.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(cudaMemcpyAsync(cols.l_receiptdate,
                              h_receiptdate.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(cudaMemcpyAsync(cols.l_shipinstruct,
                              h_shipinstruct.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(cudaMemcpyAsync(cols.l_shipmode,
                              h_shipmode.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));

    GPU_CHECK(cudaStreamSynchronize(stream));
    fmt::println("  Lineitem loaded to GPU");

    return cols;
}

// ============================================================================
// Orders Loader
// ============================================================================

inline OrdersColumns load_orders(const std::string& path, cudaStream_t stream = 0)
{
    fmt::println("Loading orders from {}...", path);

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open: " + path);
    }

    std::string line;
    size_t num_rows = 0;
    while (std::getline(file, line)) {
        if (!line.empty())
            num_rows++;
    }

    fmt::println("  {} rows", num_rows);

    std::vector<int32_t> h_orderkey(num_rows);
    std::vector<int32_t> h_custkey(num_rows);
    std::vector<int8_t> h_orderstatus(num_rows);
    std::vector<double> h_totalprice(num_rows);
    std::vector<int32_t> h_orderdate(num_rows);
    std::vector<int32_t> h_orderpriority(num_rows);
    std::vector<int32_t> h_clerk(num_rows);
    std::vector<int32_t> h_shippriority(num_rows);

    file.clear();
    file.seekg(0);
    size_t idx = 0;
    while (std::getline(file, line)) {
        if (line.empty())
            continue;
        auto tokens = split_line(line);
        if (tokens.size() < 9)
            continue;

        h_orderkey[idx] = std::stoi(tokens[0]);
        h_custkey[idx] = std::stoi(tokens[1]);
        h_orderstatus[idx] = tokens[2][0];
        h_totalprice[idx] = std::stod(tokens[3]);
        h_orderdate[idx] = parse_date(tokens[4]);
        // orderpriority: "1-URGENT" -> 1, "2-HIGH" -> 2, etc.
        h_orderpriority[idx] = tokens[5][0] - '0';
        // clerk: "Clerk#000000001" -> 1
        h_clerk[idx] = std::stoi(tokens[6].substr(6));
        h_shippriority[idx] = std::stoi(tokens[7]);
        idx++;
    }

    OrdersColumns cols;
    cols.num_rows = num_rows;

    GPU_CHECK(cudaMalloc(&cols.o_orderkey, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.o_custkey, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.o_orderstatus, num_rows * sizeof(int8_t)));
    GPU_CHECK(cudaMalloc(&cols.o_totalprice, num_rows * sizeof(double)));
    GPU_CHECK(cudaMalloc(&cols.o_orderdate, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.o_orderpriority, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.o_clerk, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.o_shippriority, num_rows * sizeof(int32_t)));

    GPU_CHECK(cudaMemcpyAsync(cols.o_orderkey,
                              h_orderkey.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(
        cudaMemcpyAsync(cols.o_custkey, h_custkey.data(), num_rows * sizeof(int32_t), cudaMemcpyHostToDevice, stream));
    GPU_CHECK(cudaMemcpyAsync(cols.o_orderstatus,
                              h_orderstatus.data(),
                              num_rows * sizeof(int8_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(cudaMemcpyAsync(cols.o_totalprice,
                              h_totalprice.data(),
                              num_rows * sizeof(double),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(cudaMemcpyAsync(cols.o_orderdate,
                              h_orderdate.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(cudaMemcpyAsync(cols.o_orderpriority,
                              h_orderpriority.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(
        cudaMemcpyAsync(cols.o_clerk, h_clerk.data(), num_rows * sizeof(int32_t), cudaMemcpyHostToDevice, stream));
    GPU_CHECK(cudaMemcpyAsync(cols.o_shippriority,
                              h_shippriority.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));

    GPU_CHECK(cudaStreamSynchronize(stream));
    fmt::println("  Orders loaded to GPU");

    return cols;
}

// ============================================================================
// Customer Loader
// ============================================================================

inline CustomerColumns load_customer(const std::string& path, cudaStream_t stream = 0)
{
    fmt::println("Loading customer from {}...", path);

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open: " + path);
    }

    std::string line;
    size_t num_rows = 0;
    while (std::getline(file, line)) {
        if (!line.empty())
            num_rows++;
    }

    fmt::println("  {} rows", num_rows);

    std::vector<int32_t> h_custkey(num_rows);
    std::vector<int32_t> h_nationkey(num_rows);
    std::vector<double> h_acctbal(num_rows);
    std::vector<int32_t> h_mktsegment(num_rows);

    file.clear();
    file.seekg(0);
    size_t idx = 0;
    while (std::getline(file, line)) {
        if (line.empty())
            continue;
        auto tokens = split_line(line);
        if (tokens.size() < 8)
            continue;

        h_custkey[idx] = std::stoi(tokens[0]);
        h_nationkey[idx] = std::stoi(tokens[3]);
        h_acctbal[idx] = std::stod(tokens[5]);
        h_mktsegment[idx] = encode_mktsegment(tokens[6]);
        idx++;
    }

    CustomerColumns cols;
    cols.num_rows = num_rows;

    GPU_CHECK(cudaMalloc(&cols.c_custkey, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.c_nationkey, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.c_acctbal, num_rows * sizeof(double)));
    GPU_CHECK(cudaMalloc(&cols.c_mktsegment, num_rows * sizeof(int32_t)));

    // Unused columns - set to nullptr
    cols.c_name = nullptr;
    cols.c_address = nullptr;
    cols.c_phone = nullptr;
    cols.c_comment = nullptr;

    GPU_CHECK(
        cudaMemcpyAsync(cols.c_custkey, h_custkey.data(), num_rows * sizeof(int32_t), cudaMemcpyHostToDevice, stream));
    GPU_CHECK(cudaMemcpyAsync(cols.c_nationkey,
                              h_nationkey.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(
        cudaMemcpyAsync(cols.c_acctbal, h_acctbal.data(), num_rows * sizeof(double), cudaMemcpyHostToDevice, stream));
    GPU_CHECK(cudaMemcpyAsync(cols.c_mktsegment,
                              h_mktsegment.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));

    GPU_CHECK(cudaStreamSynchronize(stream));
    fmt::println("  Customer loaded to GPU");

    return cols;
}

// ============================================================================
// Part Loader
// ============================================================================

inline PartColumns load_part(const std::string& path, cudaStream_t stream = 0)
{
    fmt::println("Loading part from {}...", path);

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open: " + path);
    }

    std::string line;
    size_t num_rows = 0;
    while (std::getline(file, line)) {
        if (!line.empty())
            num_rows++;
    }

    fmt::println("  {} rows", num_rows);

    std::vector<int32_t> h_partkey(num_rows);
    std::vector<int32_t> h_brand(num_rows);
    std::vector<int32_t> h_size(num_rows);
    std::vector<int32_t> h_container(num_rows);
    std::vector<double> h_retailprice(num_rows);

    file.clear();
    file.seekg(0);
    size_t idx = 0;
    while (std::getline(file, line)) {
        if (line.empty())
            continue;
        auto tokens = split_line(line);
        if (tokens.size() < 9)
            continue;

        h_partkey[idx] = std::stoi(tokens[0]);
        h_brand[idx] = encode_brand(tokens[3]);
        h_size[idx] = std::stoi(tokens[5]);
        h_container[idx] = encode_container(tokens[6]);
        h_retailprice[idx] = std::stod(tokens[7]);
        idx++;
    }

    PartColumns cols;
    cols.num_rows = num_rows;

    GPU_CHECK(cudaMalloc(&cols.p_partkey, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.p_brand, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.p_size, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.p_container, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.p_retailprice, num_rows * sizeof(double)));

    // Unused columns
    cols.p_name = nullptr;
    cols.p_mfgr = nullptr;
    cols.p_type = nullptr;

    GPU_CHECK(
        cudaMemcpyAsync(cols.p_partkey, h_partkey.data(), num_rows * sizeof(int32_t), cudaMemcpyHostToDevice, stream));
    GPU_CHECK(
        cudaMemcpyAsync(cols.p_brand, h_brand.data(), num_rows * sizeof(int32_t), cudaMemcpyHostToDevice, stream));
    GPU_CHECK(cudaMemcpyAsync(cols.p_size, h_size.data(), num_rows * sizeof(int32_t), cudaMemcpyHostToDevice, stream));
    GPU_CHECK(cudaMemcpyAsync(cols.p_container,
                              h_container.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(cudaMemcpyAsync(cols.p_retailprice,
                              h_retailprice.data(),
                              num_rows * sizeof(double),
                              cudaMemcpyHostToDevice,
                              stream));

    GPU_CHECK(cudaStreamSynchronize(stream));
    fmt::println("  Part loaded to GPU");

    return cols;
}

// ============================================================================
// Supplier Loader
// ============================================================================

inline SupplierColumns load_supplier(const std::string& path, cudaStream_t stream = 0)
{
    fmt::println("Loading supplier from {}...", path);

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open: " + path);
    }

    std::string line;
    size_t num_rows = 0;
    while (std::getline(file, line)) {
        if (!line.empty())
            num_rows++;
    }

    fmt::println("  {} rows", num_rows);

    std::vector<int32_t> h_suppkey(num_rows);
    std::vector<int32_t> h_nationkey(num_rows);
    std::vector<double> h_acctbal(num_rows);

    file.clear();
    file.seekg(0);
    size_t idx = 0;
    while (std::getline(file, line)) {
        if (line.empty())
            continue;
        auto tokens = split_line(line);
        if (tokens.size() < 7)
            continue;

        h_suppkey[idx] = std::stoi(tokens[0]);
        h_nationkey[idx] = std::stoi(tokens[3]);
        h_acctbal[idx] = std::stod(tokens[5]);
        idx++;
    }

    SupplierColumns cols;
    cols.num_rows = num_rows;

    GPU_CHECK(cudaMalloc(&cols.s_suppkey, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.s_nationkey, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.s_acctbal, num_rows * sizeof(double)));

    cols.s_name = nullptr;
    cols.s_address = nullptr;
    cols.s_phone = nullptr;
    cols.s_comment = nullptr;

    GPU_CHECK(
        cudaMemcpyAsync(cols.s_suppkey, h_suppkey.data(), num_rows * sizeof(int32_t), cudaMemcpyHostToDevice, stream));
    GPU_CHECK(cudaMemcpyAsync(cols.s_nationkey,
                              h_nationkey.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(
        cudaMemcpyAsync(cols.s_acctbal, h_acctbal.data(), num_rows * sizeof(double), cudaMemcpyHostToDevice, stream));

    GPU_CHECK(cudaStreamSynchronize(stream));
    fmt::println("  Supplier loaded to GPU");

    return cols;
}

// ============================================================================
// Nation Loader
// ============================================================================

inline NationColumns load_nation(const std::string& path, cudaStream_t stream = 0)
{
    fmt::println("Loading nation from {}...", path);

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open: " + path);
    }

    std::string line;
    size_t num_rows = 0;
    while (std::getline(file, line)) {
        if (!line.empty())
            num_rows++;
    }

    fmt::println("  {} rows", num_rows);

    std::vector<int32_t> h_nationkey(num_rows);
    std::vector<int32_t> h_name(num_rows); // We'll encode nation names
    std::vector<int32_t> h_regionkey(num_rows);

    // Nation name encoding
    std::unordered_map<std::string, int32_t> nation_map;

    file.clear();
    file.seekg(0);
    size_t idx = 0;
    while (std::getline(file, line)) {
        if (line.empty())
            continue;
        auto tokens = split_line(line);
        if (tokens.size() < 4)
            continue;

        h_nationkey[idx] = std::stoi(tokens[0]);
        // Store nation name encoding (just use nationkey as id for simplicity)
        h_name[idx] = h_nationkey[idx];
        h_regionkey[idx] = std::stoi(tokens[2]);
        idx++;
    }

    NationColumns cols;
    cols.num_rows = num_rows;

    GPU_CHECK(cudaMalloc(&cols.n_nationkey, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.n_name, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.n_regionkey, num_rows * sizeof(int32_t)));

    GPU_CHECK(cudaMemcpyAsync(cols.n_nationkey,
                              h_nationkey.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(cudaMemcpyAsync(cols.n_name, h_name.data(), num_rows * sizeof(int32_t), cudaMemcpyHostToDevice, stream));
    GPU_CHECK(cudaMemcpyAsync(cols.n_regionkey,
                              h_regionkey.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));

    GPU_CHECK(cudaStreamSynchronize(stream));
    fmt::println("  Nation loaded to GPU");

    return cols;
}

// ============================================================================
// Region Loader
// ============================================================================

inline RegionColumns load_region(const std::string& path, cudaStream_t stream = 0)
{
    fmt::println("Loading region from {}...", path);

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open: " + path);
    }

    std::string line;
    size_t num_rows = 0;
    while (std::getline(file, line)) {
        if (!line.empty())
            num_rows++;
    }

    fmt::println("  {} rows", num_rows);

    std::vector<int32_t> h_regionkey(num_rows);
    std::vector<int32_t> h_name(num_rows);

    file.clear();
    file.seekg(0);
    size_t idx = 0;
    while (std::getline(file, line)) {
        if (line.empty())
            continue;
        auto tokens = split_line(line);
        if (tokens.size() < 3)
            continue;

        h_regionkey[idx] = std::stoi(tokens[0]);
        h_name[idx] = encode_region(tokens[1]);
        idx++;
    }

    RegionColumns cols;
    cols.num_rows = num_rows;

    GPU_CHECK(cudaMalloc(&cols.r_regionkey, num_rows * sizeof(int32_t)));
    GPU_CHECK(cudaMalloc(&cols.r_name, num_rows * sizeof(int32_t)));

    GPU_CHECK(cudaMemcpyAsync(cols.r_regionkey,
                              h_regionkey.data(),
                              num_rows * sizeof(int32_t),
                              cudaMemcpyHostToDevice,
                              stream));
    GPU_CHECK(cudaMemcpyAsync(cols.r_name, h_name.data(), num_rows * sizeof(int32_t), cudaMemcpyHostToDevice, stream));

    GPU_CHECK(cudaStreamSynchronize(stream));
    fmt::println("  Region loaded to GPU");

    return cols;
}

// ============================================================================
// Load All Tables
// ============================================================================

inline TPCHTables load_tpch_tables(const std::string& data_dir, cudaStream_t stream = 0)
{
    TPCHTables tables;

    tables.lineitem = load_lineitem(data_dir + "/lineitem.tbl", stream);
    tables.orders = load_orders(data_dir + "/orders.tbl", stream);
    tables.customer = load_customer(data_dir + "/customer.tbl", stream);
    tables.part = load_part(data_dir + "/part.tbl", stream);
    tables.supplier = load_supplier(data_dir + "/supplier.tbl", stream);
    tables.nation = load_nation(data_dir + "/nation.tbl", stream);
    tables.region = load_region(data_dir + "/region.tbl", stream);

    fmt::println("All tables loaded!");
    return tables;
}

// ============================================================================
// Free Tables
// ============================================================================

inline void free_lineitem(LineitemColumns& cols)
{
    cudaFree(cols.l_orderkey);
    cudaFree(cols.l_partkey);
    cudaFree(cols.l_suppkey);
    cudaFree(cols.l_linenumber);
    cudaFree(cols.l_quantity);
    cudaFree(cols.l_extendedprice);
    cudaFree(cols.l_discount);
    cudaFree(cols.l_tax);
    cudaFree(cols.l_returnflag);
    cudaFree(cols.l_linestatus);
    cudaFree(cols.l_shipdate);
    cudaFree(cols.l_commitdate);
    cudaFree(cols.l_receiptdate);
    cudaFree(cols.l_shipinstruct);
    cudaFree(cols.l_shipmode);
}

inline void free_orders(OrdersColumns& cols)
{
    cudaFree(cols.o_orderkey);
    cudaFree(cols.o_custkey);
    cudaFree(cols.o_orderstatus);
    cudaFree(cols.o_totalprice);
    cudaFree(cols.o_orderdate);
    cudaFree(cols.o_orderpriority);
    cudaFree(cols.o_clerk);
    cudaFree(cols.o_shippriority);
}

inline void free_customer(CustomerColumns& cols)
{
    cudaFree(cols.c_custkey);
    cudaFree(cols.c_nationkey);
    cudaFree(cols.c_acctbal);
    cudaFree(cols.c_mktsegment);
}

inline void free_part(PartColumns& cols)
{
    cudaFree(cols.p_partkey);
    cudaFree(cols.p_brand);
    cudaFree(cols.p_size);
    cudaFree(cols.p_container);
    cudaFree(cols.p_retailprice);
}

inline void free_tpch_tables(TPCHTables& tables)
{
    free_lineitem(tables.lineitem);
    free_orders(tables.orders);
    free_customer(tables.customer);
    free_part(tables.part);
    cudaFree(tables.supplier.s_suppkey);
    cudaFree(tables.supplier.s_nationkey);
    cudaFree(tables.supplier.s_acctbal);
    cudaFree(tables.nation.n_nationkey);
    cudaFree(tables.nation.n_name);
    cudaFree(tables.nation.n_regionkey);
    cudaFree(tables.region.r_regionkey);
    cudaFree(tables.region.r_name);
}

} // namespace gpu_native
