"""
TPC-H Row Reduction Analysis

Compares input table sizes (total rows) vs. output rows after filter + join
(before aggregation). This shows the data reduction achieved by the
selection-join sub-plan.

Usage:
    python row_reduction_analysis.py --sf 1000
"""

import argparse
import colorsys
import os
from dataclasses import dataclass, field
from typing import Dict, List

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import numpy as np
    import seaborn as sns
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
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
# TPC-H Table Row Counts (base SF1)
# =============================================================================
BASE_ROW_COUNTS = {
    'lineitem': 6_000_000,      # ~6M per SF
    'orders': 1_500_000,        # 1.5M per SF
    'customer': 150_000,        # 150K per SF
    'supplier': 10_000,         # 10K per SF
    'part': 200_000,            # 200K per SF
    'partsupp': 800_000,        # 800K per SF
    'nation': 25,               # Fixed
    'region': 5,                # Fixed
}


def get_row_count(table: str, sf: int) -> int:
    """Get row count for a table at given scale factor."""
    base = BASE_ROW_COUNTS.get(table, 0)
    if table in ['nation', 'region']:
        return base  # Fixed size tables
    return base * sf


@dataclass
class QueryRowAnalysis:
    """Defines a TPC-H query for row reduction analysis."""
    query_name: str
    description: str
    tables: List[str]

    # Filter selectivity for each table (rows surviving filter / total rows)
    filter_selectivity: Dict[str, float] = field(default_factory=dict)

    # Join selectivity: fraction of rows surviving join with previous tables
    # This is the combined effect of the join chain
    join_selectivity: Dict[str, float] = field(default_factory=dict)

    # Final output selectivity: what fraction of probe table rows produce output
    # This accounts for all filters and joins combined
    output_selectivity: float = 0.01


def define_queries() -> List[QueryRowAnalysis]:
    """Define TPC-H queries with selectivity estimates."""

    queries = []

    # =========================================================================
    # Q3: Shipping Priority
    # SELECT l_orderkey, SUM(l_extendedprice * (1 - l_discount)) as revenue,
    #        o_orderdate, o_shippriority
    # FROM customer, orders, lineitem
    # WHERE c_mktsegment = 'BUILDING'
    #   AND c_custkey = o_custkey
    #   AND l_orderkey = o_orderkey
    #   AND o_orderdate < '1995-03-15'
    #   AND l_shipdate > '1995-03-15'
    # GROUP BY l_orderkey, o_orderdate, o_shippriority
    # =========================================================================
    q3 = QueryRowAnalysis(
        query_name='Q3',
        description='Shipping Priority',
        tables=['customer', 'orders', 'lineitem'],
        filter_selectivity={
            'customer': 0.20,    # c_mktsegment = 'BUILDING' (1 of 5)
            'orders': 0.48,      # o_orderdate < '1995-03-15'
            'lineitem': 0.52,    # l_shipdate > '1995-03-15'
        },
        join_selectivity={
            'customer': 1.0,
            'orders': 0.20,      # 20% of orders join with BUILDING customers
            'lineitem': 1.0,     # lineitems that match filtered orders
        },
        # Final: ~5% of lineitem rows produce output after all filters + joins
        output_selectivity=0.20 * 0.48 * 0.52 * 1.0,  # ~5%
    )
    queries.append(q3)

    # =========================================================================
    # Q5: Local Supplier Volume
    # SELECT n_name, SUM(l_extendedprice * (1 - l_discount)) as revenue
    # FROM customer, orders, lineitem, supplier, nation, region
    # WHERE c_custkey = o_custkey
    #   AND l_orderkey = o_orderkey
    #   AND l_suppkey = s_suppkey
    #   AND c_nationkey = s_nationkey
    #   AND s_nationkey = n_nationkey
    #   AND n_regionkey = r_regionkey
    #   AND r_name = 'ASIA'
    #   AND o_orderdate >= '1994-01-01'
    #   AND o_orderdate < '1995-01-01'
    # GROUP BY n_name
    # =========================================================================
    q5 = QueryRowAnalysis(
        query_name='Q5',
        description='Local Supplier Volume',
        tables=['region', 'nation', 'supplier', 'customer', 'orders', 'lineitem'],
        filter_selectivity={
            'region': 0.20,      # r_name = 'ASIA' (1 of 5)
            'nation': 0.20,      # Nations in ASIA (5 of 25)
            'supplier': 0.20,    # Suppliers in ASIA nations
            'customer': 0.20,    # Customers in ASIA nations
            'orders': 0.16,      # o_orderdate in 1994 (1 year of 6.5)
            'lineitem': 1.0,     # No direct filter
        },
        join_selectivity={
            'region': 1.0,
            'nation': 1.0,
            'supplier': 1.0,
            'customer': 1.0,
            'orders': 0.20,      # Orders from ASIA customers
            'lineitem': 0.20,    # Lineitems from ASIA suppliers
        },
        # Very selective: ASIA region + 1994 + local supplier constraint
        output_selectivity=0.20 * 0.20 * 0.16 * 0.20,  # ~0.13%
    )
    queries.append(q5)

    # =========================================================================
    # Q10: Returned Item Reporting
    # SELECT c_custkey, c_name, SUM(l_extendedprice * (1 - l_discount)) as revenue,
    #        c_acctbal, n_name, c_address, c_phone, c_comment
    # FROM customer, orders, lineitem, nation
    # WHERE c_custkey = o_custkey
    #   AND l_orderkey = o_orderkey
    #   AND o_orderdate >= '1993-10-01'
    #   AND o_orderdate < '1994-01-01'
    #   AND l_returnflag = 'R'
    #   AND c_nationkey = n_nationkey
    # GROUP BY c_custkey, c_name, c_acctbal, c_phone, n_name, c_address, c_comment
    # =========================================================================
    q10 = QueryRowAnalysis(
        query_name='Q10',
        description='Returned Item Reporting',
        tables=['nation', 'customer', 'orders', 'lineitem'],
        filter_selectivity={
            'nation': 1.0,       # No filter
            'customer': 1.0,     # No filter
            'orders': 0.04,      # Q4 1993 (3 months of 6.5 years)
            'lineitem': 0.25,    # l_returnflag = 'R' (~25%)
        },
        join_selectivity={
            'nation': 1.0,
            'customer': 1.0,
            'orders': 1.0,
            'lineitem': 1.0,
        },
        # ~1% of lineitems: 4% orders * 25% returned
        output_selectivity=0.04 * 0.25,  # ~1%
    )
    queries.append(q10)

    # =========================================================================
    # Q18: Large Volume Customer
    # SELECT c_name, c_custkey, o_orderkey, o_orderdate, o_totalprice,
    #        SUM(l_quantity)
    # FROM customer, orders, lineitem
    # WHERE o_orderkey IN (
    #     SELECT l_orderkey FROM lineitem
    #     GROUP BY l_orderkey HAVING SUM(l_quantity) > 300
    # )
    #   AND c_custkey = o_custkey
    #   AND o_orderkey = l_orderkey
    # GROUP BY c_name, c_custkey, o_orderkey, o_orderdate, o_totalprice
    # =========================================================================
    q18 = QueryRowAnalysis(
        query_name='Q18',
        description='Large Volume Customer',
        tables=['customer', 'orders', 'lineitem'],
        filter_selectivity={
            'customer': 1.0,     # No filter
            'orders': 0.01,      # ~1% orders have quantity > 300
            'lineitem': 1.0,     # No direct filter (but join with filtered orders)
        },
        join_selectivity={
            'customer': 1.0,
            'orders': 1.0,
            'lineitem': 0.01,    # Only lineitems for large orders
        },
        # Very selective: only large orders (~1%)
        output_selectivity=0.01,
    )
    queries.append(q18)

    return queries


def analyze_row_reduction(query: QueryRowAnalysis, sf: int) -> Dict:
    """Analyze row reduction for a query."""

    # Calculate total input rows
    total_input_rows = 0
    input_breakdown = {}
    for table in query.tables:
        rows = get_row_count(table, sf)
        total_input_rows += rows
        input_breakdown[table] = rows

    # Calculate output rows (after filter + join, before aggregation)
    # The output is based on the probe table (usually lineitem) with all selectivities applied

    # Find the main fact table (usually lineitem or the largest table)
    fact_table = 'lineitem' if 'lineitem' in query.tables else query.tables[-1]
    fact_rows = get_row_count(fact_table, sf)

    # Apply output selectivity
    output_rows = int(fact_rows * query.output_selectivity)

    # Calculate reduction ratio
    reduction_ratio = total_input_rows / output_rows if output_rows > 0 else float('inf')
    reduction_pct = (1 - output_rows / total_input_rows) * 100 if total_input_rows > 0 else 0

    return {
        'query': query.query_name,
        'description': query.description,
        'scale_factor': sf,
        'total_input_rows': total_input_rows,
        'output_rows': output_rows,
        'reduction_ratio': reduction_ratio,
        'reduction_pct': reduction_pct,
        'input_breakdown': input_breakdown,
    }


def print_analysis(results: List[Dict], sf: int):
    """Print row reduction analysis."""

    print("=" * 90)
    print(f"TPC-H Row Reduction Analysis (SF{sf})")
    print("=" * 90)
    print()
    print("This analysis compares:")
    print("  • Input: Total rows from all tables involved in the query")
    print("  • Output: Rows after filter + join (before aggregation)")
    print()

    print("-" * 90)
    print(f"{'Query':<8} {'Description':<30} {'Input Rows':>18} {'Output Rows':>18} {'Reduction':>12}")
    print("-" * 90)

    for r in results:
        input_str = f"{r['total_input_rows']:,}"
        output_str = f"{r['output_rows']:,}"
        reduction_str = f"{r['reduction_pct']:.1f}%"
        print(f"{r['query']:<8} {r['description']:<30} {input_str:>18} {output_str:>18} {reduction_str:>12}")

    print("-" * 90)
    print()

    # Detailed breakdown
    for r in results:
        print(f"\n{r['query']}: {r['description']}")
        print("=" * 70)

        print("\n📥 Input Tables:")
        for table, rows in r['input_breakdown'].items():
            print(f"    {table:<15}: {rows:>18,} rows")
        print(f"    {'─' * 15}  {'─' * 18}")
        print(f"    {'TOTAL':<15}: {r['total_input_rows']:>18,} rows")

        print(f"\n📤 Output (after filter + join):")
        print(f"    Result rows    : {r['output_rows']:>18,} rows")

        print(f"\n📉 Reduction:")
        print(f"    Ratio          : {r['reduction_ratio']:,.0f}x")
        print(f"    Percentage     : {r['reduction_pct']:.1f}%")


def plot_comparison(results: List[Dict], sf: int, output_path: str = None):
    """Generate bar chart comparing input vs output rows."""
    if not HAS_MATPLOTLIB:
        print("Warning: matplotlib not available")
        return

    palette = sns.color_palette("crest", 2)
    color_input = adjust_color(palette[0])
    color_output = palette[1]

    queries = [r['query'] for r in results]
    input_rows = [r['total_input_rows'] for r in results]
    output_rows = [r['output_rows'] for r in results]
    reduction_pct = [r['reduction_pct'] for r in results]

    sns.set_theme(style="whitegrid")
    fig, ax = plt.subplots(figsize=(6, 3), facecolor='white')
    ax.set_facecolor('white')

    x = np.arange(len(queries))
    width = 0.35

    bars1 = ax.bar(x - width/2, input_rows, width,
                   label='Input Table Rows',
                   color=color_input, alpha=0.85, linewidth=1.5)
    bars2 = ax.bar(x + width/2, output_rows, width,
                   label='Output Rows (after Filter + Join)',
                   color=color_output, alpha=0.85, linewidth=1.5)

    ax.set_yscale('log')

    max_val = max(input_rows)
    ax.set_ylim(top=max_val * 10)

    ax.set_xlabel('TPC-H Query', fontsize=12)
    ax.set_ylabel('Number of Rows', fontsize=12)
    ax.set_xticks(x)
    ax.set_xticklabels(queries)

    legend = ax.legend(loc='upper center', fontsize=9, framealpha=1.0,
                       fancybox=True, shadow=True, ncol=2)
    legend.get_frame().set_facecolor('white')

    ax.grid(axis='y', linestyle='--', alpha=0.5)
    ax.grid(axis='x', visible=False)

    def format_rows(n):
        if n >= 1e9:
            return f'{n/1e9:.1f}B'
        elif n >= 1e6:
            return f'{n/1e6:.0f}M'
        elif n >= 1e3:
            return f'{n/1e3:.0f}K'
        return str(n)

    for bar, val in zip(bars1, input_rows):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                format_rows(val),
                ha='center', va='bottom', fontsize=8, fontweight='bold',
                color='black')

    for bar, val, pct in zip(bars2, output_rows, reduction_pct):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                f'{format_rows(val)}\n(-{pct:.1f}%)',
                ha='center', va='bottom', fontsize=8, fontweight='bold',
                color='black')

    plt.tight_layout()

    path = output_path or os.path.join(FIGURE_DIR, f'row_reduction_sf{sf}.pdf')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    plt.savefig(path, dpi=200, bbox_inches='tight', facecolor='white')
    print(f"\nPlot saved to: {path}")
    plt.close()


def main():
    parser = argparse.ArgumentParser(
        description='TPC-H Row Reduction Analysis'
    )
    parser.add_argument('--sf', type=int, default=1000,
                        help='Scale factor (default: 1000)')
    parser.add_argument('--no-plot', action='store_true',
                        help='Skip plot generation')

    args = parser.parse_args()
    sf = args.sf

    queries = define_queries()
    results = [analyze_row_reduction(q, sf) for q in queries]

    print_analysis(results, sf)

    if not args.no_plot:
        plot_comparison(results, sf)

    # Summary
    print("\n" + "=" * 90)
    print("SUMMARY")
    print("=" * 90)

    total_input = sum(r['total_input_rows'] for r in results)
    total_output = sum(r['output_rows'] for r in results)
    avg_reduction = sum(r['reduction_pct'] for r in results) / len(results)

    print(f"\nScale Factor: SF{sf}")
    print(f"Queries Analyzed: {len(results)}")
    print(f"\nTotal Input Rows (all queries): {total_input:,}")
    print(f"Total Output Rows (all queries): {total_output:,}")
    print(f"Average Row Reduction: {avg_reduction:.1f}%")
    print(f"Overall Reduction Ratio: {total_input/total_output:,.0f}x")


if __name__ == '__main__':
    main()
