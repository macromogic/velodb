import os

os.environ["OPENBLAS_NUM_THREADS"] = "1"
os.environ["MKL_NUM_THREADS"] = "1"
os.environ["OMP_NUM_THREADS"] = "1"
os.environ["NUMEXPR_NUM_THREADS"] = "1"

from dataclasses import dataclass, field
import pandas as pd
import duckdb
import time
import gc
import argparse
import os


@dataclass
class TableInfo:
    columns: list[str]
    date_columns: list[str] = field(default_factory=list)


tpch_tables: dict[str, TableInfo] = {
    "lineitem": TableInfo(
        columns=[
            "l_orderkey", "l_partkey", "l_suppkey", "l_linenumber", "l_quantity",
            "l_extendedprice", "l_discount", "l_tax", "l_returnflag", "l_linestatus",
            "l_shipdate", "l_commitdate", "l_receiptdate", "l_shipinstruct",
            "l_shipmode", "l_comment"
        ],
        date_columns=["l_shipdate", "l_commitdate", "l_receiptdate"]
    ),
    "orders": TableInfo(
        columns=[
            "o_orderkey", "o_custkey", "o_orderstatus", "o_totalprice",
            "o_orderdate", "o_orderpriority", "o_clerk", "o_shippriority", "o_comment"
        ],
        date_columns=["o_orderdate"]
    ),
    "customer": TableInfo(
        columns=[
            "c_custkey", "c_name", "c_address", "c_nationkey", "c_phone",
            "c_acctbal", "c_mktsegment", "c_comment"
        ],
    ),
    "nation": TableInfo(
        columns=[
            "n_nationkey", "n_name", "n_regionkey", "n_comment"
        ],
    ),
    "region": TableInfo(
        columns=[
            "r_regionkey", "r_name", "r_comment"
        ],
    ),
    "part": TableInfo(
        columns=[
            "p_partkey", "p_name", "p_mfgr", "p_brand", "p_type",
            "p_size", "p_container", "p_retailprice", "p_comment"
        ],
    ),
    "supplier": TableInfo(
        columns=[
            "s_suppkey", "s_name", "s_address", "s_nationkey", "s_phone",
            "s_acctbal", "s_comment"
        ],
    ),
    "partsupp": TableInfo(
        columns=[
            "ps_partkey", "ps_suppkey", "ps_availqty", "ps_supplycost",
            "ps_comment"
        ],
    ),
}


def load_tpch_table(filename: str, columns: list[str], date_column_names: list[str] = None) -> pd.DataFrame:
    date_column_names = date_column_names or []
    df = pd.read_csv(
        filename,
        sep='|',
        header=None,
        names=columns + ['dummy'],
        usecols=columns,
        engine='c',
        parse_dates=date_column_names,
        index_col=False,
    )
    return df


def load_data(data_dir: str) -> dict[str, pd.DataFrame]:
    print("Loading data...")
    tables: dict[str, pd.DataFrame] = {}
    for table_name, table_info in tpch_tables.items():
        filename = f"{data_dir}/{table_name}.tbl"
        print(f"  Loading {table_name} from {filename}...")
        start_time = time.time()
        df = load_tpch_table(filename, table_info.columns, table_info.date_columns)
        tables[table_name] = df
        elapsed = time.time() - start_time
        print(f"    Loaded {len(df)} rows in {elapsed:.2f} seconds.")
        gc.collect()
    return tables


def main() -> None:
    parser = argparse.ArgumentParser(description="Load TPC-H tables into Pandas DataFrames.")
    parser.add_argument('--data-dir', type=str, required=True, help='Directory containing TPC-H .tbl files')
    parser.add_argument('--query-dir', type=str, required=False, help='Directory containing TPC-H query files')
    parser.add_argument('--iterations', '-i', type=int, default=1, help='Number of iterations to run each query')
    parser.add_argument('--queries', '-q', type=int, nargs='+', required=False, help='List of query numbers to run')
    args = parser.parse_args()

    tables = load_data(args.data_dir)
    with duckdb.connect() as con:
        con.execute("PRAGMA enable_profiling='json'")
        con.execute("PRAGMA disable_optimizer")
        con.execute("PRAGMA threads=1")
        for table_name, df in tables.items():
            con.register(table_name, df)

        if args.query_dir:
            query_dir = args.query_dir
        else:
            query_dir = os.path.join(os.path.dirname(__file__), '../tpch/queries/templates')
        query_files = sorted([f for f in os.listdir(query_dir) if f.startswith('q') and f.endswith('.sql')])
        for query_file in query_files:
            query_number = int(query_file[1:query_file.index('.')])
            if args.queries and query_number not in args.queries:
                continue
            query_path = os.path.join(query_dir, query_file)
            with open(query_path, 'r') as f:
                query_sql = f.read()
            # print(f"Running Query {query_number}...")
            for iteration in range(args.iterations):
                con.execute(f"PRAGMA profiling_output='q{query_number}_{iteration + 1}.json'")
                start_time = time.perf_counter_ns()
                con.execute(query_sql)
                result = con.fetchall()
                end_time = time.perf_counter_ns()
                elapsed_ms = (end_time - start_time) / 1_000_000
                print(f"Q{query_number},{iteration + 1},Pandas,{elapsed_ms:.2f}")
                # print(f"  Iteration {iteration + 1}: {len(result)} rows returned in {elapsed_ms:.2f} ms.")
                gc.collect()


if __name__ == "__main__":
    main()
