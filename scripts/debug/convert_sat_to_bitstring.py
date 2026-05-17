#!/usr/bin/env python3
"""
Convert SAT assignments to bit strings based on variable mappings.
In the bit string: 0 = false, 1 = true
Bits are arranged in increasing order of feature number and split value.
Sensitive features are separated by a space at the end.
"""

import re
import sys
from collections import defaultdict


def parse_variable_mapping(mapping_file):
    """Parse the variable mapping file to create a mapping from variable names to feature info."""
    with open(mapping_file, 'r') as f:
        content = f.read()
    
    # Parse mappings like 'x1': v1_b_f0_24.0 (handles negative numbers)
    pattern = r"'(x\d+)':\s*(v\d+_b_f(\d+)_([-\d.]+))"
    matches = re.findall(pattern, content)
    
    var_to_feature = {}
    for var_name, full_feature, feature_num, split_value in matches:
        var_num = int(var_name[1:])  # Extract number from 'x1' -> 1
        feature_num = int(feature_num)
        split_value = float(split_value)
        var_to_feature[var_num] = {
            'feature': feature_num,
            'split': split_value,
            'full_name': full_feature
        }
    
    return var_to_feature


def sort_variables_by_feature_split(var_to_feature, sensitive_features):
    """Sort variables by feature number, then by split value.
    Separate sensitive and non-sensitive features."""
    # Create lists for sensitive and non-sensitive variables
    non_sensitive_vars = []
    sensitive_vars = []
    
    for var_num, info in var_to_feature.items():
        var_tuple = (var_num, info['feature'], info['split'])
        if info['feature'] in sensitive_features:
            sensitive_vars.append(var_tuple)
        else:
            non_sensitive_vars.append(var_tuple)
    
    # Sort each list by feature number first, then by split value
    non_sensitive_vars.sort(key=lambda x: (x[1], x[2]))
    sensitive_vars.sort(key=lambda x: (x[1], x[2]))
    
    non_sensitive_order = [var_num for var_num, _, _ in non_sensitive_vars]
    sensitive_order = [var_num for var_num, _, _ in sensitive_vars]
    
    return non_sensitive_order, sensitive_order


def parse_sat_assignment(sat_lines):
    """Parse SAT assignment lines (v lines) into a set of positive literals."""
    positive_vars = set()
    
    for line in sat_lines:
        line = line.strip()
        if not line.startswith('v '):
            continue
        
        # Remove 'v ' prefix and split
        literals = line[2:].strip().split()
        
        for lit in literals:
            lit_num = int(lit)
            if lit_num > 0:
                positive_vars.add(lit_num)
            elif lit_num == 0:
                break  # End of assignment
    
    return positive_vars


def convert_to_bitstring(positive_vars, non_sensitive_order, sensitive_order):
    """Convert SAT assignment to bit string based on sorted variable order.
    Non-sensitive features first, then a space, then sensitive features."""
    non_sensitive_bits = []
    sensitive_bits = []
    
    for var_num in non_sensitive_order:
        if var_num in positive_vars:
            non_sensitive_bits.append('1')
        else:
            non_sensitive_bits.append('0')
    
    for var_num in sensitive_order:
        if var_num in positive_vars:
            sensitive_bits.append('1')
        else:
            sensitive_bits.append('0')
    
    return ''.join(non_sensitive_bits) + ' ' + ''.join(sensitive_bits)


def process_sat_file(sat_file, var_to_feature, sensitive_features):
    """Process SAT file and convert all assignments to bit strings."""
    non_sensitive_order, sensitive_order = sort_variables_by_feature_split(var_to_feature, sensitive_features)
    
    print(f"Non-sensitive variable order: {non_sensitive_order}")
    print(f"Sensitive variable order: {sensitive_order}")
    print(f"Total variables: {len(non_sensitive_order) + len(sensitive_order)}")
    print()
    
    with open(sat_file, 'r') as f:
        lines = f.readlines()
    
    assignments = []
    current_assignment = []
    
    for line in lines:
        line = line.strip()
        if line.startswith('s SATISFIABLE'):
            if current_assignment:
                # Process previous assignment
                positive_vars = parse_sat_assignment(current_assignment)
                bitstring = convert_to_bitstring(positive_vars, non_sensitive_order, sensitive_order)
                assignments.append(bitstring)
            current_assignment = []
        elif line.startswith('v '):
            current_assignment.append(line)
    
    # Process last assignment
    if current_assignment:
        positive_vars = parse_sat_assignment(current_assignment)
        bitstring = convert_to_bitstring(positive_vars, non_sensitive_order, sensitive_order)
        assignments.append(bitstring)
    
    return assignments, non_sensitive_order, sensitive_order


def print_assignments_with_details(assignments, non_sensitive_order, sensitive_order, var_to_feature):
    """Print assignments with detailed variable information."""
    print(f"\nFound {len(assignments)} satisfying assignments:\n")
    
    # Print header with variable info
    print("Non-sensitive variable order:")
    for i, var_num in enumerate(non_sensitive_order):
        info = var_to_feature[var_num]
        print(f"  Bit {i}: x{var_num} = {info['full_name']} (f{info['feature']}, split={info['split']})")
    
    print("\nSensitive variable order:")
    for i, var_num in enumerate(sensitive_order):
        info = var_to_feature[var_num]
        print(f"  Bit {i}: x{var_num} = {info['full_name']} (f{info['feature']}, split={info['split']})")
    print()
    
    # Print each assignment
    for idx, bitstring in enumerate(assignments, 1):
        print(f"Assignment {idx}: {bitstring}")


def main():
    if len(sys.argv) < 4:
        print("Usage: python convert_sat_to_bitstring.py <variable_mapping.txt> <sat_assignment.txt> <sensitive_features> [--output <file>] [--verbose]")
        print("  sensitive_features: comma-separated list of feature numbers (e.g., '1,2,3')")
        print("  --output: optional output file path (default: assignments_output.txt)")
        sys.exit(1)
    
    mapping_file = sys.argv[1]
    sat_file = sys.argv[2]
    sensitive_features_str = sys.argv[3]
    verbose = '--verbose' in sys.argv or '-v' in sys.argv
    
    # Parse output file
    output_file = "assignments_output.txt"
    if '--output' in sys.argv:
        output_idx = sys.argv.index('--output')
        if output_idx + 1 < len(sys.argv):
            output_file = sys.argv[output_idx + 1]
    
    # Parse sensitive features
    try:
        sensitive_features = set(int(f.strip()) for f in sensitive_features_str.split(',') if f.strip())
    except ValueError:
        print("Error: sensitive_features must be comma-separated integers (e.g., '1,2,3')")
        sys.exit(1)
    
    print(f"Sensitive features: {sorted(sensitive_features)}")
    
    # Parse variable mapping
    var_to_feature = parse_variable_mapping(mapping_file)
    print(f"Loaded {len(var_to_feature)} variable mappings")
    
    # Process SAT file
    assignments, non_sensitive_order, sensitive_order = process_sat_file(sat_file, var_to_feature, sensitive_features)
    
    # Save assignments to file
    with open(output_file, 'w') as f:
        for bitstring in assignments:
            f.write(f"{bitstring}\n")
    
    print(f"\n✅ Saved {len(assignments)} assignments to: {output_file}")
    
    if verbose:
        print_assignments_with_details(assignments, non_sensitive_order, sensitive_order, var_to_feature)
    else:
        print(f"\nFound {len(assignments)} satisfying assignments:\n")
        for idx, bitstring in enumerate(assignments, 1):
            print(f"{bitstring}")


if __name__ == "__main__":
    main()
