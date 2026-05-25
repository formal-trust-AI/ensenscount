#!/usr/bin/env python3
"""
Script to analyze ADD node growth as trees are added in XGBoost counting experiments.
Creates plots showing the number of ADD nodes vs. tree number for each subproblem.
"""

import os
import re
import glob
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path
import argparse

def count_nodes_in_dot_file(dot_file_path):
    """
    Count the number of nodes in a DOT file by parsing the node definitions.
    
    Args:
        dot_file_path (str): Path to the DOT file
        
    Returns:
        int: Number of nodes in the ADD
    """
    if not os.path.exists(dot_file_path):
        return 0
    
    try:
        with open(dot_file_path, 'r') as f:
            content = f.read()
        
        # Count unique node identifiers (hexadecimal addresses like "0x6fd")
        # These appear as quoted strings in the DOT file
        node_pattern = r'"0x[0-9a-fA-F]+"'
        nodes = set(re.findall(node_pattern, content))
        
        return len(nodes)
    
    except Exception as e:
        print(f"Error reading {dot_file_path}: {e}")
        return 0

def extract_tree_number(filename):
    """
    Extract tree number from filename like 'ensemble1_after_tree_5.dot'
    
    Args:
        filename (str): Filename to parse
        
    Returns:
        int: Tree number, or -1 if not found
    """
    match = re.search(r'after_tree_(\d+)\.dot', filename)
    if match:
        return int(match.group(1))
    return -1

def analyze_subproblem(subproblem_dir):
    """
    Analyze a single subproblem directory and return node count data.
    
    Args:
        subproblem_dir (str): Path to subproblem directory
        
    Returns:
        dict: Data for both ensembles with tree numbers and node counts
    """
    intermediate_dir = os.path.join(subproblem_dir, 'intermediate_adds')
    
    if not os.path.exists(intermediate_dir):
        print(f"Warning: No intermediate_adds directory found in {subproblem_dir}")
        return None
    
    # Data storage for both ensembles
    data = {
        'ensemble1': {'tree_numbers': [], 'node_counts': []},
        'ensemble2': {'tree_numbers': [], 'node_counts': []}
    }
    
    # Get all DOT files in intermediate_adds directory
    dot_files = glob.glob(os.path.join(intermediate_dir, '*.dot'))
    
    for dot_file in dot_files:
        filename = os.path.basename(dot_file)
        
        # Determine which ensemble this file belongs to
        if filename.startswith('ensemble1_'):
            ensemble = 'ensemble1'
        elif filename.startswith('ensemble2_'):
            ensemble = 'ensemble2'
        else:
            continue
        
        # Extract tree number
        tree_num = extract_tree_number(filename)
        if tree_num == -1:
            continue
        
        # Count nodes in this DOT file
        node_count = count_nodes_in_dot_file(dot_file)
        
        # Store data
        data[ensemble]['tree_numbers'].append(tree_num)
        data[ensemble]['node_counts'].append(node_count)
    
    # Sort by tree number for both ensembles
    for ensemble in ['ensemble1', 'ensemble2']:
        if data[ensemble]['tree_numbers']:
            # Sort by tree number
            sorted_data = sorted(zip(data[ensemble]['tree_numbers'], 
                                   data[ensemble]['node_counts']))
            data[ensemble]['tree_numbers'] = [x[0] for x in sorted_data]
            data[ensemble]['node_counts'] = [x[1] for x in sorted_data]
    
    return data

def create_plot(data, subproblem_name, output_dir):
    """
    Create a plot showing ADD node growth for a subproblem.
    
    Args:
        data (dict): Data from analyze_subproblem
        subproblem_name (str): Name of the subproblem
        output_dir (str): Directory to save the plot
    """
    plt.figure(figsize=(12, 8))
    
    # Plot both ensembles
    colors = ['blue', 'red']
    markers = ['o', 's']
    
    for i, ensemble in enumerate(['ensemble1', 'ensemble2']):
        if data[ensemble]['tree_numbers']:
            plt.plot(data[ensemble]['tree_numbers'], 
                    data[ensemble]['node_counts'],
                    color=colors[i], 
                    marker=markers[i],
                    markersize=4,
                    linewidth=2,
                    label=f'{ensemble.capitalize()}')
    
    plt.xlabel('Tree Number', fontsize=12)
    plt.ylabel('Number of ADD Nodes', fontsize=12)
    plt.title(f'ADD Node Growth vs Tree Addition - {subproblem_name}', fontsize=14)
    plt.grid(True, alpha=0.3)
    plt.legend()
    
    # Set integer ticks for x-axis if reasonable number of trees
    if data['ensemble1']['tree_numbers'] or data['ensemble2']['tree_numbers']:
        all_tree_nums = (data['ensemble1']['tree_numbers'] + 
                        data['ensemble2']['tree_numbers'])
        if max(all_tree_nums) <= 50:  # Only if reasonable number
            plt.xticks(range(0, max(all_tree_nums) + 1, max(1, max(all_tree_nums) // 10)))
    
    # Save plot
    plot_filename = f'{subproblem_name}_add_node_growth.png'
    plot_path = os.path.join(output_dir, plot_filename)
    plt.savefig(plot_path, dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"Saved plot: {plot_path}")

def main():
    parser = argparse.ArgumentParser(description='Analyze ADD node growth in debug output')
    parser.add_argument('--debug-dir', default='debug_output',
                       help='Path to debug output directory (default: debug_output)')
    parser.add_argument('--output-dir', default='debug_output',
                       help='Directory to save plots (default: same as debug-dir)')
    parser.add_argument('--subproblem', 
                       help='Analyze specific subproblem only (e.g., subproblem_0)')
    
    args = parser.parse_args()
    
    debug_dir = args.debug_dir
    output_dir = args.output_dir
    
    if not os.path.exists(debug_dir):
        print(f"Error: Debug directory {debug_dir} does not exist")
        return 1
    
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Find all subproblem directories
    if args.subproblem:
        subproblem_dirs = [os.path.join(debug_dir, args.subproblem)]
    else:
        subproblem_pattern = os.path.join(debug_dir, 'subproblem_*')
        subproblem_dirs = glob.glob(subproblem_pattern)
        subproblem_dirs.sort(key=lambda x: int(re.search(r'subproblem_(\d+)', x).group(1)))
    
    if not subproblem_dirs:
        print("No subproblem directories found")
        return 1
    
    print(f"Found {len(subproblem_dirs)} subproblem(s) to analyze")
    
    # Process each subproblem
    total_processed = 0
    for subproblem_dir in subproblem_dirs:
        if not os.path.isdir(subproblem_dir):
            continue
            
        subproblem_name = os.path.basename(subproblem_dir)
        print(f"\nAnalyzing {subproblem_name}...")
        
        # Analyze this subproblem
        data = analyze_subproblem(subproblem_dir)
        
        if data is None:
            print(f"Skipping {subproblem_name} - no data found")
            continue
        
        # Check if we have any data
        has_data = (len(data['ensemble1']['tree_numbers']) > 0 or 
                   len(data['ensemble2']['tree_numbers']) > 0)
        
        if not has_data:
            print(f"Skipping {subproblem_name} - no valid DOT files found")
            continue
        
        # Create output directory for this subproblem
        subproblem_output_dir = os.path.join(output_dir, subproblem_name)
        os.makedirs(subproblem_output_dir, exist_ok=True)
        
        # Create plot
        create_plot(data, subproblem_name, subproblem_output_dir)
        
        # Print summary
        for ensemble in ['ensemble1', 'ensemble2']:
            if data[ensemble]['tree_numbers']:
                max_trees = max(data[ensemble]['tree_numbers'])
                max_nodes = max(data[ensemble]['node_counts'])
                print(f"  {ensemble}: {len(data[ensemble]['tree_numbers'])} trees, "
                      f"max {max_nodes} nodes at tree {max_trees}")
        
        total_processed += 1
    
    print(f"\nCompleted! Processed {total_processed} subproblems.")
    print(f"Plots saved in: {output_dir}")
    
    return 0

if __name__ == '__main__':
    exit(main())