import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import colorsys
import argparse
import os
import numpy as np


def parse_query_num(query):
    return int(query.replace('Q', '').replace('q', ''))


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
    parser.add_argument('-q', '--queries', type=str, default=None, help='Comma-separated list of queries to include (e.g., Q1,Q3). If not specified, includes all queries.')
    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Error: {args.input} not found.")
        return
    df = pd.read_csv(args.input)
    df['Time'] = pd.to_numeric(df['Time'], errors='coerce')

    hue_column = args.hue
    if args.queries:
        queries = [q.strip() for q in args.queries.split(',')]
        df = df[df['Query'].isin(queries)]
        if df.empty:
            print(f"Error: No data found for specified queries: {args.queries}")
            return
    avg_df = df.groupby(['SF', hue_column])['Time'].mean().reset_index()
    avg_df['QueryNum'] = avg_df['Query'].apply(parse_query_num)
    all_sfs = sorted(df['SF'].unique())
    all_hue_values = sorted(df[hue_column].unique())
    avg_df = avg_df.sort_values('SF')

    hue_order = sorted(all_hue_values, key=parse_query_num)

    figsize = tuple(map(float, args.figsize.split(',')))
    sns.set_theme(style="whitegrid")
    fig, ax = plt.subplots(figsize=figsize, dpi=300)

    lineplot = sns.lineplot(
        data=avg_df,
        x='SF',
        y='Time',
        hue=hue_column,
        style=hue_column,
        palette="crest",
        ax=ax,
        hue_order=hue_order,
        markers=True,
    )

    if args.title:
        plt.title(args.title, fontsize=16)
    plt.xlabel('SF', fontsize=12)
    plt.ylabel(args.ylabel, fontsize=12)
    if args.log_scale:
        ax.set_yscale('log')

    # Apply y-axis upper limit scaling if specified
    if args.ylim_scale:
        ymin, ymax = ax.get_ylim()
        ax.set_ylim(ymin, ymax * args.ylim_scale)

    plt.tight_layout()
    plt.savefig(args.output)
    print(f"Figure successfully saved to {args.output}")

if __name__ == "__main__":
    main()
