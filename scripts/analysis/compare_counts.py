#!/usr/bin/env python3
"""
Extract and compare counts from experiment output files
"""

import os
import re
import csv
import sys
from pathlib import Path


def is_baseline_file(filepath):
    """Determine if a file is baseline by checking for baseline-specific content"""
    try:
        with open(filepath, 'r') as f:
            content = f.read()
        # Baseline files have "Building ADD for ensemble" or "Total variables created"
        return "Building ADD for ensemble" in content or "Total variables created" in content
    except Exception:
        return False


def extract_count_from_opt_file(filepath):
    """Extract count from xcount (opt) output file"""
    try:
        with open(filepath, 'r') as f:
            content = f.read()
            
        # Look for "Total count (method counting): X" pattern
        match = re.search(r'Total count \([^)]+\):\s+(\d+\.?\d*)', content)
        if match:
            return float(match.group(1))
        
        return None
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return None


def extract_count_from_baseline_file(filepath):
    """Extract count from xcount_base (baseline) output file"""
    try:
        with open(filepath, 'r') as f:
            content = f.read()
            
        # Look for "Total satisfying assignments: X" pattern
        match = re.search(r'Total satisfying assignments:\s+(\d+\.?\d*)', content)
        if match:
            return float(match.group(1))
        
        return None
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return None


def extract_metadata_from_filename(filename):
    """Extract job id and model name from filename like 0000_0010_d_2.out or baseline_0001_0010_d_2.out"""
    # Remove baseline_ prefix if present
    clean_filename = filename.replace('baseline_', '')
    
    match = re.match(r'(\d+)_(.+)\.out$', clean_filename)
    if match:
        job_id = match.group(1)
        model_name = match.group(2)
        return job_id, model_name
    return None, filename


def extract_feature_from_file(filepath):
    """Extract sensitive feature from output file"""
    try:
        with open(filepath, 'r') as f:
            content = f.read()
            
        # Look for "Sensitive features: X" pattern
        match = re.search(r'Sensitive features?:\s+(\d+)', content)
        if match:
            return match.group(1)
        
        return None
    except Exception as e:
        return None


def calculate_error_percent(xcount, xcount_base):
    """Calculate error percentage: (xcount - xcount_base) / xcount_base * 100"""
    if xcount_base is None or xcount_base == 0:
        return None
    if xcount is None:
        return None
    
    return ((xcount - xcount_base) / xcount_base) * 100


def process_outputs_directory(outputs_dir, output_csv):
    """Process all output files in directory and generate CSV with count comparison"""
    
    if not os.path.exists(outputs_dir):
        print(f"Error: Directory {outputs_dir} does not exist")
        return
    
    # Collect data - keyed by (model_name, feature)
    data = {}
    
    # Process all .out files
    for filename in sorted(os.listdir(outputs_dir)):
        if not filename.endswith('.out'):
            continue
        
        filepath = os.path.join(outputs_dir, filename)
        job_id, model_name = extract_metadata_from_filename(filename)
        
        if job_id is None:
            print(f"Skipping {filename}: Could not extract metadata")
            continue
        
        # Extract feature from file content
        feature = extract_feature_from_file(filepath)
        if feature is None:
            print(f"Skipping {filename}: Could not extract feature")
            continue
        
        # Create a unique key based on model_name and feature
        key = (model_name, feature)
        
        # Initialize entry if not exists
        if key not in data:
            data[key] = {
                'model_name': model_name,
                'feature': feature,
                'xcount': None,
                'xcount_base': None,
                'error_percent': None
            }
        
        # Detect file type by content instead of filename prefix
        is_baseline = is_baseline_file(filepath)
        
        if is_baseline:
            count = extract_count_from_baseline_file(filepath)
            print(f"Processing baseline {filename}: feature={feature}, count={count}")
            data[key]['xcount_base'] = count
        else:
            count = extract_count_from_opt_file(filepath)
            print(f"Processing opt {filename}: feature={feature}, count={count}")
            data[key]['xcount'] = count
    
    # Calculate error percentages
    for key in data:
        xcount = data[key]['xcount']
        xcount_base = data[key]['xcount_base']
        error_percent = calculate_error_percent(xcount, xcount_base)
        data[key]['error_percent'] = error_percent
    
    # Write to CSV
    with open(output_csv, 'w', newline='') as csvfile:
        fieldnames = ['model_name', 'feature', 'xcount', 'xcount_base', 'error_percent']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        
        writer.writeheader()
        # Sort by model_name first, then by feature
        for key in sorted(data.keys()):
            writer.writerow(data[key])
    
    print(f"✓ Extracted counts for {len(data)} model-feature combinations")
    print(f"✓ CSV written to: {output_csv}")


def main():
    if len(sys.argv) < 2:
        print("Usage: python compare_counts.py <outputs_directory> [output_csv]")
        print("\nExample:")
        print("  python compare_counts.py outputs count_comparison.csv")
        print("  python compare_counts.py ./outputs")
        sys.exit(1)
    
    outputs_dir = sys.argv[1]
    output_csv = sys.argv[2] if len(sys.argv) > 2 else 'count_comparison.csv'
    
    process_outputs_directory(outputs_dir, output_csv)


if __name__ == '__main__':
    main()
