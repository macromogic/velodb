import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import argparse
import os
import numpy as np

def main():
    parser = argparse.ArgumentParser(description='Plot TPC-H benchmark results')
    parser.add_argument('-i', '--input', type=str, default='data.csv', help='Input CSV file path')
    parser.add_argument('-o', '--output', type=str, default='benchmark_comparison.png', help='Output image file path')
    parser.add_argument('--log-scale', action='store_true', help='Enable log scale for y-axis')
    parser.add_argument('-t', '--title', type=str, default='TPC-H Benchmark (SF=1)', help='Title for the plot')
    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Error: {args.input} not found.")
        return

    # Load data
    df = pd.read_csv(args.input)

    # Identify timeout entries before conversion
    df['is_timeout'] = df['Time(ms)'].astype(str).str.upper() == 'TIMEOUT'

    # Handle TIMEOUT values - convert to NaN for numeric processing
    df['Time(ms)'] = pd.to_numeric(df['Time(ms)'], errors='coerce')

    # Track which (Query, Engine) pairs have timeouts
    timeout_info = df[df['is_timeout']].groupby(['Query', 'Engine']).size().reset_index(name='timeout_count')
    timeout_set = set(zip(timeout_info['Query'], timeout_info['Engine']))

    if not timeout_info.empty:
        print("Warning: The following queries had timeouts:")
        for _, row in timeout_info.iterrows():
            print(f"  {row['Query']} ({row['Engine']}): {row['timeout_count']} timeout(s)")

    # Calculate average time per query and engine (excluding timeouts)
    avg_df = df[~df['is_timeout']].groupby(['Query', 'Engine'])['Time(ms)'].mean().reset_index()

    # Sort queries numerically (Q1, Q2, ... instead of Q1, Q10, Q2...)
    avg_df['QueryNum'] = avg_df['Query'].apply(lambda x: int(x.replace('Q', '')))

    # Determine all queries and engines
    all_queries = sorted(df['Query'].unique(), key=lambda x: int(x.replace('Q', '')))
    all_engines = sorted(df['Engine'].unique())

    # For timeout-only entries, use a small placeholder value
    TIMEOUT_PLACEHOLDER = 0.01  # Will be replaced visually

    # Ensure all (Query, Engine) combinations exist in avg_df
    for query in all_queries:
        for engine in all_engines:
            if not ((avg_df['Query'] == query) & (avg_df['Engine'] == engine)).any():
                # Add placeholder entry
                is_timeout = (query, engine) in timeout_set
                new_row = pd.DataFrame({
                    'Query': [query],
                    'Engine': [engine],
                    'Time(ms)': [TIMEOUT_PLACEHOLDER if is_timeout else 0],
                    'QueryNum': [int(query.replace('Q', ''))],
                })
                avg_df = pd.concat([avg_df, new_row], ignore_index=True)

    avg_df = avg_df.sort_values('QueryNum')

    # Determine the order for plotting
    query_order = all_queries
    engine_order = all_engines

    # Set up the plot
    fig, ax = plt.subplots(figsize=(12, 8), dpi=300)
    sns.set_theme(style="whitegrid")

    # Draw the bar chart
    barplot = sns.barplot(
        data=avg_df,
        x='Query',
        y='Time(ms)',
        hue='Engine',
        palette="viridis",
        ax=ax,
        order=query_order,
        hue_order=engine_order
    )

    # Add labels and title
    plt.title(args.title, fontsize=16)
    plt.xlabel('Query', fontsize=12)
    plt.ylabel('Time (ms)', fontsize=12)

    # Apply log scale if requested
    if args.log_scale:
        ax.set_yscale('log')

    # Get the y-axis limit for positioning timeout labels
    y_max = ax.get_ylim()[1]

    # Add value labels on top of bars, mark timeouts
    for i, engine in enumerate(engine_order):
        container = barplot.containers[i]
        labels = []
        for j, bar in enumerate(container):
            query = query_order[j]
            bar_height = bar.get_height()
            if (query, engine) in timeout_set:
                # Check if this is a timeout-only entry or has partial data
                avg_row = avg_df[(avg_df['Query'] == query) & (avg_df['Engine'] == engine)]
                original_time = avg_row['Time(ms)'].values[0] if not avg_row.empty else 0
                if original_time <= TIMEOUT_PLACEHOLDER:
                    # Pure timeout - hide bar and show text
                    labels.append('TIMEOUT')
                    bar.set_height(0)
                    bar.set_visible(False)
                else:
                    # Has some successful runs but also timeouts
                    labels.append(f'{bar_height:.1f}\n(+TIMEOUT)')
            elif bar_height <= 0:
                labels.append('')
            else:
                labels.append(f'{bar_height:.1f}')
        barplot.bar_label(container, labels=labels, padding=3, fontsize=8)

    plt.tight_layout()

    # Save the plot
    plt.savefig(args.output)
    print(f"Chart successfully saved to {args.output}")

if __name__ == "__main__":
    main()
