"""
TPC-H Memory Savings Analysis

This script computes the memory savings achieved by Oblivious Late Materialization (OLM)
compared to loading full tables for TPC-H queries Q3, Q5, Q10, and Q18.

OLM loads only the compact structure (join keys + filter columns) to GPU, while
traditional approaches must load all columns required for the full query.

Usage:
    python memory_savings_analysis.py [--sf SCALE_FACTOR]

Default scale factor is SF1000 (~6 billion rows total).
"""

import argparse
import colorsys
import os
from dataclasses import dataclass
from typing import Dict, List, Tuple

# Optional imports for plotting
try:
    import matplotlib
    matplotlib.use('Agg')  # Use non-interactive backend
    import matplotlib.pyplot as plt
    import numpy as np
    import seaborn as sns
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    np = None
    sns = None

FIGURE_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'figures')


def adjust_color(color):
    r, g, b = color
    h, l, s = colorsys.rgb_to_hls(r, g, b)
    l = max(0, min(1, l * 1.5))
    s = max(0, min(1, s * 0.5))
    r, g, b = colorsys.hls_to_rgb(h, l, s)
    return (r, g, b)


# =============================================================================
# TPC-H Table Schema Definitions (Column sizes in bytes)
# =============================================================================

# Standard TPC-H column sizes (estimated average sizes)
COLUMN_SIZES = {
    # LINEITEM columns
    'l_orderkey': 8,       # BIGINT
    'l_partkey': 8,
    'l_suppkey': 8,
    'l_linenumber': 4,
    'l_quantity': 8,       # DECIMAL(15,2) -> 8 bytes
    'l_extendedprice': 8,
    'l_discount': 8,
    'l_tax': 8,
    'l_returnflag': 1,
    'l_linestatus': 1,
    'l_shipdate': 4,       # DATE
    'l_commitdate': 4,
    'l_receiptdate': 4,
    'l_shipinstruct': 25,
    'l_shipmode': 10,
    'l_comment': 44,       # VARCHAR(44) avg

    # ORDERS columns
    'o_orderkey': 8,
    'o_custkey': 8,
    'o_orderstatus': 1,
    'o_totalprice': 8,
    'o_orderdate': 4,
    'o_orderpriority': 15,
    'o_clerk': 15,
    'o_shippriority': 4,
    'o_comment': 79,       # VARCHAR(79) avg

    # CUSTOMER columns
    'c_custkey': 8,
    'c_name': 25,
    'c_address': 40,       # VARCHAR(40) avg
    'c_nationkey': 4,
    'c_phone': 15,
    'c_acctbal': 8,
    'c_mktsegment': 10,
    'c_comment': 117,      # VARCHAR(117) avg

    # SUPPLIER columns
    's_suppkey': 8,
    's_name': 25,
    's_address': 40,
    's_nationkey': 4,
    's_phone': 15,
    's_acctbal': 8,
    's_comment': 101,

    # NATION columns
    'n_nationkey': 4,
    'n_name': 25,
    'n_regionkey': 4,
    'n_comment': 152,

    # REGION columns
    'r_regionkey': 4,
    'r_name': 25,
    'r_comment': 152,

    # PARTSUPP columns
    'ps_partkey': 8,
    'ps_suppkey': 8,
    'ps_availqty': 4,
    'ps_supplycost': 8,
    'ps_comment': 199,

    # PART columns
    'p_partkey': 8,
    'p_name': 55,
    'p_mfgr': 25,
    'p_brand': 10,
    'p_type': 25,
    'p_size': 4,
    'p_container': 10,
    'p_retailprice': 8,
    'p_comment': 23,
}

# Total row size for each table (sum of all columns)
TABLE_ROW_SIZES = {
    'lineitem': sum([
        COLUMN_SIZES['l_orderkey'], COLUMN_SIZES['l_partkey'], COLUMN_SIZES['l_suppkey'],
        COLUMN_SIZES['l_linenumber'], COLUMN_SIZES['l_quantity'], COLUMN_SIZES['l_extendedprice'],
        COLUMN_SIZES['l_discount'], COLUMN_SIZES['l_tax'], COLUMN_SIZES['l_returnflag'],
        COLUMN_SIZES['l_linestatus'], COLUMN_SIZES['l_shipdate'], COLUMN_SIZES['l_commitdate'],
        COLUMN_SIZES['l_receiptdate'], COLUMN_SIZES['l_shipinstruct'], COLUMN_SIZES['l_shipmode'],
        COLUMN_SIZES['l_comment']
    ]),  # ~155 bytes
    'orders': sum([
        COLUMN_SIZES['o_orderkey'], COLUMN_SIZES['o_custkey'], COLUMN_SIZES['o_orderstatus'],
        COLUMN_SIZES['o_totalprice'], COLUMN_SIZES['o_orderdate'], COLUMN_SIZES['o_orderpriority'],
        COLUMN_SIZES['o_clerk'], COLUMN_SIZES['o_shippriority'], COLUMN_SIZES['o_comment']
    ]),  # ~142 bytes
    'customer': sum([
        COLUMN_SIZES['c_custkey'], COLUMN_SIZES['c_name'], COLUMN_SIZES['c_address'],
        COLUMN_SIZES['c_nationkey'], COLUMN_SIZES['c_phone'], COLUMN_SIZES['c_acctbal'],
        COLUMN_SIZES['c_mktsegment'], COLUMN_SIZES['c_comment']
    ]),  # ~227 bytes
    'supplier': sum([
        COLUMN_SIZES['s_suppkey'], COLUMN_SIZES['s_name'], COLUMN_SIZES['s_address'],
        COLUMN_SIZES['s_nationkey'], COLUMN_SIZES['s_phone'], COLUMN_SIZES['s_acctbal'],
        COLUMN_SIZES['s_comment']
    ]),  # ~201 bytes
    'nation': sum([
        COLUMN_SIZES['n_nationkey'], COLUMN_SIZES['n_name'], COLUMN_SIZES['n_regionkey'],
        COLUMN_SIZES['n_comment']
    ]),  # ~185 bytes
    'region': sum([
        COLUMN_SIZES['r_regionkey'], COLUMN_SIZES['r_name'], COLUMN_SIZES['r_comment']
    ]),  # ~181 bytes
    'partsupp': sum([
        COLUMN_SIZES['ps_partkey'], COLUMN_SIZES['ps_suppkey'], COLUMN_SIZES['ps_availqty'],
        COLUMN_SIZES['ps_supplycost'], COLUMN_SIZES['ps_comment']
    ]),  # ~227 bytes
    'part': sum([
        COLUMN_SIZES['p_partkey'], COLUMN_SIZES['p_name'], COLUMN_SIZES['p_mfgr'],
        COLUMN_SIZES['p_brand'], COLUMN_SIZES['p_type'], COLUMN_SIZES['p_size'],
        COLUMN_SIZES['p_container'], COLUMN_SIZES['p_retailprice'], COLUMN_SIZES['p_comment']
    ]),  # ~168 bytes
}

# =============================================================================
# TPC-H Row Counts at SF1 (scales linearly)
# =============================================================================

SF1_ROW_COUNTS = {
    'lineitem': 6_000_000,
    'orders': 1_500_000,
    'customer': 150_000,
    'supplier': 10_000,
    'nation': 25,       # Fixed, does not scale
    'region': 5,        # Fixed, does not scale
    'partsupp': 800_000,
    'part': 200_000,
}


@dataclass
class QueryMemoryAnalysis:
    """Memory analysis for a single TPC-H query."""
    query_name: str
    description: str

    # Tables involved in the query
    tables: List[str]

    # Columns needed for full execution (join + filter + projection + aggregation)
    full_columns: Dict[str, List[str]]

    # Columns needed for OLM compact structure (join keys + filter predicates only)
    olm_columns: Dict[str, List[str]]

    # Selectivity after filter for each table (fraction of rows that pass filter)
    filter_selectivity: Dict[str, float] = None

    # Join selectivity - fraction of rows remaining after joining with previous tables
    join_selectivity: Dict[str, float] = None

    # Hash table multiplier (capacity = rows * multiplier, entry size = 16 bytes)
    hash_table_factor: float = 2.0

    # Tables that need hash tables built (build side of joins)
    build_tables: List[str] = None

    # Probe table (streamed, not fully loaded)
    probe_table: str = None

    # Streaming chunk size in rows (for probe side)
    stream_chunk_rows: int = 1_000_000

    def __post_init__(self):
        if self.build_tables is None:
            self.build_tables = []
        if self.filter_selectivity is None:
            self.filter_selectivity = {}
        if self.join_selectivity is None:
            self.join_selectivity = {}


def get_row_count(table: str, sf: int) -> int:
    """Get row count for a table at given scale factor."""
    if table in ['nation', 'region']:
        return SF1_ROW_COUNTS[table]  # Fixed size tables
    return SF1_ROW_COUNTS[table] * sf


def compute_table_size(table: str, columns: List[str], sf: int) -> int:
    """Compute memory size for specified columns of a table."""
    row_count = get_row_count(table, sf)
    col_size = sum(COLUMN_SIZES.get(col, 8) for col in columns)
    return row_count * col_size


def compute_full_table_size(table: str, sf: int) -> int:
    """Compute memory size for full table."""
    row_count = get_row_count(table, sf)
    return row_count * TABLE_ROW_SIZES[table]


def compute_hash_table_size(table: str, sf: int, factor: float = 2.0) -> int:
    """Compute hash table size (open addressing with load factor ~0.5)."""
    row_count = get_row_count(table, sf)
    # HashEntry = { key: uint64_t, row_id: uint32_t, padding: 4 bytes } = 16 bytes
    hash_entry_size = 16
    capacity = int(row_count * factor)
    return capacity * hash_entry_size


def compute_compact_tuple_size(num_keys: int) -> int:
    """Compute size of CompactTuple structure."""
    # CompactTuple: { record_id: uint32_t, num_keys: uint32_t, keys[num_keys]: uint64_t[] }
    # In practice: record_id (4) + padding (4) + keys (8 * num_keys) = 8 + 8*num_keys
    # Or simplified: each CompactTuple = 8 bytes header + 8 bytes per key
    return 8 + 8 * num_keys


# =============================================================================
# Query Definitions with Selectivity Estimates
# =============================================================================

# TPC-H Filter Selectivity Estimates (based on standard benchmark data distribution)
# These are well-known selectivities from TPC-H literature
TPCH_SELECTIVITY = {
    # Q3 filters
    'c_mktsegment_building': 0.20,      # 1 of 5 segments
    'o_orderdate_lt_19950315': 0.48,    # ~48% of orders before mid-1995
    'l_shipdate_gt_19950315': 0.52,     # ~52% shipped after mid-1995

    # Q5 filters
    'r_name_asia': 0.20,                # 1 of 5 regions
    'n_in_region': 0.20,                # ~5 of 25 nations per region
    's_in_nation': 1.0,                 # suppliers filtered by nation
    'c_in_nation': 1.0,                 # customers filtered by nation
    'o_orderdate_1994': 0.16,           # 1 year out of ~6.5 years

    # Q10 filters
    'o_orderdate_q4_1993': 0.04,        # 3 months out of ~6.5 years
    'l_returnflag_r': 0.25,             # 'R' is ~25% of lineitems

    # Q18 filters (subquery)
    'l_orderkey_high_qty': 0.01,        # ~1% of orders have SUM(qty) > 300
}

def define_queries() -> List[QueryMemoryAnalysis]:
    """Define memory analysis for Q3, Q5, Q10, Q18 with proper selectivity."""

    queries = []

    # =========================================================================
    # Q3: Shipping Priority
    # =========================================================================
    # Execution:
    #   1. Filter customer (mktsegment='BUILDING') → 20% remain
    #   2. Build customer HT (filtered)
    #   3. Filter orders (orderdate < date) AND probe customer → ~10% remain
    #   4. Build orders HT (filtered + joined)
    #   5. Stream lineitem, filter (shipdate > date), probe orders HT

    q3 = QueryMemoryAnalysis(
        query_name="Q3",
        description="Shipping Priority (C ⋈ O ⋈ L)",
        tables=['customer', 'orders', 'lineitem'],
        full_columns={
            'customer': ['c_custkey', 'c_mktsegment'],
            'orders': ['o_orderkey', 'o_custkey', 'o_orderdate', 'o_shippriority'],
            'lineitem': ['l_orderkey', 'l_shipdate', 'l_extendedprice', 'l_discount'],
        },
        olm_columns={
            'customer': ['c_custkey', 'c_mktsegment'],
            'orders': ['o_orderkey', 'o_custkey', 'o_orderdate'],
            'lineitem': ['l_orderkey', 'l_shipdate'],
        },
        filter_selectivity={
            'customer': 0.20,   # c_mktsegment = 'BUILDING'
            'orders': 0.48,     # o_orderdate < '1995-03-15'
        },
        join_selectivity={
            # After filter AND join with customer: ~20% of orders match building customers
            'orders': 0.20,     # orders that join with filtered customers
        },
        build_tables=['customer', 'orders'],
        probe_table='lineitem',
        stream_chunk_rows=10_000_000,  # 10M rows per chunk
    )
    queries.append(q3)

    # =========================================================================
    # Q5: Local Supplier Volume (6-way join)
    # =========================================================================
    # Execution:
    #   1. Filter region (r_name='ASIA') → 20% (1 of 5)
    #   2. Build nation HT (n_regionkey in region) → 20% (5 of 25)
    #   3. Build supplier HT (s_nationkey in nations) → 20%
    #   4. Build customer HT (c_nationkey in nations) → 20%
    #   5. Filter orders (date in 1994) AND probe customer → ~3.2%
    #   6. Stream lineitem, probe orders + supplier

    q5 = QueryMemoryAnalysis(
        query_name="Q5",
        description="Local Supplier Volume (6-way join)",
        tables=['customer', 'orders', 'lineitem', 'supplier', 'nation', 'region'],
        full_columns={
            'customer': ['c_custkey', 'c_nationkey'],
            'orders': ['o_orderkey', 'o_custkey', 'o_orderdate'],
            'lineitem': ['l_orderkey', 'l_suppkey', 'l_extendedprice', 'l_discount'],
            'supplier': ['s_suppkey', 's_nationkey'],
            'nation': ['n_nationkey', 'n_name', 'n_regionkey'],
            'region': ['r_regionkey', 'r_name'],
        },
        olm_columns={
            'customer': ['c_custkey', 'c_nationkey'],
            'orders': ['o_orderkey', 'o_custkey', 'o_orderdate'],
            'lineitem': ['l_orderkey', 'l_suppkey'],
            'supplier': ['s_suppkey', 's_nationkey'],
            'nation': ['n_nationkey', 'n_regionkey'],
            'region': ['r_regionkey'],
        },
        filter_selectivity={
            'region': 0.20,     # r_name = 'ASIA'
            'nation': 0.20,     # nations in ASIA region
            'supplier': 0.20,   # suppliers in ASIA nations
            'customer': 0.20,   # customers in ASIA nations
            'orders': 0.16,     # o_orderdate in 1994
        },
        join_selectivity={
            'orders': 0.20,     # orders from ASIA customers
        },
        build_tables=['region', 'nation', 'supplier', 'customer', 'orders'],
        probe_table='lineitem',
        stream_chunk_rows=10_000_000,
    )
    queries.append(q5)

    # =========================================================================
    # Q10: Returned Item Reporting (4-way join)
    # =========================================================================
    # Execution:
    #   1. Build nation HT (all nations, small)
    #   2. Build customer HT (all customers that join with nation)
    #   3. Filter orders (date in Q4 1993) AND probe customer → ~4%
    #   4. Stream lineitem, filter (returnflag='R'), probe orders

    q10 = QueryMemoryAnalysis(
        query_name="Q10",
        description="Returned Item Reporting (C ⋈ O ⋈ L ⋈ N)",
        tables=['customer', 'orders', 'lineitem', 'nation'],
        full_columns={
            'customer': ['c_custkey', 'c_name', 'c_acctbal', 'c_address', 'c_phone',
                        'c_comment', 'c_nationkey'],
            'orders': ['o_orderkey', 'o_custkey', 'o_orderdate'],
            'lineitem': ['l_orderkey', 'l_returnflag', 'l_extendedprice', 'l_discount'],
            'nation': ['n_nationkey', 'n_name'],
        },
        olm_columns={
            'customer': ['c_custkey', 'c_nationkey'],
            'orders': ['o_orderkey', 'o_custkey', 'o_orderdate'],
            'lineitem': ['l_orderkey', 'l_returnflag'],
            'nation': ['n_nationkey'],
        },
        filter_selectivity={
            'nation': 1.0,      # All nations (no filter)
            'customer': 1.0,    # All customers (no filter)
            'orders': 0.04,     # 3 months = ~4% of date range
        },
        join_selectivity={
            'orders': 1.0,      # All filtered orders join (no customer filter)
        },
        build_tables=['nation', 'customer', 'orders'],
        probe_table='lineitem',
        stream_chunk_rows=10_000_000,
    )
    queries.append(q10)

    # =========================================================================
    # Q18: Large Volume Customer
    # =========================================================================
    # Execution (with subquery optimization):
    #   1. First pass: aggregate lineitem by orderkey, find orders with qty > 300
    #      Result: ~1% of orders qualify (stored in hash set)
    #   2. Build customer HT (all customers)
    #   3. Filter orders (in qualifying set) AND probe customer → ~1%
    #   4. Stream lineitem, probe orders

    q18 = QueryMemoryAnalysis(
        query_name="Q18",
        description="Large Volume Customer (C ⋈ O ⋈ L)",
        tables=['customer', 'orders', 'lineitem'],
        full_columns={
            'customer': ['c_custkey', 'c_name'],
            'orders': ['o_orderkey', 'o_custkey', 'o_orderdate', 'o_totalprice'],
            'lineitem': ['l_orderkey', 'l_quantity'],
        },
        olm_columns={
            'customer': ['c_custkey'],
            'orders': ['o_orderkey', 'o_custkey'],
            'lineitem': ['l_orderkey', 'l_quantity'],
        },
        filter_selectivity={
            'customer': 1.0,    # All customers
            'orders': 0.01,     # ~1% of orders have high quantity
        },
        join_selectivity={
            'orders': 1.0,      # Filtered orders all join with some customer
        },
        build_tables=['customer', 'orders'],
        probe_table='lineitem',
        stream_chunk_rows=10_000_000,
    )
    queries.append(q18)

    return queries


def analyze_query_memory(query: QueryMemoryAnalysis, sf: int) -> Dict:
    """Analyze memory requirements for a query at given scale factor."""

    # =========================================================================
    # Raw Memory: Full tables loaded to GPU
    # =========================================================================
    raw_table_memory = 0
    raw_breakdown = {}
    for table in query.tables:
        size = compute_full_table_size(table, sf)
        raw_table_memory += size
        raw_breakdown[table] = size

    # =========================================================================
    # OLM Memory with Streaming + Selectivity
    # =========================================================================
    # Key insight:
    #   1. Only BUILD-side tables need hash tables in memory
    #   2. Hash tables contain only FILTERED rows (after filter + join predicates)
    #   3. PROBE-side table is STREAMED in chunks (not fully loaded)
    #   4. Hash entry = key(8B) + row_id(4B) + pad(4B) = 16 bytes
    #   5. No need to store filter columns in hash table - just join key

    olm_breakdown = {}

    # Calculate hash table sizes with selectivity
    olm_hash_memory = 0
    for table in query.build_tables:
        base_rows = get_row_count(table, sf)

        # Apply filter selectivity
        filter_sel = query.filter_selectivity.get(table, 1.0)
        rows_after_filter = base_rows * filter_sel

        # Apply join selectivity (rows that survive join with previous tables)
        join_sel = query.join_selectivity.get(table, 1.0)
        rows_in_ht = rows_after_filter * join_sel

        # Hash table: capacity = rows * load_factor_inverse, entry = 16 bytes
        ht_capacity = int(rows_in_ht * query.hash_table_factor)
        ht_size = ht_capacity * 16  # 16 bytes per HashEntry

        olm_hash_memory += ht_size
        olm_breakdown[f"{table}_ht ({rows_in_ht/1e6:.1f}M rows)"] = ht_size

    # Streaming chunk for probe table (NOT full table in memory!)
    stream_chunk_memory = 0
    if query.probe_table:
        num_keys = len(query.olm_columns.get(query.probe_table, []))
        tuple_size = compute_compact_tuple_size(num_keys)
        stream_chunk_memory = query.stream_chunk_rows * tuple_size
        olm_breakdown[f"{query.probe_table}_stream_chunk ({query.stream_chunk_rows/1e6:.0f}M rows)"] = stream_chunk_memory

    # Output buffer: estimated result size
    # For join queries, result is typically 1-10% of lineitem rows
    # Conservative estimate: 10% of probe table filtered
    if query.probe_table and query.probe_table == 'lineitem':
        probe_rows = get_row_count(query.probe_table, sf)
        # Estimate result size based on combined selectivity
        combined_sel = 1.0
        for sel in query.filter_selectivity.values():
            combined_sel *= sel
        estimated_results = int(probe_rows * combined_sel * 0.5)  # 50% join match rate
        output_buffer = estimated_results * 8  # 8 bytes per result pair
    else:
        output_buffer = 100_000_000 * 8  # Conservative 100M results

    # Cap output buffer at reasonable size
    output_buffer = min(output_buffer, 8 * 1024**3)  # Max 8 GB
    olm_breakdown[f"output_buffer (est. {output_buffer / (8 * 1e6):.1f}M results)"] = output_buffer

    olm_total = olm_hash_memory + stream_chunk_memory + output_buffer

    # =========================================================================
    # Calculate savings
    # =========================================================================
    savings_bytes = raw_table_memory - olm_total
    savings_pct = (savings_bytes / raw_table_memory) * 100 if raw_table_memory > 0 else 0

    return {
        'query': query.query_name,
        'description': query.description,
        'scale_factor': sf,
        'raw_memory_bytes': raw_table_memory,
        'raw_memory_gb': raw_table_memory / (1024**3),
        'olm_hash_bytes': olm_hash_memory,
        'olm_stream_bytes': stream_chunk_memory,
        'olm_output_bytes': output_buffer,
        'olm_total_bytes': olm_total,
        'olm_total_gb': olm_total / (1024**3),
        'savings_bytes': savings_bytes,
        'savings_gb': savings_bytes / (1024**3),
        'savings_pct': savings_pct,
        'raw_breakdown': raw_breakdown,
        'olm_breakdown': olm_breakdown,
    }


def print_analysis(results: List[Dict], sf: int):
    """Print detailed memory analysis."""

    print("=" * 80)
    print(f"GPU_ObliDB: TPC-H Memory Savings with OLM (SF{sf})")
    print("=" * 80)
    print()
    print("OLM Approach:")
    print("  • Build-side: Only FILTERED rows stored in hash tables (key + row_id = 16B)")
    print("  • Probe-side: STREAMED in chunks (not fully loaded)")
    print("  • Filter columns discarded after filtering")
    print()

    # TPC-H SF row counts
    print("TPC-H Table Sizes at SF{}:".format(sf))
    print("-" * 50)
    for table, base_count in SF1_ROW_COUNTS.items():
        count = get_row_count(table, sf)
        size_gb = compute_full_table_size(table, sf) / (1024**3)
        print(f"  {table:12s}: {count:>15,} rows  ({size_gb:>8.2f} GB)")
    print()

    # Summary table
    print("Memory Comparison Summary:")
    print("-" * 90)
    print(f"{'Query':<8} {'Description':<40} {'Raw (GB)':<12} {'OLM (GB)':<12} {'Savings':<10}")
    print("-" * 90)

    for r in results:
        print(f"{r['query']:<8} {r['description']:<40} {r['raw_memory_gb']:>10.2f}  "
              f"{r['olm_total_gb']:>10.2f}  {r['savings_pct']:>7.1f}%")
    print("-" * 90)
    print()

    # Detailed breakdown
    for r in results:
        print(f"\n{r['query']}: {r['description']}")
        print("=" * 70)

        print("\nRaw Memory (Full Tables to GPU):")
        for table, size in r['raw_breakdown'].items():
            size_gb = size / (1024**3)
            print(f"  {table:25s}: {size_gb:>10.2f} GB")
        print(f"  {'─' * 25}  {'─' * 10}")
        print(f"  {'TOTAL':25s}: {r['raw_memory_gb']:>10.2f} GB")

        print("\nOLM Working Memory (Hash Tables + Stream Chunk + Output):")
        for item, size in r['olm_breakdown'].items():
            size_gb = size / (1024**3)
            size_mb = size / (1024**2)
            if size_gb >= 1.0:
                print(f"  {item:45s}: {size_gb:>10.2f} GB")
            else:
                print(f"  {item:45s}: {size_mb:>10.2f} MB")
        print(f"  {'─' * 45}  {'─' * 10}")
        print(f"  {'TOTAL':45s}: {r['olm_total_gb']:>10.2f} GB")

        print(f"\n  ✓ Memory Savings: {r['savings_gb']:.2f} GB ({r['savings_pct']:.1f}%)")


def plot_memory_comparison(results: List[Dict], sf: int, output_path: str = None):
    """Generate elegant bar chart comparing Raw vs SJ w/ LM memory with log scale."""
    if not HAS_MATPLOTLIB:
        print("Warning: matplotlib not available, skipping plot")
        return

    palette = sns.color_palette("crest", 2)
    color_raw = adjust_color(palette[0])
    color_olm = palette[1]

    queries = [r['query'] for r in results]
    raw_memory = [r['raw_memory_gb'] for r in results]
    olm_memory = [r['olm_total_gb'] for r in results]
    savings_pct = [r['savings_pct'] for r in results]

    sns.set_theme(style="whitegrid")
    fig, ax = plt.subplots(figsize=(6, 3), facecolor='white')
    ax.set_facecolor('white')

    x = np.arange(len(queries))
    width = 0.35

    bars1 = ax.bar(x - width/2, raw_memory, width,
                   label='Full Table Size',
                   color=color_raw, alpha=0.85, linewidth=1.5)
    bars2 = ax.bar(x + width/2, olm_memory, width,
                   label='Working Memory Required by SJ',
                   color=color_olm, alpha=0.85, linewidth=1.5)

    ax.set_yscale('log')

    max_val = max(raw_memory)
    ax.set_ylim(top=max_val * 10)

    ax.set_xlabel('TPC-H Query', fontsize=12)
    ax.set_ylabel('GPU Memory (GB)', fontsize=12)
    ax.set_xticks(x)
    ax.set_xticklabels(queries)

    legend = ax.legend(loc='upper center', fontsize=9, framealpha=1.0,
                       fancybox=True, shadow=True, ncol=2)
    legend.get_frame().set_facecolor('white')

    ax.grid(axis='y', linestyle='--', alpha=0.5)
    ax.grid(axis='x', visible=False)

    for bar, val in zip(bars1, raw_memory):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                f'{val:.0f}',
                ha='center', va='bottom', fontsize=8, fontweight='bold',
                color='black')

    for bar, val, pct in zip(bars2, olm_memory, savings_pct):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                f'({val:.1f}\n-{pct:.0f}%)',
                ha='center', va='bottom', fontsize=8, fontweight='bold',
                color='black')

    plt.tight_layout()

    path = output_path or os.path.join(FIGURE_DIR, f'memory_savings_streaming_sf{sf}.pdf')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    plt.savefig(path, dpi=200, bbox_inches='tight', facecolor='white')
    print(f"\nPlot saved to: {path}")
    plt.close()


def plot_detailed_breakdown(results: List[Dict], sf: int, output_path: str = None):
    """Generate stacked bar chart showing memory breakdown."""
    if not HAS_MATPLOTLIB:
        print("Warning: matplotlib not available, skipping plot")
        return

    fig, ax = plt.subplots(figsize=(14, 7))

    queries = [r['query'] for r in results]
    x = np.arange(len(queries))
    width = 0.4

    # Extract OLM components (new structure: hash + stream + output)
    hash_mem = [r['olm_hash_bytes'] / (1024**3) for r in results]
    stream_mem = [r['olm_stream_bytes'] / (1024**3) for r in results]
    output_mem = [r['olm_output_bytes'] / (1024**3) for r in results]
    raw_mem = [r['raw_memory_gb'] for r in results]

    # Raw memory bars
    bars_raw = ax.bar(x - width/2, raw_mem, width, label='Raw (Full Tables)',
                      color='#E74C3C', edgecolor='black', linewidth=1)

    # OLM stacked bars
    bars_hash = ax.bar(x + width/2, hash_mem, width, label='OLM: Hash Tables (filtered)',
                       color='#27AE60', edgecolor='black', linewidth=1)
    bars_stream = ax.bar(x + width/2, stream_mem, width, bottom=hash_mem,
                         label='OLM: Stream Chunk', color='#3498DB', edgecolor='black', linewidth=1)

    bottom_for_output = [h + s for h, s in zip(hash_mem, stream_mem)]
    bars_output = ax.bar(x + width/2, output_mem, width, bottom=bottom_for_output,
                         label='OLM: Output Buffer', color='#F39C12', edgecolor='black', linewidth=1)

    ax.set_xlabel('TPC-H Query', fontsize=12, fontweight='bold')
    ax.set_ylabel('GPU Memory (GB)', fontsize=12, fontweight='bold')
    ax.set_title(f'GPU Memory: Raw vs OLM Working Memory\n(TPC-H SF{sf})',
                 fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels([f"{q}\n{results[i]['description'][:30]}"
                        for i, q in enumerate(queries)], fontsize=9)
    ax.legend(loc='upper left', fontsize=9)
    ax.grid(axis='y', linestyle='--', alpha=0.7)

    # Add value labels
    for i, (raw, olm) in enumerate(zip(raw_mem, [h + s + o for h, s, o in zip(hash_mem, stream_mem, output_mem)])):
        ax.annotate(f'{raw:.0f} GB', xy=(x[i] - width/2, raw), xytext=(0, 3),
                   textcoords="offset points", ha='center', va='bottom', fontsize=8, fontweight='bold')
        ax.annotate(f'{olm:.1f} GB', xy=(x[i] + width/2, olm), xytext=(0, 3),
                   textcoords="offset points", ha='center', va='bottom', fontsize=8, fontweight='bold', color='green')

    plt.tight_layout()

    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        print(f"Plot saved to: {output_path}")
    else:
        plt.savefig(f'memory_breakdown_sf{sf}.png', dpi=150, bbox_inches='tight')
        print(f"Plot saved to: memory_breakdown_sf{sf}.png")

    plt.show()


def main():
    parser = argparse.ArgumentParser(
        description='Analyze TPC-H query memory savings with OLM (Oblivious Late Materialization)'
    )
    parser.add_argument('--sf', type=int, default=1000,
                       help='TPC-H Scale Factor (default: 1000)')
    parser.add_argument('--output-dir', type=str, default=FIGURE_DIR,
                       help='Output directory for plots')
    parser.add_argument('--no-plot', action='store_true',
                       help='Skip generating plots')

    args = parser.parse_args()
    sf = args.sf

    # Define queries
    queries = define_queries()

    # Analyze each query
    results = []
    for query in queries:
        result = analyze_query_memory(query, sf)
        results.append(result)

    # Print analysis
    print_analysis(results, sf)

    # Generate plot
    if not args.no_plot:
        try:
            os.makedirs(args.output_dir, exist_ok=True)
            plot_memory_comparison(results, sf,
                                   os.path.join(args.output_dir, f'memory_savings_streaming_sf{sf}.pdf'))
        except ImportError:
            print("\nWarning: matplotlib not available, skipping plot")
        except Exception as e:
            print(f"\nWarning: Could not generate plot: {e}")

    # Print summary statistics
    print("\n" + "=" * 80)
    print("SUMMARY")
    print("=" * 80)
    savings_list = [r['savings_pct'] for r in results]
    avg_savings = sum(savings_list) / len(savings_list)
    total_raw = sum([r['raw_memory_gb'] for r in results])
    total_olm = sum([r['olm_total_gb'] for r in results])

    print(f"\nScale Factor: SF{sf}")
    print(f"Queries Analyzed: {len(results)}")
    print(f"\nAverage Memory Savings: {avg_savings:.1f}%")
    print(f"Total Raw Memory (all queries): {total_raw:.2f} GB")
    print(f"Total SJ w/ LM Memory (all queries): {total_olm:.2f} GB")
    print(f"Total Savings: {total_raw - total_olm:.2f} GB")

    # GPU fit analysis
    print("\n" + "-" * 50)
    print("GPU Memory Fit Analysis:")
    print("-" * 50)
    gpu_configs = [
        ("RTX 4090", 24),
        ("A100 (40GB)", 40),
        ("A100 (80GB)", 80),
        ("H100", 80),
        ("H200", 141),
    ]

    for r in results:
        print(f"\n{r['query']} ({r['description']}):")
        for gpu_name, gpu_mem in gpu_configs:
            raw_fits = "✓" if r['raw_memory_gb'] <= gpu_mem else "✗"
            olm_fits = "✓" if r['olm_total_gb'] <= gpu_mem else "✗"
            print(f"  {gpu_name:15s} ({gpu_mem:>3d} GB): Raw {raw_fits}  SJ w/ LM {olm_fits}")


if __name__ == "__main__":
    main()
