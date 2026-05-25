#!/usr/bin/env python3
"""
Progress checker - monitors remote experiment progress
"""

import yaml
import time
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


def check_process_running(ssh_conn, exp_folder):
    """Check if experiment process is still running"""
    try:
        # Check if remote_runner.sh process is running
        stdout, _ = ssh_conn.execute_command("pgrep -f remote_runner.sh", cwd=exp_folder)
        return bool(stdout.strip())
    except:
        return False


def get_progress_info(ssh_conn, exp_folder):
    """Get current progress information"""
    try:
        # Count completed output files
        stdout, _ = ssh_conn.execute_command("ls outputs/*.out 2>/dev/null | wc -l", cwd=exp_folder)
        completed = int(stdout.strip()) if stdout.strip() else 0
        
        # Get last few lines of log
        try:
            stdout, _ = ssh_conn.execute_command("tail -n 5 experiment_run.log", cwd=exp_folder)
            recent_log = stdout.strip()
        except:
            recent_log = "No log available"
        
        return completed, recent_log
    except Exception as e:
        return 0, f"Error getting progress: {e}"


def main():
    config = load_config()
    user = config['user']
    server = config['server']
    host = config['host']
    exp_folder = config['experiment_folder']
    
    print("=== Experiment Progress Monitor ===")
    print(f"Monitoring experiments on {host}")
    print("Press Ctrl+C to stop monitoring\n")
    
    try:
        with SSHConnection(user, server, host) as ssh_conn:
            while True:
                is_running = check_process_running(ssh_conn, exp_folder)
                completed, recent_log = get_progress_info(ssh_conn, exp_folder)
                
                print(f"Status: {'🟢 Running' if is_running else '🔴 Not Running'}")
                print(f"Completed jobs: {completed}")
                print("Recent activity:")
                print(recent_log)
                print("-" * 50)
                
                if not is_running and completed > 0:
                    print("✓ Experiments appear to have completed!")
                    break
                elif not is_running:
                    print("⚠️  No experiments running. Use trigger_experiments.py to start.")
                    break
                
                time.sleep(30)  # Check every 30 seconds
                
    except KeyboardInterrupt:
        print("\nMonitoring stopped by user")
    except Exception as e:
        print(f"Error: {e}")


if __name__ == '__main__':
    main()
