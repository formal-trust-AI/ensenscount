#!/usr/bin/env python3
"""
Download results - retrieves experiment results from remote server
"""

import yaml
import os
import sys
from pathlib import Path

# Add utils to path
sys.path.insert(0, str(Path(__file__).parent.parent / "utils"))
from ssh_utils import SSHConnection


def load_config(path=None):
    if path is None:
        # Get the project root directory (two levels up from this script)
        script_dir = Path(__file__).parent
        project_root = script_dir.parent.parent
        path = project_root / "config" / "experiment_config.yaml"
    
    with open(path, 'r') as f:
        return yaml.safe_load(f)


def main():
    config = load_config()
    user = config['user']
    server = config['server']
    host = config['host']
    exp_folder = config['experiment_folder']
    local_results_dir = config.get('local_results_dir', './experiment_results')
    
    print("=== Downloading Experiment Results ===")
    print(f"From: {exp_folder} on {host}")
    print(f"To: {local_results_dir}")
    
    with SSHConnection(user, server, host) as ssh_conn:
        print("Connected to remote server")
        
        # Create local results directory
        os.makedirs(local_results_dir, exist_ok=True)
        
        # Download outputs
        print("Downloading outputs...")
        if ssh_conn.download_files(f"{exp_folder}/outputs", local_results_dir, recursive=True):
            print("✓ Downloaded outputs")
        else:
            print("⚠️  Could not download outputs")
        
        # Download logs
        print("Downloading logs...")
        if ssh_conn.download_files(f"{exp_folder}/logs", local_results_dir, recursive=True):
            print("✓ Downloaded logs")
        else:
            print("⚠️  Could not download logs")
        
        # Download plots
        print("Downloading plots...")
        if ssh_conn.download_files(f"{exp_folder}/*.png", local_results_dir):
            print("✓ Downloaded plots")
        else:
            print("⚠️  No plots found")
        
        # Download experiment log
        print("Downloading experiment log...")
        if ssh_conn.download_files(f"{exp_folder}/experiment_run.log", local_results_dir):
            print("✓ Downloaded experiment log")
        else:
            print("⚠️  Could not download experiment log")
        
        print(f"\n✓ Results downloaded to {local_results_dir}")


if __name__ == '__main__':
    main()
