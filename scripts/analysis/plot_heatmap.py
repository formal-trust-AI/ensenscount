#!/usr/bin/env python3
"""
Generate heatmap of xcount execution times by number of trees and depth
"""

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import re
import sys
import os

# Set style
sns.set_style("white")
plt.rcParams['figure.figsize'] = (12, 10)


def parse_model_name(model_name):
    """
    Extract number of trees and depth from model name
    Format: XXXX_d_Y where XXXX is number of trees and Y is depth
    Example: 0040_d_6 -> trees=40, depth=6
    """
    match = re.match(r'(\d+)_d_(\d+)', model_name)
    if match:
        num_trees = int(match.group(1))
        depth = int(match.group(2))
        return num_trees, depth
    return None, None


def load_execution_times(csv_file, timeout_value):
    """Load execution times from CSV and parse model metadata"""
    df = pd.read_csv(csv_file)
    
    # Parse model names to extract trees and depth
    parsed_data = []
    for _, row in df.iterrows():
        num_trees, depth = parse_model_name(row['model_name'])
        if num_trees is not None and depth is not None:
            xcount_time = row['xcount'] if pd.notna(row['xcount']) else timeout_value
            parsed_data.append({
                'num_trees': num_trees,
                'depth': depth,
                'feature': row['feature'],
                'xcount_time': xcount_time
            })
    
    return pd.DataFrame(parsed_data)


def create_heatmap(df, timeout_value, output_file='plots/heatmap_xcount_times.png'):
    """Create heatmap of execution times"""
    
    # Group by num_trees and depth, taking average over features
    pivot_data = df.groupby(['num_trees', 'depth'])['xcount_time'].mean().reset_index()
    
    # Create pivot table for heatmap
    heatmap_data = pivot_data.pivot(index='depth', columns='num_trees', values='xcount_time')
    
    # Sort indices for better visualization
    heatmap_data = heatmap_data.sort_index(ascending=False)
    heatmap_data = heatmap_data.sort_index(axis=1)
    
    # Create figure
    fig, ax = plt.subplots(figsize=(14, 10))
    
    # Create custom colormap with timeout value in red
    cmap = sns.color_palette("YlOrRd", as_cmap=True)
    
    # Create heatmap
    sns.heatmap(heatmap_data, 
                annot=True, 
                fmt='.2f', 
                cmap=cmap,
                cbar_kws={'label': 'Execution Time (seconds)'},
                linewidths=0.5,
                linecolor='gray',
                ax=ax,
                vmin=0,
                vmax=timeout_value)
    
    ax.set_xlabel('Number of Trees', fontsize=14, fontweight='bold')
    ax.set_ylabel('Depth', fontsize=14, fontweight='bold')
    ax.set_title('XCount Execution Time Heatmap\n(averaged over features, missing data = timeout)', 
                 fontsize=16, fontweight='bold', pad=20)
    
    # Rotate x-axis labels for better readability
    plt.xticks(rotation=45, ha='right')
    plt.yticks(rotation=0)
    
    plt.tight_layout()
    
    # Create output directory if it doesn't exist
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"✓ Saved heatmap to {output_file}")
    plt.close()
    
    # Print statistics
    print(f"\n=== Heatmap Statistics ===")
    print(f"Number of tree values: {sorted(heatmap_data.columns.tolist())}")
    print(f"Depth values: {sorted(heatmap_data.index.tolist(), reverse=True)}")
    print(f"Total cells: {heatmap_data.size}")
    print(f"Missing cells (timeout): {heatmap_data.isna().sum().sum()}")
    print(f"Min execution time: {heatmap_data.min().min():.2f}s")
    print(f"Max execution time: {heatmap_data.max().max():.2f}s")
    print(f"Mean execution time: {heatmap_data.mean().mean():.2f}s")
    print(f"Median execution time: {heatmap_data.median().median():.2f}s")
    
    # Count instances of each configuration
    print(f"\n=== Data Coverage ===")
    counts = df.groupby(['num_trees', 'depth']).size().reset_index(name='count')
    print(f"Configurations with data: {len(counts)}")
    print(f"Features per configuration (avg): {counts['count'].mean():.1f}")
    print(f"Features per configuration (range): {counts['count'].min()}-{counts['count'].max()}")
    
    return heatmap_data


def main():
    if len(sys.argv) < 3:
        print("Usage: python plot_heatmap.py <execution_times.csv> <timeout_value> [output_file]")
        print("\nArguments:")
        print("  execution_times.csv - CSV file from parse_execution_times.py")
        print("  timeout_value       - Value to use for missing/timeout data (in seconds)")
        print("  output_file         - Optional: output PNG file path (default: plots/heatmap_xcount_times.png)")
        print("\nExample:")
        print("  python plot_heatmap.py execution_times.csv 1800")
        print("  python plot_heatmap.py execution_times.csv 3600 my_heatmap.png")
        sys.exit(1)
    
    csv_file = sys.argv[1]
    timeout_value = float(sys.argv[2])
    output_file = sys.argv[3] if len(sys.argv) > 3 else 'plots/heatmap_xcount_times.png'
    
    if not os.path.exists(csv_file):
        print(f"Error: File {csv_file} does not exist")
        sys.exit(1)
    
    print(f"Loading data from {csv_file}...")
    print(f"Using timeout value: {timeout_value}s")
    
    # Load and parse data
    df = load_execution_times(csv_file, timeout_value)
    
    if df.empty:
        print("Error: No valid data found in CSV")
        sys.exit(1)
    
    print(f"Loaded {len(df)} data points")
    
    # Create heatmap
    heatmap_data = create_heatmap(df, timeout_value, output_file)
    
    # Save processed data to CSV for reference
    csv_output = output_file.replace('.png', '_data.csv')
    heatmap_data.to_csv(csv_output)
    print(f"✓ Saved heatmap data to {csv_output}")


if __name__ == '__main__':
    main()
