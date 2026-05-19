# EnSensCount : a tool for quantifying sensitivity
EnSensCount is a tool for counting the number of sensitive regions in the input space of decision tree ensembles. It 
takes as input the model file and gives the approximate count as the output within given tolerance (referred to as "epsilon") with a certain probability (referred to as "delta"). Both of these parameters can be configured by the user.

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

Below is a peak into the tool interface (input and output) for a given input.

```bash
./ensenscount -f ./models/0001_d.json -g 2 -s 2 -p 3 -k 1 -M pepin -V 1
```

```bash
c o Model path :./models/0001_d.json
c o Model name: 0001_d
c o Precision: 3
c o Gap: 2000
c o Debug output: disabled
c o Sanity checking: disabled
c o Time starts now
c o Sensitive features: 2 
c o Counting method: pepin
c o Loading the json file...c o Done
c o No. of trees in the ensemble: 1
c o No. of splits in the original ensemble: 7
c o No. of sensitive feature splits in the original ensemble: 3
c o Guard counts feature:2, count: 3
c o Guard counts feature:5, count: 1
c o Guard counts feature:8, count: 2
c o Guard counts feature:9, count: 1
c o Subproblems generated: 6
[...]
c o Final estimated count: 31
c o Subproblem 5 ends
c o Subproblem 0: count = 0, time taken = 5.34074 s
c o Subproblem 1: count = 12, time taken = 5.33509 s
c o Subproblem 2: count = 0, time taken = 1.10534 s
c o Subproblem 3: count = 24, time taken = 1.29098 s
c o Subproblem 4: count = 0, time taken = 1.10243 s
c o Subproblem 5: count = 31, time taken = 1.27648 s
c o Pepin global state cleared
Total count (pepin counting): 31
Total time taken: 17.1373 ms
c o Maximum subproblem time: 5.34074 ms
c o Log written to: ./logs/0001_d_gap_2000.000000_sens__2_prec_3.000000_bitd_1_20260519-162029.log
c o Fraction of solutions violating sensitive feature guards: 0.645833
```

For help:
```bash
./ensenscount -h
```

## Explanation of Command Line Options 

The sensitive feature can be selected by specifying its feature index (features are indexed by natural numbers) in the option ```-s```. The gap threshold is specified in the ```-g``` option, ```-k``` option specifying how many hguards of the sensitive feature can differ. The user can provide the ```epsilon``` and ```delta``` values as per their discretion. The tool has three verbosity levels which can be chosen  by the user (```-V [0,1,2]```), default level is ```1```. The tool also has another mode for exact count, which works only for very small ensembles (<20 trees), which can be activated by the following command line parameter - ```-M exact```. Precision level of the leaves of the ensemble can be specified in ```-p``` as number of decimal places for e.g ```-p 3``` specifies precision of leaf values upto 3 decimal places.
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
│    ├── main.cpp  # Program entry point: parses CLI options, loads XGBoost JSON models, builds subproblems, and runs counting.
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
