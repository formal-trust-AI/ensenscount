import matplotlib.pyplot as plt
import os
from pathlib import Path
import yaml
import sys
from collections import defaultdict

def load_config(path=None):
    if path is None:
        script_dir = Path(__file__).parent
        project_root = script_dir.parent.parent
        path = project_root / "config" / "experiment_config.yaml"
    with open(path, 'r') as f:
        return yaml.safe_load(f)

def plot_swapped_axis_cactus(log_dir,timeout,mode):
    """
    Generates and saves a plot with benchmark counts on the X-axis
    and sorted solve times on the Y-axis.
    """
    log_files = [f for f in os.listdir(log_dir) if f.endswith('.log')]

    grouped_times = defaultdict(list)
    
    for log_file in log_files:
        benchmark, code = None, None
        solve_time = timeout

        with open(os.path.join(log_dir, log_file), 'r') as f:
            for line in f:
                if "Benchmark:" in line:
                    benchmark = line.split(":", 1)[-1].strip()
                elif "Code:" in line:
                    code = line.split(":", 1)[-1].strip()
                elif (("time taken (wall)" in line) and (mode=="total")) or (("Maximum subproblem time:" in line )and(mode=="max")) or ("Total time taken: " in line):
                    solve_time = float(line.split(":", 1)[-1].strip())
                    break 
        if benchmark and code:
            grouped_times[(benchmark, code)].append(solve_time)
                

    plt.figure(figsize=(8, 6))

    for (benchmark, code), times in grouped_times.items():
        times.sort()
        solved_counts = range(1, len(times) + 1)
        label = f"{benchmark} | {code}"
        plt.plot(solved_counts, times, label=label)

    
    plt.xlabel("Benchmarks Solved")
    plt.ylabel("Time (s)")
    plt.title(f"Cactus Plot (mode={mode})")
    plt.grid(True)
    plt.legend(loc="best", fontsize=8)

    # Save plot
    plot_filename = f"cactus_plot_grouped_{mode}.png"
    plot_dir = os.path.join(log_dir, 'plots')
    os.makedirs(plot_dir, exist_ok=True)
    plt.savefig(os.path.join(plot_dir, plot_filename))
    plt.close()
    print(f"Grouped cactus plot saved to {os.path.join(plot_dir, plot_filename)}")


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in {"max", "total"}:
        print("Usage: python plot_cactus.py [max|total]")
        sys.exit(1)
    
    mode = sys.argv[1]
    config = load_config()
    timeout= float(config['timeout'])
    exp_directory="./experiment_results/logs"
    plot_swapped_axis_cactus(exp_directory,timeout,mode)

if __name__ == "__main__":
    main()
