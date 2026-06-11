"""
TPC-H Memory Savings Analysis (Materialized Version)

This version assumes ALL filtered tables are FULLY MATERIALIZED in GPU memory.
No streaming - all build AND probe tables loaded after filter.

Comparison:
  - Raw: Full tables with all columns
  - OLM Materialized: Filtered tables with compact structure (join keys only)

Usage:
    python memory_savings_materialized.py [--sf SCALE_FACTOR]
"""

import argparse
import colorsys
import os
from dataclasses import dataclass
from typing import Dict, List

# Optional imports for plotting
try:
    import matplotlib
    matplotlib.use('Agg')
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
# TPC-H Column Sizes (bytes)
# =============================================================================

COLUMN_SIZES = {
    # LINEITEM (16 columns)
    'l_orderkey': 8, 'l_partkey': 8, 'l_suppkey': 8, 'l_linenumber': 4,
    'l_quantity': 8, 'l_extendedprice': 8, 'l_discount': 8, 'l_tax': 8,
    'l_returnflag': 1, 'l_linestatus': 1, 'l_shipdate': 4, 'l_commitdate': 4,
    'l_receiptdate': 4, 'l_shipinstruct': 25, 'l_shipmode': 10, 'l_comment': 44,

    # ORDERS (9 columns)
    'o_orderkey': 8, 'o_custkey': 8, 'o_orderstatus': 1, 'o_totalprice': 8,
    'o_orderdate': 4, 'o_orderpriority': 15, 'o_clerk': 15, 'o_shippriority': 4,
    'o_comment': 79,

    # CUSTOMER (8 columns)
    'c_custkey': 8, 'c_name': 25, 'c_address': 40, 'c_nationkey': 4,
    'c_phone': 15, 'c_acctbal': 8, 'c_mktsegment': 10, 'c_comment': 117,

    # SUPPLIER (7 columns)
    's_suppkey': 8, 's_name': 25, 's_address': 40, 's_nationkey': 4,
    's_phone': 15, 's_acctbal': 8, 's_comment': 101,

    # NATION (4 columns)
    'n_nationkey': 4, 'n_name': 25, 'n_regionkey': 4, 'n_comment': 152,

    # REGION (3 columns)
    'r_regionkey': 4, 'r_name': 25, 'r_comment': 152,
}

# Full row sizes
TABLE_ROW_SIZES = {
    'lineitem': 155,   # Sum of all lineitem columns
    'orders': 142,     # Sum of all orders columns
    'customer': 227,   # Sum of all customer columns
    'supplier': 201,   # Sum of all supplier columns
    'nation': 185,
    'region': 181,
}

# SF1 row counts
SF1_ROW_COUNTS = {
    'lineitem': 6_000_000,
    'orders': 1_500_000,
    'customer': 150_000,
    'supplier': 10_000,
    'nation': 25,
    'region': 5,
}

# =============================================================================
# TPC-H SELECTIVITY ESTIMATES - WITH JUSTIFICATION
# =============================================================================
#
# These selectivities are based on TPC-H specification and well-known benchmarks.
# References: TPC-H Spec v3.0, academic papers analyzing TPC-H.
#
# KEY TPC-H DATA CHARACTERISTICS:
# ─────────────────────────────────────────────────────────────────────────────
#
# DATE RANGE:
#   - ORDERDATE spans 7 years: 1992-01-01 to 1998-08-02 (~2406 days)
#   - SHIPDATE is typically ORDERDATE + 1 to 121 days
#
# CATEGORICAL DISTRIBUTIONS:
#   - c_mktsegment: 5 values (AUTOMOBILE, BUILDING, FURNITURE, MACHINERY, HOUSEHOLD)
#     → Each segment has ~20% of customers (uniform distribution)
#
#   - l_returnflag: 3 values ('R'=returned, 'A'=accepted, 'N'=none)
#     → 'R' is approximately 25% of lineitems (returned items)
#
#   - r_name: 5 regions (AFRICA, AMERICA, ASIA, EUROPE, MIDDLE EAST)
#     → Each region has exactly 20% (5 nations per region out of 25)
#
#   - n_nationkey: 25 nations, 5 per region
#     → Suppliers/customers uniformly distributed across nations
#
# =============================================================================

SELECTIVITY_JUSTIFICATION = """
┌─────────────────────────────────────────────────────────────────────────────┐
│                    TPC-H SELECTIVITY ESTIMATES JUSTIFICATION                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ Q3: c_mktsegment = 'BUILDING'                                               │
│     → 1 of 5 segments, uniform distribution                                 │
│     → Selectivity: 20% (0.20)                                               │
│                                                                             │
│ Q3: o_orderdate < '1995-03-15'                                              │
│     → Date range: 1992-01-01 to 1998-08-02 (2406 days)                      │
│     → Days before 1995-03-15: ~1169 days (from 1992-01-01)                  │
│     → Selectivity: 1169/2406 ≈ 48.6% (0.48)                                 │
│                                                                             │
│ Q3: l_shipdate > '1995-03-15'                                               │
│     → Inverse of above, considering shipdate offset                         │
│     → Selectivity: ~52% (0.52)                                              │
│                                                                             │
│ Q5: r_name = 'ASIA'                                                         │
│     → 1 of 5 regions                                                        │
│     → Selectivity: 20% (0.20)                                               │
│                                                                             │
│ Q5: Nations in ASIA region                                                  │
│     → 5 of 25 nations belong to ASIA                                        │
│     → Selectivity: 20% (0.20)                                               │
│                                                                             │
│ Q5: o_orderdate BETWEEN '1994-01-01' AND '1994-12-31'                       │
│     → 1 year out of ~6.5 year range                                         │
│     → Selectivity: 365/2406 ≈ 15.2% (0.16)                                  │
│                                                                             │
│ Q10: o_orderdate BETWEEN '1993-10-01' AND '1993-12-31'                      │
│     → 3 months (Q4 1993) out of ~6.5 years                                  │
│     → Selectivity: 92/2406 ≈ 3.8% (0.04)                                    │
│                                                                             │
│ Q10: l_returnflag = 'R'                                                     │
│     → Returned items are ~25% of all lineitems                              │
│     → Selectivity: 25% (0.25)                                               │
│                                                                             │
│ Q18: SUM(l_quantity) > 300 per order                                        │
│     → Large orders are rare (~1% based on quantity distribution)            │
│     → Avg lines per order: 4, avg quantity per line: 25                     │
│     → Selectivity: ~1% (0.01)                                               │
│                                                                             │
│ JOIN SELECTIVITY (rows surviving join with previous filtered tables):       │
│     → Orders joining with 'BUILDING' customers: ~20%                        │
│     → This is because customers are uniformly distributed                   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
"""

@dataclass
class QueryMemoryAnalysis:
    """Memory analysis for a TPC-H query."""
    query_name: str
    description: str
    tables: List[str]

    # Full columns needed for traditional execution
    full_columns: Dict[str, List[str]]

    # OLM compact columns (join keys + filter columns)
    olm_columns: Dict[str, List[str]]

    # Filter selectivity for each table (independent of joins)
    filter_selectivity: Dict[str, float]

    # Join selectivity (fraction surviving join with previously filtered tables)
    join_selectivity: Dict[str, float]

    # Tables needing hash tables
    build_tables: List[str]

    # Hash table load factor inverse (capacity = rows * factor)
    hash_table_factor: float = 2.0


def get_row_count(table: str, sf: int) -> int:
    """Get row count at scale factor."""
    if table in ['nation', 'region']:
        return SF1_ROW_COUNTS[table]
    return SF1_ROW_COUNTS[table] * sf


def compute_full_table_size(table: str, sf: int) -> int:
    """Full table size with all columns."""
    return get_row_count(table, sf) * TABLE_ROW_SIZES[table]


def compute_compact_tuple_size(num_keys: int) -> int:
    """CompactTuple: record_id (4B) + padding (4B) + keys (8B each)."""
    return 8 + 8 * num_keys


def define_queries() -> List[QueryMemoryAnalysis]:
    """Define queries with selectivity estimates."""

    queries = []

    # =========================================================================
    # Q3: Shipping Priority
    # =========================================================================
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
            'customer': ['c_custkey', 'c_mktsegment'],  # 2 keys
            'orders': ['o_orderkey', 'o_custkey', 'o_orderdate'],  # 3 keys
            'lineitem': ['l_orderkey', 'l_shipdate'],  # 2 keys
        },
        filter_selectivity={
            # c_mktsegment = 'BUILDING': 1/5 segments
            'customer': 0.20,
            # o_orderdate < '1995-03-15': ~48% of date range
            'orders': 0.48,
            # l_shipdate > '1995-03-15': ~52% of lineitems
            'lineitem': 0.52,
        },
        join_selectivity={
            # Orders that join with BUILDING customers: 20%
            'orders': 0.20,
            # Lineitems that join with filtered orders: ~10% (0.48 * 0.20)
            'lineitem': 0.10,
        },
        build_tables=['customer', 'orders'],
    )
    queries.append(q3)

    # =========================================================================
    # Q5: Local Supplier Volume
    # =========================================================================
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
            'customer': ['c_custkey', 'c_nationkey'],  # 2 keys
            'orders': ['o_orderkey', 'o_custkey', 'o_orderdate'],  # 3 keys
            'lineitem': ['l_orderkey', 'l_suppkey'],  # 2 keys
            'supplier': ['s_suppkey', 's_nationkey'],  # 2 keys
            'nation': ['n_nationkey', 'n_regionkey'],  # 2 keys
            'region': ['r_regionkey'],  # 1 key
        },
        filter_selectivity={
            # r_name = 'ASIA': 1/5 regions
            'region': 0.20,
            # Nations in ASIA: 5/25 nations
            'nation': 0.20,
            # Suppliers in ASIA nations: 20%
            'supplier': 0.20,
            # Customers in ASIA nations: 20%
            'customer': 0.20,
            # o_orderdate in 1994: 1 year / 6.5 years
            'orders': 0.16,
            # All lineitems considered (filtered by join)
            'lineitem': 1.0,
        },
        join_selectivity={
            # Orders from ASIA customers with 1994 date
            'orders': 0.20,
            # Lineitems from filtered orders + ASIA suppliers
            # ~3.2% (0.16 * 0.20) of orders, then ~20% have ASIA supplier
            'lineitem': 0.032 * 0.20,
        },
        build_tables=['region', 'nation', 'supplier', 'customer', 'orders'],
    )
    queries.append(q5)

    # =========================================================================
    # Q10: Returned Item Reporting
    # =========================================================================
    q10 = QueryMemoryAnalysis(
        query_name="Q10",
        description="Returned Item Reporting (C ⋈ O ⋈ L ⋈ N)",
        tables=['customer', 'orders', 'lineitem', 'nation'],
        full_columns={
            'customer': ['c_custkey', 'c_name', 'c_acctbal', 'c_address',
                        'c_phone', 'c_comment', 'c_nationkey'],
            'orders': ['o_orderkey', 'o_custkey', 'o_orderdate'],
            'lineitem': ['l_orderkey', 'l_returnflag', 'l_extendedprice', 'l_discount'],
            'nation': ['n_nationkey', 'n_name'],
        },
        olm_columns={
            'customer': ['c_custkey', 'c_nationkey'],  # 2 keys
            'orders': ['o_orderkey', 'o_custkey', 'o_orderdate'],  # 3 keys
            'lineitem': ['l_orderkey', 'l_returnflag'],  # 2 keys
            'nation': ['n_nationkey'],  # 1 key
        },
        filter_selectivity={
            # All nations
            'nation': 1.0,
            # All customers
            'customer': 1.0,
            # o_orderdate in Q4 1993: 3 months / 78 months
            'orders': 0.04,
            # l_returnflag = 'R': ~25%
            'lineitem': 0.25,
        },
        join_selectivity={
            # Orders in date range
            'orders': 1.0,
            # Lineitems with returnflag='R' that join with filtered orders
            # 4% of orders * 25% returnflag
            'lineitem': 0.04,
        },
        build_tables=['nation', 'customer', 'orders'],
    )
    queries.append(q10)

    # =========================================================================
    # Q18: Large Volume Customer
    # =========================================================================
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
            'customer': ['c_custkey'],  # 1 key
            'orders': ['o_orderkey', 'o_custkey'],  # 2 keys
            'lineitem': ['l_orderkey', 'l_quantity'],  # 2 keys
        },
        filter_selectivity={
            # All customers
            'customer': 1.0,
            # Orders with SUM(qty) > 300: ~1% (large orders are rare)
            'orders': 0.01,
            # All lineitems (quantity aggregated)
            'lineitem': 1.0,
        },
        join_selectivity={
            # High-quantity orders
            'orders': 1.0,
            # Lineitems for high-quantity orders: 1%
            'lineitem': 0.01,
        },
        build_tables=['customer', 'orders'],
    )
    queries.append(q18)

    return queries


def analyze_query_memory(query: QueryMemoryAnalysis, sf: int) -> Dict:
    """
    Analyze memory with FULL MATERIALIZATION (no streaming).

    All filtered tables are loaded into GPU memory completely.
    """

    # =========================================================================
    # Raw Memory: Full tables with all columns
    # =========================================================================
    raw_memory = 0
    raw_breakdown = {}
    for table in query.tables:
        size = compute_full_table_size(table, sf)
        raw_memory += size
        raw_breakdown[table] = size

    # =========================================================================
    # OLM Materialized: Filtered tables with compact structure
    # =========================================================================
    olm_breakdown = {}

    # Compact tables (ALL tables materialized after filter)
    olm_compact_memory = 0
    for table, cols in query.olm_columns.items():
        base_rows = get_row_count(table, sf)

        # Apply filter selectivity
        filter_sel = query.filter_selectivity.get(table, 1.0)

        # Apply join selectivity
        join_sel = query.join_selectivity.get(table, 1.0)

        # Final rows after filter + join
        final_rows = int(base_rows * filter_sel * join_sel)

        # Compact tuple size
        num_keys = len(cols)
        tuple_size = compute_compact_tuple_size(num_keys)

        size = final_rows * tuple_size
        olm_compact_memory += size
        olm_breakdown[f"{table} ({final_rows/1e6:.1f}M rows × {tuple_size}B)"] = size

    # Hash tables for build-side tables
    olm_hash_memory = 0
    for table in query.build_tables:
        base_rows = get_row_count(table, sf)
        filter_sel = query.filter_selectivity.get(table, 1.0)
        join_sel = query.join_selectivity.get(table, 1.0)

        rows_in_ht = int(base_rows * filter_sel * join_sel)
        ht_capacity = int(rows_in_ht * query.hash_table_factor)
        ht_size = ht_capacity * 16  # 16 bytes per HashEntry

        olm_hash_memory += ht_size
        olm_breakdown[f"{table}_ht ({rows_in_ht/1e6:.1f}M entries × 16B × 2)"] = ht_size

    # Output buffer (based on final join cardinality)
    # Estimate: smallest filtered table × join selectivity
    if 'lineitem' in query.tables:
        lineitem_rows = get_row_count('lineitem', sf)
        lineitem_filter = query.filter_selectivity.get('lineitem', 1.0)
        lineitem_join = query.join_selectivity.get('lineitem', 1.0)
        result_rows = int(lineitem_rows * lineitem_filter * lineitem_join)
    else:
        result_rows = 1_000_000

    output_buffer = result_rows * 8  # 8 bytes per result pair
    olm_breakdown[f"output ({result_rows/1e6:.1f}M results × 8B)"] = output_buffer

    olm_total = olm_compact_memory + olm_hash_memory + output_buffer

    # Savings
    savings_bytes = raw_memory - olm_total
    savings_pct = (savings_bytes / raw_memory) * 100 if raw_memory > 0 else 0

    return {
        'query': query.query_name,
        'description': query.description,
        'scale_factor': sf,
        'raw_memory_bytes': raw_memory,
        'raw_memory_gb': raw_memory / (1024**3),
        'olm_compact_bytes': olm_compact_memory,
        'olm_hash_bytes': olm_hash_memory,
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
    """Print detailed analysis."""

    print("=" * 90)
    print(f"GPU_ObliDB: TPC-H Memory Savings - MATERIALIZED VERSION (SF{sf})")
    print("=" * 90)
    print()
    print("Mode: ALL filtered tables fully materialized in GPU memory (no streaming)")
    print()
    print(SELECTIVITY_JUSTIFICATION)
    print()

    # Table sizes
    print("TPC-H Full Table Sizes at SF{}:".format(sf))
    print("-" * 60)
    for table, base_count in SF1_ROW_COUNTS.items():
        count = get_row_count(table, sf)
        size_gb = compute_full_table_size(table, sf) / (1024**3)
        print(f"  {table:12s}: {count:>15,} rows  ({size_gb:>8.2f} GB)")
    print()

    # Summary
    print("Memory Comparison Summary:")
    print("-" * 90)
    print(f"{'Query':<8} {'Description':<42} {'Raw (GB)':<12} {'OLM (GB)':<12} {'Savings':<10}")
    print("-" * 90)

    for r in results:
        print(f"{r['query']:<8} {r['description']:<42} {r['raw_memory_gb']:>10.2f}  "
              f"{r['olm_total_gb']:>10.2f}  {r['savings_pct']:>7.1f}%")
    print("-" * 90)
    print()

    # Detailed breakdown
    for r in results:
        print(f"\n{'─' * 90}")
        print(f"{r['query']}: {r['description']}")
        print(f"{'─' * 90}")

        print("\n📦 Raw Memory (Full Tables, All Columns):")
        for table, size in r['raw_breakdown'].items():
            size_gb = size / (1024**3)
            print(f"    {table:15s}: {size_gb:>10.2f} GB")
        print(f"    {'─' * 15}  {'─' * 10}")
        print(f"    {'TOTAL':15s}: {r['raw_memory_gb']:>10.2f} GB")

        print("\n📦 OLM Materialized Memory (Filtered Tables + Hash Tables):")
        for item, size in r['olm_breakdown'].items():
            size_gb = size / (1024**3)
            size_mb = size / (1024**2)
            if size_gb >= 0.1:
                print(f"    {item:50s}: {size_gb:>10.2f} GB")
            else:
                print(f"    {item:50s}: {size_mb:>10.2f} MB")
        print(f"    {'─' * 50}  {'─' * 10}")
        print(f"    {'TOTAL':50s}: {r['olm_total_gb']:>10.2f} GB")

        print(f"\n    ✓ Memory Savings: {r['savings_gb']:.2f} GB ({r['savings_pct']:.1f}%)")


def plot_comparison(results: List[Dict], sf: int, output_path: str = None):
    """Generate elegant bar chart comparing Raw vs SJ w/ LM with log scale."""
    if not HAS_MATPLOTLIB:
        print("Warning: matplotlib not available")
        return

    palette = sns.color_palette("crest", 2)
    color_raw = adjust_color(palette[0])
    color_olm = palette[1]

    queries = [r['query'] for r in results]
    raw_mem = [r['raw_memory_gb'] for r in results]
    olm_mem = [r['olm_total_gb'] for r in results]
    savings = [r['savings_pct'] for r in results]

    sns.set_theme(style="whitegrid")
    fig, ax = plt.subplots(figsize=(6, 3), facecolor='white')
    ax.set_facecolor('white')

    x = np.arange(len(queries))
    width = 0.35

    bars1 = ax.bar(x - width/2, raw_mem, width,
                   label='Full Table Size',
                   color=color_raw, alpha=0.85, linewidth=1.5)
    bars2 = ax.bar(x + width/2, olm_mem, width,
                   label='Working Memory Required by SJ',
                   color=color_olm, alpha=0.85, linewidth=1.5)

    ax.set_yscale('log')

    max_val = max(raw_mem)
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

    for bar, val in zip(bars1, raw_mem):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                f'{val:.0f}',
                ha='center', va='bottom', fontsize=8, fontweight='bold',
                color='black')

    for bar, val, pct in zip(bars2, olm_mem, savings):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                f'{val:.1f}\n(-{pct:.0f}%)',
                ha='center', va='bottom', fontsize=8, fontweight='bold',
                color='black')

    plt.tight_layout()

    path = output_path or os.path.join(FIGURE_DIR, f'memory_savings_materialized_sf{sf}.pdf')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    plt.savefig(path, dpi=200, bbox_inches='tight', facecolor='white')
    print(f"\nPlot saved to: {path}")
    plt.close()


def main():
    parser = argparse.ArgumentParser(
        description='TPC-H Memory Savings Analysis (Materialized Version)'
    )
    parser.add_argument('--sf', type=int, default=1000)
    parser.add_argument('--no-plot', action='store_true')

    args = parser.parse_args()
    sf = args.sf

    queries = define_queries()
    results = [analyze_query_memory(q, sf) for q in queries]

    print_analysis(results, sf)

    if not args.no_plot:
        plot_comparison(results, sf)

    # Summary
    print("\n" + "=" * 90)
    print("SUMMARY")
    print("=" * 90)

    avg_savings = sum(r['savings_pct'] for r in results) / len(results)
    total_raw = sum(r['raw_memory_gb'] for r in results)
    total_olm = sum(r['olm_total_gb'] for r in results)

    print(f"\nScale Factor: SF{sf}")
    print(f"Mode: Full Materialization (no streaming)")
    print(f"\nAverage Memory Savings: {avg_savings:.1f}%")
    print(f"Total Raw Memory: {total_raw:.2f} GB")
    print(f"Total OLM Memory: {total_olm:.2f} GB")

    # GPU fit
    print("\n" + "-" * 60)
    print("GPU Memory Fit Analysis:")
    print("-" * 60)
    gpus = [("RTX 4090", 24), ("A100-40GB", 40), ("A100-80GB", 80),
            ("H100", 80), ("H200", 141)]

    for r in results:
        print(f"\n{r['query']} ({r['olm_total_gb']:.1f} GB):")
        for gpu, mem in gpus:
            fits = "✓" if r['olm_total_gb'] <= mem else "✗"
            print(f"  {gpu:12s} ({mem:>3d} GB): {fits}")


if __name__ == "__main__":
    main()
