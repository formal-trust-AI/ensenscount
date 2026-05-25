#!/usr/bin/env python3
"""
Trigger script - starts experiments on remote server with a single command
"""

import yaml
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
    
    print("=== Triggering Remote Experiments ===")
    print(f"Starting experiments on {host} in {exp_folder}")
    
    with SSHConnection(user, server, host) as ssh_conn:
        print("Connected to remote server")
        
        # Check if runner script exists
        try:
            ssh_conn.execute_command("test -f remote_runner.sh", cwd=exp_folder)
            print("✓ Remote runner script found")
        except:
            print("✗ Remote runner script not found!")
            print("Please run 'python3 upload_and_prepare.py' first")
            return
        
        # Start experiments in background
        print("Starting experiments in background...")
        cmd = "nohup ./remote_runner.sh > experiment_run.log 2>&1 &"
        ssh_conn.execute_command(cmd, cwd=exp_folder)
        
        print("✓ Experiments started successfully!")
        print(f"✓ Check progress with: python3 check_progress.py")
        print(f"✓ View logs with: ssh {user}@{host if server == host else server} 'tail -f {exp_folder}/experiment_run.log'")


if __name__ == '__main__':
    main()
