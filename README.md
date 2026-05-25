# EnSensCount : a tool for quantifying sensitivity
EnSensCount is a tool for counting the number of sensitive regions in the input space of decision tree ensembles. It 
takes as input the model files and gives the approximate count as the output within given epsilon-delta bounds that can be configured by the user.

# Usage Instructions

## Dependencies

```
sudo apt install make cmake g++
```

## After cloning the repository
```
make
make runtests
```

## Instructions to use the tool
```bash
./ensenscount -f <model-name> -g <gap between outputs> -p <precision, -1 for exact> -s <sensitive feature> -k <no of sensitive guards allowed to differ>
```

For help:
```bash
./ensenscount -h
```

## Directory Structure

```
xgboost-counting/
├── ensenscount_experiments           # Main entry point script
├── config/
│   └── experiment_config.yaml  # Configuration file
├── scripts/
│   ├── experiment/             # Experiment workflow scripts
│   │   ├── upload_and_prepare.py
│   │   ├── trigger_experiments.py
│   │   ├── check_progress.py
│   │   ├── download_results.py
│   │   └── run_experiment_legacy.py
│   ├── utils/                  # Reusable utilities
│   │   └── ssh_utils.py
│   └── analysis/               # Analysis and plotting
│       ├── plot_cactus.py
│       └── result.py
├── data/
│   └── benchmarks/             # Benchmark datasets
│       └── covtype_d3/
├── src/                        # Source code (C++)
│   ├── main.cpp
│   └── ...
├── include/                        # Header files
├── lib/                        # Libraries
├── experiment_results/         # Downloaded results
│   ├── outputs/
│   ├── logs/
│   └── plots/
└── reports/                    # Documentation and reports
```

## Configuration

Edit `config/experiment_config.yaml`:

```yaml
# SSH Connection
user: ajinkya
server: 10.129.26.36           # Jump server
host: 10.1.1.101               # Target server
experiment_folder: /home/ajinkya/experiments/new_exps

# Data and Parameters
benchmark_dir: ./data/benchmarks/covtype_d3
gap: 0.2
precision: -1
bit_distance: 1
num_cores: 25                  # Cores per ensenscount process
max_parallel_jobs: 8           # Parallel benchmark jobs
timeout: 3600

# Options
upload_code: true
compile: true
```

## Scripts Overview

### Experiment Scripts (`scripts/experiment/`)
- **`upload_and_prepare.py`** - Uploads code, compiles, generates runner script
- **`trigger_experiments.py`** - Starts experiments on remote server
- **`check_progress.py`** - Monitors experiment progress
- **`download_results.py`** - Downloads results and logs

### Analysis Scripts (`scripts/analysis/`)
- **`plot_cactus.py`** - Generates cumulative time cactus plots
- **`result.py`** - Result processing utilities

### Utilities (`scripts/utils/`)
- **`ssh_utils.py`** - SSH connection and file transfer utilities

## Results

Results are downloaded to `experiment_results/`:
- `outputs/` - Individual benchmark results
- `logs/` - Experiment logs
- `experiment_run.log` - Complete experiment log
- `cactus_plot.png` - Generated analysis plots
