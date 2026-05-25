import json
import numpy as np
import argparse


def sigmoid(x):
    return 1 / (1 + np.exp(-x))


def extract_feature_splits_from_model(model_path):
    """
    Extracts all unique split values for each feature from the model JSON.
    Returns: dict {feature_index: sorted list of split values}
    """
    with open(model_path) as f:
        trees = json.load(f)
    feature_splits = {}
    
    def collect_splits(node):
        if "split" in node:
            fid = node["split"]
            cond = node["split_condition"]
            feature_splits.setdefault(fid, set()).add(cond)
            for child in node["children"]:
                collect_splits(child)
    
    for tree in trees:
        collect_splits(tree)
    
    # Convert sets to sorted lists
    return {fid: sorted(list(vals)) for fid, vals in feature_splits.items()}


def bitstring_to_indices(bitstring):
    """Return the number of 0s in the bitstring."""
    return bitstring.count('0')


def assignment_to_feature_vector(x, m, features, sensitive, feature_splits, splits_map):
    """
    For each feature, use the bitstring to select the split value from feature_splits.
    For the sensitive feature, use the m bitstring; for others, use x.
    """
    # Determine the full vector length (max feature index + 1)
    all_fids = set(feature_splits.keys()) | set(features)
    vec_len = max(all_fids) + 1 if all_fids else 0
    vec = [0.0] * vec_len
    feature_bitstrings = {}
    curr_index = 0
    
    for fid in features:
        if fid == sensitive:
            feature_bitstrings[fid] = m
        else:
            feature_bitstrings[fid] = x[curr_index: curr_index + splits_map[fid]]
            curr_index += splits_map[fid]
    
    for fid in features:
        index = bitstring_to_indices(feature_bitstrings[fid])
        if index == 0:
            vec[fid] = float('-inf')  # Less than the smallest split
        else:
            vec[fid] = feature_splits[fid][index - 1]
    
    return np.array(vec, dtype=float)


def is_valid_bitstring(bitstring):
    """
    Check if bitstring satisfies consistency constraint:
    No 0 can appear after a 1 (i.e., must be of form 0*1*)
    """
    seen_one = False
    for bit in bitstring:
        if bit == '1':
            seen_one = True
        elif bit == '0' and seen_one:
            return False
    return True


def generate_hamming_distance_1(m1):
    """
    Generate all bitstrings at Hamming distance 1 from m1
    that satisfy the consistency constraint (no 0 after 1).
    """
    candidates = []
    m1_list = list(m1)
    
    for i in range(len(m1)):
        # Flip bit at position i
        new_m = m1_list.copy()
        new_m[i] = '0' if m1[i] == '1' else '1'
        new_bitstring = ''.join(new_m)
        
        # Check if valid (no 0 after 1)
        if is_valid_bitstring(new_bitstring):
            candidates.append(new_bitstring)
    
    return candidates


def parse_assignments(assignments_path, sensitive_feature=None, feature_splits_from_model=None):
    """
    Parse assignments.txt format with x m1 (no m2).
    Can get metadata from file or derive from model.
    Returns: features, splits_map, sensitive, assignments
    """
    sensitive = sensitive_feature
    features = []
    splits_map = {}
    assignments = []
    meta_lines = []
    
    with open(assignments_path) as f:
        lines = [line.strip() for line in f if line.strip()]
    
    # First, collect assignments until we hit a metadata line
    i = 0
    while i < len(lines):
        line = lines[i]
        if line.startswith("#"):
            i += 1
            continue
        parts = line.split()
        # Now expecting 2 parts: x m1
        if len(parts) == 2 and all(len(p) > 0 for p in parts):
            assignments.append(tuple(p.strip() for p in parts))
            i += 1
        else:
            break
    
    # The rest are metadata lines
    while i < len(lines):
        meta_lines.append(lines[i])
        i += 1
    
    # Try to parse metadata from file
    for line in meta_lines:
        if line.startswith("Sensitive features:") or line.startswith("Sensitive feature:"):
            sensitive = int(line.split(":")[1].strip())
        elif " " in line:
            parts = line.split()
            if len(parts) == 2 and parts[0].isdigit() and parts[1].isdigit():
                features.append(int(parts[0]))
                splits_map[int(parts[0])] = int(parts[1])
    
    # If no metadata in file, derive from model
    if not features and feature_splits_from_model:
        # Get all features except sensitive
        all_features = sorted(feature_splits_from_model.keys())
        features = [f for f in all_features if f != sensitive]
        # Add sensitive feature at the end
        if sensitive is not None and sensitive in all_features:
            features.append(sensitive)
        splits_map = {fid: len(splits) for fid, splits in feature_splits_from_model.items()}
    
    # Ensure sensitive feature is not in the middle of non-sensitive features
    if sensitive is not None and sensitive in features:
        features = [f for f in features if f != sensitive]
        features.append(sensitive)
    
    return features, splits_map, sensitive, assignments


def predict_tree(tree):
    def _predict(x):
        node = tree
        while "leaf" not in node:
            fid = node["split"]
            cond = node["split_condition"]
            # XGBoost convention: left if <, right if >=
            if x[fid] < cond:
                node = node["children"][0]
            else:
                node = node["children"][1]
        return node["leaf"]
    return _predict


def load_trees(model_path):
    with open(model_path) as f:
        trees = json.load(f)
    return [predict_tree(tree) for tree in trees]


def main():
    parser = argparse.ArgumentParser(
        description="Validate XGBoost assignments with Hamming distance 1 constraint."
    )
    parser.add_argument("--model", required=True, help="Path to model JSON dump.")
    parser.add_argument("--assignments", required=True, help="Path to assignments.txt with x m1 format.")
    parser.add_argument("--gap", type=float, default=0.0, help="Required gap threshold.")
    parser.add_argument("--sensitive", type=int, required=True, help="Sensitive feature index.")
    parser.add_argument("--verbose", action="store_true", help="Print detailed results.")
    args = parser.parse_args()
    
    trees = load_trees(args.model)
    feature_splits = extract_feature_splits_from_model(args.model)
    features, splits_map, sensitive, assignments = parse_assignments(
        args.assignments, args.sensitive, feature_splits
    )
    
    if args.verbose:
        for fid in features:
            print(f"Feature {fid} splits: {feature_splits.get(fid, [])}")
        print(f"Loaded {len(trees)} trees.")
        print(f"Sensitive feature: {sensitive}")
        print(f"Features: {features}")
        print(f"No. of splits: {splits_map}")
    
    print(f"Gap condition: {args.gap}")
    print(f"Total assignments to check: {len(assignments)}")
    print()
    
    valid_count = 0
    invalid_count = 0
    
    for idx, (x, m1) in enumerate(assignments):
        # Generate all valid m2 candidates at Hamming distance 1
        m2_candidates = generate_hamming_distance_1(m1)
        
        if args.verbose:
            print(f"\nAssignment {idx}: x={x}, m1={m1}")
            print(f"  Generated {len(m2_candidates)} valid m2 candidates")
        
        # Check if m1 itself is valid
        if not is_valid_bitstring(m1):
            print(f"⚠️  Assignment {idx}: m1={m1} violates consistency constraint!")
            invalid_count += 1
            continue
        
        # Compute prediction for (x, m1)
        v1 = assignment_to_feature_vector(x, m1, features, sensitive, feature_splits, splits_map)
        pred1 = sum(tree(v1) for tree in trees)
        
        # Check each m2 candidate
        found_valid = False
        for m2 in m2_candidates:
            v2 = assignment_to_feature_vector(x, m2, features, sensitive, feature_splits, splits_map)
            pred2 = sum(tree(v2) for tree in trees)
            
            if args.verbose:
                print(f"  m2={m2}, pred2={pred2:.6f}, diff={abs(pred1-pred2):.6f}", end="")
            
            if abs(pred1 - pred2) >= args.gap:
                found_valid = True
                if args.verbose:
                    print(f" ✓ VALID")
                    break
                else:
                    break
            else:
                if args.verbose:
                    print(f" ✗")
        
        if found_valid:
            valid_count += 1
            if not args.verbose:
                print(f"✅ Assignment {idx}: VALID")
        else:
            invalid_count += 1
            print(f"❌ Assignment {idx}: INVALID - no valid m2 found")
            print(f"   x={x}, m1={m1}, pred1={pred1:.6f}")
            print(f"   Tried {len(m2_candidates)} m2 candidates, none satisfied gap >= {args.gap}")
    
    print("\n" + "="*60)
    print(f"Summary:")
    print(f"  Valid assignments:   {valid_count}/{len(assignments)}")
    print(f"  Invalid assignments: {invalid_count}/{len(assignments)}")
    if valid_count == len(assignments):
        print("✅ All assignments are VALID!")
    else:
        print(f"⚠️  {invalid_count} assignments failed validation")


if __name__ == "__main__":
    main()
