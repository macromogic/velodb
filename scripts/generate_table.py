import argparse
import pandas as pd
import re
import os


def parse_operation_type(operation):
    if 'SeqScan' in operation:
        return '1. Linear Scan'
    if 'Projection' in operation:
        return '2. Projection'
    elif 'Join' in operation or 'Post-join' in operation:
        return '3. Join'
    elif 'Bitonic Sort (Materialization)' in operation:
        return '*. Bitonic Sort (Materialization)'
    elif 'Materialization' in operation:
        return '4. Materialization'
    elif 'Sort' in operation:
        return '5. Order By'
    elif 'Limit' in operation:
        return '6. Limit'
    elif 'FilterCompaction' in operation:
        return 'FilterCompaction'
    else:
        return None


def main():
    parser = argparse.ArgumentParser(description='Generate summary table from benchmark results')
    parser.add_argument('-i', '--input', type=str, default='data.csv', help='Input CSV file path')
    args = parser.parse_args()
    if not os.path.exists(args.input):
        print(f"Error: {args.input} not found.")
        return

    query_breakdown = []
    with open(args.input, 'r') as f:
        parse_state = 'HEADER'
        for line in f:
            match parse_state:
                case 'HEADER':
                    if entries := re.match(r'^Running (Q\d+) \(SF=[0-9.]+, iteration (\d+)\)', line):
                        query = entries.group(1)
                        iteration = entries.group(2)
                        parse_state = 'TIME'
                case 'TIME':
                    if entries := re.match(r'^.*Execution time:\s*([0-9.]+)ms', line):
                        time_ms = float(entries.group(1))
                        parse_state = 'WAIT_BODY'
                case 'WAIT_BODY':
                    if line.startswith('-----'):
                        parse_state = 'BODY'
                case 'BODY':
                    if line.startswith('====='):
                        parse_state = 'HEADER'
                    elif entries := re.split(r'\s+', line.strip()):
                        operation = ' '.join(entries[:-5])
                        time_ms = float(entries[-4])
                        if op_type := parse_operation_type(operation):
                            query_breakdown.append((query, iteration, op_type, time_ms))

    breakdown_df = pd.DataFrame(query_breakdown, columns=['Query', 'Iteration', 'Operation', 'Time'])
    breakdown_df = breakdown_df.groupby(['Query', 'Iteration', 'Operation']).sum()
    breakdown_df = breakdown_df.groupby(['Query', 'Operation']).mean()
    group_df = breakdown_df.groupby(['Query'])
    print("Query,Engine,Time")
    for _, group in group_df:
        bitonic_sort_time = group[group.index.get_level_values('Operation') == '*. Bitonic Sort (Materialization)']['Time']
        total_time = group['Time'].sum()
        if bitonic_sort_time.empty:
            bitonic_overhead_time = 0
        else:
            bitonic_overhead_time = bitonic_sort_time.iloc[0]
        print(f"{group.index[0][0]},VelODB,{total_time / 1000:.2f}")
        print(f"{group.index[0][0]},VelODB (non-oblivious),{(total_time - bitonic_overhead_time) / 1000:.2f}")
        group['Percentage'] = (group['Time'] / total_time) * 100
        group['Format'] = group.apply(lambda row: f"{row['Time']:.2f}ms ({row['Percentage']:.1f}%)", axis=1)
        # print(group)
        # print(f'Total time for {group.index[0][0]}: {total_time:.2f}ms\n')



if __name__ == "__main__":
    main()
