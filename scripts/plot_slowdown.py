import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import colorsys
import argparse
import os


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
    parser = argparse.ArgumentParser(
        description='Plot oblivious-vs-non-oblivious slowdown per engine per query'
    )
    parser.add_argument('-i', '--input', type=str, default='data.csv', help='Input CSV file path')
    parser.add_argument('-o', '--output', type=str, default='slowdown.pdf', help='Output image file path')
    parser.add_argument('--log-scale', action='store_true', help='Enable log scale for y-axis')
    parser.add_argument('-t', '--title', type=str, help='Title for the plot')
    parser.add_argument('--figsize', type=str, default='10,6', help='Figure size as width,height (default: 10,6)')
    parser.add_argument('--bar-labels', action='store_true', help='Enable slowdown value labels on top of bars')
    parser.add_argument('--ylabel', type=str, default='Slowdown (x)', help='Label for the y-axis')
    parser.add_argument('--ylim-scale', type=float, default=None, help='Scale factor for y-axis upper limit')
    parser.add_argument('--time-col', type=str, default='Time (s)', help='Name of the time column (default: "Time (s)")')
    parser.add_argument('--oblivious-col', type=str, default='Oblivious', help='Name of the oblivious flag column (default: Oblivious)')
    parser.add_argument('--oblivious-val', type=str, default='Yes', help='Value indicating oblivious row (default: Yes)')
    parser.add_argument('--nobl-val', type=str, default='No', help='Value indicating non-oblivious row (default: No)')
    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Error: {args.input} not found.")
        return

    df = pd.read_csv(args.input)
    df[args.time_col] = pd.to_numeric(df[args.time_col], errors='coerce')

    obliv = df[df[args.oblivious_col] == args.oblivious_val].groupby(['Engine', 'Query'])[args.time_col].mean()
    nobl  = df[df[args.oblivious_col] == args.nobl_val ].groupby(['Engine', 'Query'])[args.time_col].mean()

    slowdown = (obliv / nobl).reset_index()
    slowdown.columns = ['Engine', 'Query', 'Slowdown']
    slowdown['QueryNum'] = slowdown['Query'].apply(parse_query_num)
    slowdown = slowdown.sort_values('QueryNum')

    query_order  = sorted(slowdown['Query'].unique(),  key=parse_query_num)
    engine_order = sorted(slowdown['Engine'].unique())

    figsize = tuple(map(float, args.figsize.split(',')))
    sns.set_theme(style="whitegrid")
    fig, ax = plt.subplots(figsize=figsize, dpi=300)

    base_palette = sns.color_palette("crest", len(engine_order))
    palette = {
        eng: color if eng == "VelODB" else adjust_color(color)
        for eng, color in zip(engine_order, base_palette)
    }

    barplot = sns.barplot(
        data=slowdown,
        x='Query',
        y='Slowdown',
        hue='Engine',
        palette=palette,
        ax=ax,
        order=query_order,
        hue_order=engine_order,
    )

    # Reference line at 1× (no slowdown)
    ax.axhline(1, color='black', linewidth=0.8, linestyle='--', alpha=0.6)

    if args.title:
        plt.title(args.title, fontsize=16)
    plt.xlabel('Query', fontsize=12)
    plt.ylabel(args.ylabel, fontsize=12)

    if args.log_scale:
        ax.set_yscale('log')

    if args.ylim_scale:
        ymin, ymax = ax.get_ylim()
        ax.set_ylim(ymin, ymax * args.ylim_scale)

    ax.legend(
        loc="lower center",
        bbox_to_anchor=(0.5, 1),
        ncol=len(engine_order),
        title='Engine',
        fontsize=9,
        title_fontsize=10,
    )

    if args.bar_labels:
        for i, engine in enumerate(engine_order):
            container = barplot.containers[i]
            for j, bar in enumerate(container):
                h = bar.get_height()
                if h <= 0:
                    continue
                x = bar.get_x() + bar.get_width() / 2
                ax.text(x, h, f'{h:.2f}x', ha='center', va='bottom',
                        fontsize=8, fontweight='bold')

    plt.tight_layout()
    plt.savefig(args.output)
    print(f"Figure successfully saved to {args.output}")


if __name__ == "__main__":
    main()
