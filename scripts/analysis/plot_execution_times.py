#!/usr/bin/env python3
"""
Plot execution times from CSV files as cactus plots.

A cactus plot shows the number of solved instances (x-axis) vs execution time (y-axis),
with times sorted in ascending order. This helps visualize solver performance.

Usage:
    python plot_execution_times.py <csv_file1> [csv_file2] ... [--output plot.html]
    
Example:
    python plot_execution_times.py times.csv --output cactus_plot.html
    python plot_execution_times.py times1.csv times2.csv timess.csv -o comparison.html
"""

import argparse
import sys
import csv
from pathlib import Path

try:
    import matplotlib.pyplot as plt
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Note: matplotlib not found. Will generate HTML plot instead.", file=sys.stderr)


def parse_csv(csv_path, timeout=None):
    """
    Parse a CSV file and extract execution times.
    
    Args:
        csv_path: Path to the CSV file
        timeout: If specified, use this value for empty/missing entries
    
    Returns:
        tuple: (solver_dict, benchmark_ids)
            solver_dict: {solver_name: {benchmark_id: time}}
            benchmark_ids: set of benchmark identifiers
    """
    results = {}
    benchmark_ids = set()
    
    with open(csv_path, 'r') as f:
        reader = csv.DictReader(f)
        headers = reader.fieldnames
        
        # Identify time columns (skip model, feature, job_id, model_name)
        skip_cols = {'model', 'feature', 'job_id', 'model_name'}
        time_columns = [col for col in headers if col not in skip_cols]
        
        # Determine which column to use for model name
        model_col = 'model' if 'model' in headers else 'model_name'
        
        # Initialize dicts for each solver
        for col in time_columns:
            results[col] = {}
        
        # Read all rows
        for row in reader:
            model = row.get(model_col, '').strip()
            feature = row.get('feature', '').strip()
            
            if not model or not feature:
                continue
            
            # Normalize model name: remove trailing _s suffix
            if model.endswith('_s'):
                model = model[:-2]
            
            # Create benchmark identifier: model_fFEATURE
            benchmark_id = f"{model}_f{feature}"
            benchmark_ids.add(benchmark_id)
            
            for col in time_columns:
                value = row[col].strip()
                if value and value != '':
                    try:
                        time_val = float(value)
                        results[col][benchmark_id] = time_val
                    except ValueError:
                        # Non-numeric value - skip
                        pass
    
    return results, benchmark_ids


def align_solver_counts(data_dict_with_benchmarks, timeout):
    """
    Align all solvers to have the same set of benchmarks by matching model+feature.
    
    Args:
        data_dict_with_benchmarks: {csv_name: (solver_dict, benchmark_ids)}
            where solver_dict is {solver_name: {benchmark_id: time}}
        timeout: Value to use for missing entries
    
    Returns:
        tuple: (aligned_dict, original_counts)
            aligned_dict: {csv_name: {solver: [sorted times]}}
            original_counts: {csv_name: {solver: original_count}}
    """
    # Collect all unique benchmarks across all CSVs (union)
    all_benchmarks = set()
    for csv_name, (solver_dict, benchmark_ids) in data_dict_with_benchmarks.items():
        all_benchmarks.update(benchmark_ids)
    
    all_benchmarks = sorted(all_benchmarks)
    total_benchmarks = len(all_benchmarks)
    
    print(f"\nTotal unique benchmarks across all files: {total_benchmarks}")
    
    # Align each solver to have all benchmarks
    aligned_results = {}
    original_counts = {}
    
    for csv_name, (solver_dict, benchmark_ids) in data_dict_with_benchmarks.items():
        aligned_results[csv_name] = {}
        original_counts[csv_name] = {}
        
        for solver_name, benchmark_times in solver_dict.items():
            # Store original count (actual benchmarks with values)
            original_counts[csv_name][solver_name] = len(benchmark_times)
            
            # Build aligned list
            times_list = []
            for benchmark_id in all_benchmarks:
                if benchmark_id in benchmark_times:
                    times_list.append(benchmark_times[benchmark_id])
                else:
                    # Missing benchmark - use timeout
                    times_list.append(timeout)
            
            # Sort the times for cactus plot
            aligned_results[csv_name][solver_name] = sorted(times_list)
    
    return aligned_results, original_counts


def plot_cactus(data_dict, output_file=None, title="Execution Time Cactus Plot"):
    """
    Create a cactus plot from execution time data.
    
    Args:
        data_dict: dict of {csv_filename: {solver: [times]}}
        output_file: Path to save the plot (optional)
        title: Plot title
    """
    if HAS_MATPLOTLIB:
        plot_cactus_matplotlib(data_dict, output_file, title)
    else:
        plot_cactus_html(data_dict, output_file or "cactus_plot.html", title)


def plot_cactus_matplotlib(data_dict, output_file=None, title="Execution Time Cactus Plot", original_counts=None):
    """Create a cactus plot using matplotlib."""
    plt.figure(figsize=(12, 8))
    
    # Color palette
    colors = plt.cm.tab10(np.linspace(0, 1, 10))
    color_idx = 0
    
    # Track max values for axis limits
    max_count = 0
    max_time = 0
    
    # Plot each solver from each CSV
    for csv_name, solver_data in data_dict.items():
        csv_basename = Path(csv_name).stem
        
        for solver_name, times in solver_data.items():
            if not times:
                continue
            
            # Sort times in ascending order
            sorted_times = sorted(times)
            
            # X-axis: number of instances solved (1 to len(times))
            x_values = list(range(1, len(sorted_times) + 1))
            
            # Get original count for legend
            if original_counts and csv_name in original_counts and solver_name in original_counts[csv_name]:
                original_count = original_counts[csv_name][solver_name]
            else:
                original_count = len(sorted_times)
            
            # Plot
            label = f"{csv_basename} - {solver_name} ({original_count} instances)"
            plt.plot(x_values, sorted_times, 
                    marker='o', markersize=4, linewidth=2,
                    color=colors[color_idx % len(colors)],
                    label=label, alpha=0.8)
            
            color_idx += 1
            
            # Update max values
            max_count = max(max_count, len(sorted_times))
            max_time = max(max_time, max(sorted_times))
    
    # Formatting
    plt.xlabel('Number of Benchmarks Solved', fontsize=12, fontweight='bold')
    plt.ylabel('Execution Time (seconds)', fontsize=12, fontweight='bold')
    plt.title(title, fontsize=14, fontweight='bold')
    plt.grid(True, alpha=0.3, linestyle='--')
    plt.legend(loc='best', fontsize=10, framealpha=0.9)
    
    # Set axis limits with some padding
    plt.xlim(0, max_count * 1.05)
    plt.ylim(0, max_time * 1.1)
    
    plt.tight_layout()
    
    # Save or show
    if output_file:
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        print(f"Plot saved to: {output_file}")
    else:
        plt.show()


def plot_cactus_html(data_dict, output_file, title, original_counts=None):
    """Create an interactive HTML cactus plot using Plotly-like JavaScript."""
    
    # Prepare data for plotting
    series_data = []
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', 
              '#8c564b', '#e377c2', '#7f7f7f', '#bcbd22', '#17becf']
    color_idx = 0
    
    for csv_name, solver_data in data_dict.items():
        csv_basename = Path(csv_name).stem
        
        for solver_name, times in solver_data.items():
            if not times:
                continue
            
            sorted_times = sorted(times)
            x_values = list(range(1, len(sorted_times) + 1))
            
            # Get original count for legend
            if original_counts and csv_name in original_counts and solver_name in original_counts[csv_name]:
                original_count = original_counts[csv_name][solver_name]
            else:
                original_count = len(sorted_times)
            
            series_data.append({
                'name': f"{csv_basename} - {solver_name} ({original_count} instances)",
                'x': x_values,
                'y': sorted_times,
                'color': colors[color_idx % len(colors)]
            })
            color_idx += 1
    
    # Generate HTML with embedded Chart.js
    html_content = f"""<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>{title}</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
    <style>
        body {{
            font-family: Arial, sans-serif;
            margin: 20px;
            background-color: #f5f5f5;
        }}
        .container {{
            max-width: 1400px;
            margin: 0 auto;
            background-color: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }}
        h1 {{
            text-align: center;
            color: #333;
        }}
        #chartContainer {{
            position: relative;
            height: 600px;
            margin-top: 20px;
        }}
    </style>
</head>
<body>
    <div class="container">
        <h1>{title}</h1>
        <div id="chartContainer">
            <canvas id="myChart"></canvas>
        </div>
    </div>
    
    <script>
        const ctx = document.getElementById('myChart');
        
        const data = {{
            datasets: ["""
    
    # Add dataset for each series
    for i, series in enumerate(series_data):
        if i > 0:
            html_content += ",\n                "
        
        x_data = ", ".join([f"{{x: {x}, y: {y}}}" for x, y in zip(series['x'], series['y'])])
        
        html_content += f"""{{
                    label: '{series['name']}',
                    data: [{x_data}],
                    borderColor: '{series['color']}',
                    backgroundColor: '{series['color']}',
                    borderWidth: 2,
                    pointRadius: 4,
                    pointHoverRadius: 6,
                    tension: 0.1
                }}"""
    
    html_content += """
            ]
        };
        
        const config = {
            type: 'line',
            data: data,
            options: {
                responsive: true,
                maintainAspectRatio: false,
                interaction: {
                    mode: 'nearest',
                    axis: 'x',
                    intersect: false
                },
                plugins: {
                    title: {
                        display: false
                    },
                    legend: {
                        display: true,
                        position: 'top',
                        labels: {
                            font: {
                                size: 12
                            },
                            padding: 15,
                            usePointStyle: true
                        }
                    },
                    tooltip: {
                        callbacks: {
                            label: function(context) {
                                return context.dataset.label + ': ' + context.parsed.y.toFixed(2) + 's';
                            }
                        }
                    }
                },
                scales: {
                    x: {
                        type: 'linear',
                        title: {
                            display: true,
                            text: 'Number of Benchmarks Solved',
                            font: {
                                size: 14,
                                weight: 'bold'
                            }
                        },
                        grid: {
                            color: 'rgba(0, 0, 0, 0.1)'
                        }
                    },
                    y: {
                        title: {
                            display: true,
                            text: 'Execution Time (seconds)',
                            font: {
                                size: 14,
                                weight: 'bold'
                            }
                        },
                        beginAtZero: true,
                        grid: {
                            color: 'rgba(0, 0, 0, 0.1)'
                        }
                    }
                }
            }
        };
        
        new Chart(ctx, config);
    </script>
</body>
</html>"""
    
    # Write to file
    with open(output_file, 'w') as f:
        f.write(html_content)
    
    print(f"Interactive HTML plot saved to: {output_file}")
    print(f"Open it in your browser to view the plot.")


def plot_cactus(data_dict, output_file=None, title="Execution Time Cactus Plot", original_counts=None):
    """
    Create a cactus plot from execution time data.
    
    Args:
        data_dict: dict of {csv_filename: {solver: [times]}}
        output_file: Path to save the plot (optional)
        title: Plot title
        original_counts: dict of {csv_filename: {solver: original_count}}
    """
    if HAS_MATPLOTLIB:
        # Generate matplotlib plot (PNG/PDF)
        plot_cactus_matplotlib(data_dict, output_file, title, original_counts)
        # Also generate HTML version
        if output_file:
            html_file = str(Path(output_file).with_suffix('.html'))
            plot_cactus_html(data_dict, html_file, title, original_counts)
    else:
        plot_cactus_html(data_dict, output_file or "cactus_plot.html", title, original_counts)


def print_statistics(data_dict):
    """Print summary statistics for each solver."""
    print("\n" + "="*80)
    print("SUMMARY STATISTICS")
    print("="*80)
    
    for csv_name, solver_data in data_dict.items():
        print(f"\nFile: {csv_name}")
        print("-" * 80)
        
        for solver_name, times in solver_data.items():
            if not times:
                print(f"  {solver_name}: No data")
                continue
            
            sorted_times = sorted(times)
            mean_time = sum(times) / len(times)
            median_time = sorted_times[len(sorted_times) // 2]
            
            print(f"\n  {solver_name}:")
            print(f"    Total instances: {len(times)}")
            print(f"    Min time:        {min(times):.2f}s")
            print(f"    Max time:        {max(times):.2f}s")
            print(f"    Mean time:       {mean_time:.2f}s")
            print(f"    Median time:     {median_time:.2f}s")
            
            if len(times) > 1:
                variance = sum((x - mean_time) ** 2 for x in times) / len(times)
                std_dev = variance ** 0.5
                print(f"    Std deviation:   {std_dev:.2f}s")
    
    print("\n" + "="*80 + "\n")


def main():
    parser = argparse.ArgumentParser(
        description='Plot execution times from CSV files as cactus plots.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python plot_execution_times.py times.csv
  python plot_execution_times.py times.csv timess.csv -o comparison.png
  python plot_execution_times.py *.csv --output results/cactus.png --title "Solver Comparison"
        """
    )
    
    parser.add_argument('csv_files', nargs='+', 
                        help='One or more CSV files containing execution times')
    parser.add_argument('-o', '--output', 
                        help='Output file path for the plot (PNG, PDF, SVG, etc.)')
    parser.add_argument('-t', '--title', 
                        default='Execution Time Cactus Plot',
                        help='Plot title (default: "Execution Time Cactus Plot")')
    parser.add_argument('--no-stats', action='store_true',
                        help='Skip printing statistics')
    parser.add_argument('--timeout', type=float, default=None,
                        help='Timeout value (in seconds) to use for missing/empty entries. If not specified, missing entries are ignored.')
    
    args = parser.parse_args()
    
    # Check if CSV files exist
    for csv_file in args.csv_files:
        if not Path(csv_file).exists():
            print(f"Error: File not found: {csv_file}", file=sys.stderr)
            sys.exit(1)
    
    # Parse all CSV files
    data_dict_with_benchmarks = {}
    print(f"Processing {len(args.csv_files)} CSV file(s)...")
    
    if args.timeout is not None:
        print(f"Using timeout value: {args.timeout}s for missing/empty entries\n")
    
    for csv_file in args.csv_files:
        print(f"  Reading {csv_file}...")
        try:
            solver_dict, benchmark_ids = parse_csv(csv_file, args.timeout)
            data_dict_with_benchmarks[csv_file] = (solver_dict, benchmark_ids)
            
            # Print what was found
            print(f"    Found {len(benchmark_ids)} unique benchmarks")
            for solver, times_dict in solver_dict.items():
                if times_dict:
                    print(f"      {solver}: {len(times_dict)} entries")
        except Exception as e:
            print(f"  Error parsing {csv_file}: {e}", file=sys.stderr)
            continue
    
    if not data_dict_with_benchmarks:
        print("Error: No data found in any CSV file", file=sys.stderr)
        sys.exit(1)
    
    # Align all solvers to have the same benchmarks if timeout is specified
    original_counts = None
    if args.timeout is not None:
        data_dict, original_counts = align_solver_counts(data_dict_with_benchmarks, args.timeout)
    else:
        # Convert to simple format without alignment
        data_dict = {}
        for csv_name, (solver_dict, _) in data_dict_with_benchmarks.items():
            data_dict[csv_name] = {}
            for solver_name, times_dict in solver_dict.items():
                data_dict[csv_name][solver_name] = sorted(times_dict.values())
    
    # Print statistics
    if not args.no_stats:
        print_statistics(data_dict)
    
    # Create plot
    print("Generating plot...")
    plot_cactus(data_dict, args.output, args.title, original_counts)
    
    if not args.output:
        print("Displaying plot (close window to exit)...")


if __name__ == '__main__':
    main()
