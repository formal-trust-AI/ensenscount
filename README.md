# EnSensCount : a tool for quantifying sensitivity
EnSensCount is a tool for counting the number of sensitive regions in the input space of decision tree ensembles. It 
takes as input the model file and gives the approximate count as the output within given epsilon-delta bounds that can be configured by the user.

# Usage Instructions

## Dependencies

```
sudo apt install make cmake g++
```

## Creating the binary
After cloning the repository, run the following commands in the root

```
make
```
This builds the tool binary and runs test cases for sanity check.

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
ensenscount/
├── CMakeLists.txt
├── Makefile
├── README.md
├── baseline/       #source files for ADD-baseline
│   ├── main.cpp
│   └── ...
├── include/
│   ├── DTree.h
│   ├── json.hpp
│   ├── pepin_counting.hpp
│   └── sanity_check.hpp
├── models/
│   ├── diabetes/           # DTE models trained on tabular datasets (as json files)
│   │   ├── 0010_3.json
│   │   └── ...
│   └── ...
├── scripts/
│   ├── analysis/
│   │   ├── compare_counts.py
│   │   ├── parse_execution_times.py
│   │   ├── plot_add_node_growth.py
│   │   ├── plot_cactus.py
│   │   ├── plot_comparison.py
│   │   ├── plot_execution_times.py
│   │   ├── plot_heatmap.py
│   │   ├── print_tree.py
│   │   └── result.py
│   ├── debug/ #additional tools for debugging
│   ├── ensenscount_experiments/ #script to run experiments
│   ├── experiment/
│   │   ├── check_progress.py
│   │   ├── download_results.py
│   │   ├── trigger_experiments.py
│   │   └── upload_and_prepare.py
│   ├── experiment_config.yaml #experiment configuration file
│   ├── json_to_png.py #to create visualisation of model as trees
│   ├── tests/
│   │   └── run_count_regression.sh #tests
│   └── utils/
│       ├── add_ssh_key.sh
│       └── ssh_utils.py
├── src/
│   ├── main.cpp  # Program entry point: parses CLI options, loads XGBoost JSON models, builds subproblems, and runs counting.
│    ├── utils.hpp # Shared data structures and declarations for trees, boolean split variables, config parsing, guards, and helpers.
│    ├── utils.cpp # Implements JSON tree parsing, CLI parsing, logging verbosity, guard generation/replacement, and utility routines.
│    ├── gen_subps.hpp # Declares helpers and the SubProblemGenerator used to create/prune sensitive-feature subproblems.
│    ├── gen_subps.cpp # Implements split collection, tree cloning/pruning, bitmask generation, and subproblem pair generation.
│    ├── solve_subps.hpp # Declares SubproblemSolver for converting trees to ADD/BDD representations and solving subproblems.
│    ├── solve_subps.cpp # Implements ADD construction, feature-order constraints, ensemble subtraction, thresholding, and counting dispatch.
│    ├── counting_config.hpp # Declares global counting configuration, selected method, bitmasks, Pepin parameters, and dump settings.
│    ├── counting_config.cpp # Stores and updates the global counting configuration used by counting implementations.
│    ├── counting_wrapper.hpp # Declares a unified counting interface that hides the selected counting backend.
│    ├── counting_wrapper.cpp # Dispatches counting calls to boundary, naive, or Pepin counting and computes the final aggregate count.
│    ├── counting_boundary.hpp # Declares the boundary-counting backend based on BDD satisfying minterm counts.
│    ├── counting_boundary.cpp # Implements boundary counting and optional dumping of satisfying assignments.
│    ├── counting_naive.hpp # Declares the naive exact-counting backend that enumerates satisfying assignments.
│    ├── counting_naive.cpp # Implements assignment enumeration, global de-duplication of M1x/M2x values, and optional assignment dumps.
│    ├── pepin_counting.cpp # Implements the Pepin-style randomized approximate counting backend over BDD samples.
│    ├── sanity_check.cpp # Implements optional BDD assignment sanity checks for feature-ordering constraints.
│    ├── debug_utils.hpp # Provides DebugOutputManager for exporting trees, ADDs, and intermediate debug artifacts as DOT files.
│    └── tree_exporter.hpp # Provides DecisionTreeExporter for writing internal decision trees to Graphviz DOT format.
└── tests/
    └── naive_regression_cases.tsv
```

## Configuration

Edit `config/experiment_config.yaml`:

```yaml
# SSH Connection
user: [user name]
server: [server ip]         # Jump server
host: [host ip]               # Target server
experiment_folder: /path/to/experiments/

# Data and Parameters
benchmark_dir: ./models/model-dir/
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
