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
    "Return the number of 0s in the bitstring."
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
    feature_bitsrings = {}
    curr_index = 0
    for fid in features:
        if fid == sensitive:
            feature_bitsrings[fid] = m
        else:
            feature_bitsrings[fid] = x[curr_index: curr_index + splits_map[fid]]
            curr_index += splits_map[fid]
    for fid in features:
        index = bitstring_to_indices(feature_bitsrings[fid])
        if index == 0:
            vec[fid] = float('-inf')  # Less than the smallest split
        else:
            vec[fid] = feature_splits[fid][index - 1]

    return np.array(vec, dtype=float)


def parse_assignments(assignments_path):
    """
    Parse assignments.txt format (assignments first, then metadata).
    Returns: features, splits_map, sensitive, assignments
    """
    sensitive = None
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
        if len(parts) == 3 and all(len(p) > 0 for p in parts):
            assignments.append(tuple(p.strip() for p in parts))
            i += 1
        else:
            break

    # The rest are metadata lines
    while i < len(lines):
        meta_lines.append(lines[i])
        i += 1

    for line in meta_lines:
        if line.startswith("Sensitive features:"):
            sensitive = int(line.split(":")[1].strip())
        elif " " in line:
            parts = line.split()
            if len(parts) == 2 and parts[0].isdigit() and parts[1].isdigit():
                features.append(int(parts[0]))
                splits_map[int(parts[0])] = int(parts[1])

    features = sorted(features)
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
    parser = argparse.ArgumentParser(description="Evaluate XGBoost JSON dump on (x,m1) and (x,m2) assignments.")
    parser.add_argument("--model", required=True, help="Path to model JSON dump.")
    parser.add_argument("--assignments", required=True, help="Path to assignments.txt.")
    parser.add_argument("--output", default="results.csv", help="Output CSV file name.")
    parser.add_argument("--gap", type=float, default=0.0, help="Optional gap to add to predictions.")
    args = parser.parse_args()


    trees = load_trees(args.model)
    feature_splits = extract_feature_splits_from_model(args.model)
    features, splits_map, sensitive, assignments = parse_assignments(args.assignments)
    for fid in features:
        print(f"Feature {fid} splits: {feature_splits.get(fid, [])}")
    # print(f"Loaded {len(trees)} trees.")
    # print(f"Sensitive feature: {sensitive}")
    # print(f"Features: {features}")
    # print("No. of splits: ", splits_map)
    # print(f"Assignments: {len(assignments)}")
    results = []
    # print the feature splits map
    # print(f"Feature splits: {feature_splits}")
    check = True
    print(f"Gap condition: {args.gap}")
    print(len(assignments))
    for i, (x, m1, m2) in enumerate(assignments):
        v1 = assignment_to_feature_vector(x, m1, features, sensitive, feature_splits, splits_map)
        v2 = assignment_to_feature_vector(x, m2, features, sensitive, feature_splits, splits_map)
        # print("v1: ", v1)
        # print("v2: ", v2)
        pred1 = sum(tree(v1) for tree in trees)
        # for i in range(len(trees)):
        #     tree = trees[i]
        #     print(f"Tree {i} prediction for v1:")
        #     print(tree(v1))
        pred2 = sum(tree(v2) for tree in trees)
        # for i in range(len(trees)):
        #     tree = trees[i]
        #     print(f"Tree {i} prediction for v2:")
        #     print(tree(v2))
        # print(f"There are {len(trees)} trees.")
        # print(pred1)
        # print(pred2)
        results.append((i, pred1, pred2))
        if abs(pred1 - pred2) >= args.gap:
            print(abs(pred1 - pred2))
            pass
        else:
            print(f"Warning: Gap condition violated for assignment {i}: ")
            print(f"[{i}] x1 ->{pred1:.6f}, x2 ->{pred2:.6f}")
            check = False
        # if pred1 - pred2 < args.gap:
        #     print(f"Warning: Gap condition violated for assignment {i}: ")
        #     print(f"[{i}] ({x},{m1})->{v1}->{pred1:.6f}, ({x},{m2})->{v2}->{pred2:.6f}")
        #     print(f"pred1 - pred2 = {pred1 - pred2:.6f} < {args.gap}")
        #     check = False

    if check:
        print("✅ All assignments satisfy the gap condition.")
            


if __name__ == "__main__":
    main()