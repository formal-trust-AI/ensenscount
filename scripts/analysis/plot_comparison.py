#!/usr/bin/env python3
"""
Plot count error comparison from CSV files
"""

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import sys
import os
from pathlib import Path

# Set style
sns.set_style("whitegrid")
plt.rcParams['figure.figsize'] = (14, 8)


def plot_error_percentage(csv_file, output_dir='plots', clip_limit=200):
    """Plot error percentages, clipped and sorted"""
    
    # Read CSV
    df = pd.read_csv(csv_file)
    
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Create model-feature labels
    df['label'] = df['model_name'] + '_f' + df['feature'].astype(str)
    
    # Remove rows with missing data
    df_clean = df.dropna(subset=['error_percent'])
    
    if len(df_clean) == 0:
        print("No data to plot")
        return
    
    # Clip error percentages
    df_clean['error_clipped'] = df_clean['error_percent'].clip(-clip_limit, clip_limit)
    
    # Sort by error percentage
    df_sorted = df_clean.sort_values('error_clipped')
    
    # Plot
    fig, ax = plt.subplots(figsize=(16, 8))
    colors = ['red' if e > 0 else 'blue' if e < 0 else 'green' for e in df_sorted['error_clipped']]
    bars = ax.bar(range(len(df_sorted)), df_sorted['error_clipped'], color=colors, alpha=0.7, edgecolor='black', linewidth=0.5)
    
    ax.axhline(y=0, color='black', linestyle='-', linewidth=1.5)
    ax.axhline(y=clip_limit, color='red', linestyle='--', linewidth=1, alpha=0.5, label=f'Clipped at ±{clip_limit}%')
    ax.axhline(y=-clip_limit, color='red', linestyle='--', linewidth=1, alpha=0.5)
    
    ax.set_xlabel('Model-Feature Combination (sorted by error)', fontsize=12)
    ax.set_ylabel('Error Percentage (%) [clipped]', fontsize=12)
    ax.set_title(f'Count Error: (xcount - xcount_base) / xcount_base × 100 (clipped at ±{clip_limit}%)', fontsize=14)
    ax.set_xticks(range(len(df_sorted)))
    ax.set_xticklabels(df_sorted['label'], rotation=90, ha='right', fontsize=8)
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/error_percentage_clipped.png', dpi=300, bbox_inches='tight')
    print(f"✓ Saved {output_dir}/error_percentage_clipped.png")
    plt.close()
    
    # Print statistics
    print(f"\n=== Error Statistics ===")
    print(f"Total instances: {len(df_clean)}")
    print(f"Average error: {df_clean['error_percent'].mean():.3f}%")
    print(f"Median error: {df_clean['error_percent'].median():.3f}%")
    print(f"Max error: {df_clean['error_percent'].max():.3f}%")
    print(f"Min error: {df_clean['error_percent'].min():.3f}%")
    print(f"Exact matches (0% error): {(df_clean['error_percent'] == 0).sum()}/{len(df_clean)}")
    print(f"Within ±10% error: {(df_clean['error_percent'].abs() <= 10).sum()}/{len(df_clean)}")
    print(f"Clipped values (beyond ±{clip_limit}%): {(df_clean['error_percent'].abs() > clip_limit).sum()}/{len(df_clean)}")


def main():
    if len(sys.argv) < 2:
        print("Usage: python plot_comparison.py <count_comparison.csv> [output_directory] [clip_limit]")
        print("\nExamples:")
        print("  python plot_comparison.py count_comparison.csv")
        print("  python plot_comparison.py count_comparison.csv plots 200")
        print("  python plot_comparison.py count_comparison.csv plots 100")
        sys.exit(1)
    
    csv_file = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else 'plots'
    clip_limit = float(sys.argv[3]) if len(sys.argv) > 3 else 200
    
    if not os.path.exists(csv_file):
        print(f"Error: File {csv_file} does not exist")
        sys.exit(1)
    
    plot_error_percentage(csv_file, output_dir, clip_limit)


if __name__ == '__main__':
    main()
