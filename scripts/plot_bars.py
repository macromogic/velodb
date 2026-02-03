import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import colorsys
import argparse
import os
import numpy as np

TIMEOUT_PLACEHOLDER = 0.01  # Will be replaced visually


def parse_query_num(query):
    return int(query.replace('Q', '').replace('q', ''))


def adjust_color(color):
    r, g, b = color
    h, l, s = colorsys.rgb_to_hls(r, g, b)
    l = max(0, min(1, l * 1.5))
    s = max(0, min(1, s * 0.5))
    r, g, b = colorsys.hls_to_rgb(h, l, s)
    return (r, g, b)


def main():
    parser = argparse.ArgumentParser(description='Plot TPC-H benchmark results')
    parser.add_argument('-i', '--input', type=str, default='data.csv', help='Input CSV file path')
    parser.add_argument('-o', '--output', type=str, default='benchmark_comparison.png', help='Output image file path')
    parser.add_argument('--log-scale', action='store_true', help='Enable log scale for y-axis')
    parser.add_argument('-t', '--title', type=str, help='Title for the plot')
    parser.add_argument('--figsize', type=str, default='12,8', help='Figure size as width,height (default: 12,8)')
    parser.add_argument('--bar-labels', action='store_true', help='Enable value labels on top of bars')
    parser.add_argument('--ylabel', type=str, default='Time (ms)', help='Label for the y-axis (default: Time (ms))')
    parser.add_argument('--hue', type=str, default='Engine', help='Column name to use for grouping/coloring bars (default: Engine)')
    parser.add_argument('--base-hue', type=str, default=None, help='Base hue value for relative speedup calculation. If set, bar labels show speedup relative to this hue value.')
    parser.add_argument('--ylim-scale', type=float, default=None, help='Scale factor for y-axis upper limit (e.g., 1.2 for 20%% extra space)')
    parser.add_argument('--timeout-label', type=str, default='TIMEOUT', help='Label to display for timeout entries (default: TIMEOUT)')
    parser.add_argument('--rotate-labels', action='store_true', help='Rotate bar labels by 90 degrees')
    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Error: {args.input} not found.")
        return
    df = pd.read_csv(args.input)

    df['is_timeout'] = df['Time'].astype(str).str.upper() == 'TIMEOUT'
    df['Time'] = pd.to_numeric(df['Time'], errors='coerce')

    hue_column = args.hue
    timeout_info = df[df['is_timeout']].groupby(['Query', hue_column]).size().reset_index(name='timeout_count')
    timeout_set = set(zip(timeout_info['Query'], timeout_info[hue_column]))
    if not timeout_info.empty:
        print("Warning: The following queries had timeouts:")
        for _, row in timeout_info.iterrows():
            print(f"  {row['Query']} ({row[hue_column]}): {row['timeout_count']} timeout(s)")

    avg_df = df[~df['is_timeout']].groupby(['Query', hue_column])['Time'].mean().reset_index()
    avg_df['QueryNum'] = avg_df['Query'].apply(parse_query_num)

    all_queries = sorted(df['Query'].unique(), key=parse_query_num)
    all_hue_values = sorted(df[hue_column].unique())


    # Ensure all (Query, hue_column) combinations exist in avg_df
    for query in all_queries:
        for hue_value in all_hue_values:
            if not ((avg_df['Query'] == query) & (avg_df[hue_column] == hue_value)).any():
                # Add placeholder entry
                is_timeout = (query, hue_value) in timeout_set
                new_row = pd.DataFrame({
                    'Query': [query],
                    hue_column: [hue_value],
                    'Time': [TIMEOUT_PLACEHOLDER if is_timeout else 0],
                    'QueryNum': [parse_query_num(query)],
                })
                avg_df = pd.concat([avg_df, new_row], ignore_index=True)
    avg_df = avg_df.sort_values('QueryNum')

    query_order = all_queries
    hue_order = all_hue_values

    figsize = tuple(map(float, args.figsize.split(',')))
    sns.set_theme(style="whitegrid")
    fig, ax = plt.subplots(figsize=figsize, dpi=300)

    base_pallette = sns.color_palette("crest", len(hue_order))
    palette = {hue_val: adjust_color(color) if hue_val != "VelODB" else color for hue_val, color in zip(hue_order, base_pallette)}

    barplot = sns.barplot(
        data=avg_df,
        x='Query',
        y='Time',
        hue=hue_column,
        palette=palette,
        ax=ax,
        order=query_order,
        hue_order=hue_order
    )

    if args.title:
        plt.title(args.title, fontsize=16)
    plt.xlabel('Query', fontsize=12)
    plt.ylabel(args.ylabel, fontsize=12)
    if args.log_scale:
        ax.set_yscale('log')

    # Apply y-axis upper limit scaling if specified
    if args.ylim_scale:
        ymin, ymax = ax.get_ylim()
        ax.set_ylim(ymin, ymax * args.ylim_scale)

    ax.legend(
        loc="lower center",
        bbox_to_anchor=(.5, 1),
        ncol=len(hue_order),
        title=hue_column,
        fontsize=9,
        title_fontsize=10
    )

    if args.bar_labels:
        # If base_hue is specified, get base values for each query
        base_values = {}
        if args.base_hue:
            for query in query_order:
                base_row = avg_df[(avg_df['Query'] == query) & (avg_df[hue_column] == args.base_hue)]
                if not base_row.empty:
                    base_values[query] = base_row['Time'].values[0]

        for i, hue_value in enumerate(hue_order):
            container = barplot.containers[i]
            labels = []
            for j, bar in enumerate(container):
                query = query_order[j]
                bar_height = bar.get_height()
                if (query, hue_value) in timeout_set:
                    # Check if this is a timeout-only entry or has partial data
                    avg_row = avg_df[(avg_df['Query'] == query) & (avg_df[hue_column] == hue_value)]
                    original_time = avg_row['Time'].values[0] if not avg_row.empty else 0
                    if original_time <= TIMEOUT_PLACEHOLDER:
                        # Pure timeout - hide bar and show text
                        labels.append(args.timeout_label)
                        bar.set_height(0)
                        bar.set_visible(False)
                    else:
                        # Has some successful runs but also timeouts
                        labels.append(f'{bar_height:.1f}\n(+{args.timeout_label})')
                elif bar_height <= 0:
                    labels.append('')
                else:
                    # Show relative speedup if base_hue is specified
                    if args.base_hue and query in base_values and base_values[query] > 0:
                        base_time = base_values[query]
                        if base_time <= TIMEOUT_PLACEHOLDER:
                            # Base is timeout, just show absolute value
                            labels.append(f'{bar_height:.1f}')
                        else:
                            speedup = bar_height / base_time
                            labels.append(f'{speedup:.2f}x')
                    else:
                        labels.append(f'{bar_height:.1f}')
            label_rotation = 90 if args.rotate_labels else 0
            barplot.bar_label(container, labels=labels, padding=1, fontsize=8, fontweight='bold', rotation=label_rotation)

    plt.tight_layout()
    plt.savefig(args.output)
    print(f"Figure successfully saved to {args.output}")

if __name__ == "__main__":
    main()
