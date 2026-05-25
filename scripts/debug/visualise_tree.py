import json

def print_tree(node, indent=""):
    """Recursively prints an ASCII representation of a decision tree."""
    if "leaf" in node:
        print(f"{indent}leaf = {node['leaf']}")
        return
    
    split = node["split"]
    cond = node["split_condition"]
    print(f"{indent}if feature[{split}] < {cond:.6f}:")
    print_tree(node["children"][0], indent + "  ")
    print(f"{indent}else:")
    print_tree(node["children"][1], indent + "  ")

def visualize_trees(json_file):
    """Loads an ensemble JSON and prints each tree in ASCII."""
    with open(json_file, "r") as f:
        trees = json.load(f)

    for i, tree in enumerate(trees):
        print(f"\n=== Tree {i} ===")
        print_tree(tree)

# Example usage:
# visualize_trees("model.json")
if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Visualize decision trees from an XGBoost JSON dump.")
    parser.add_argument("model", help="Path to the model JSON file.")
    args = parser.parse_args()
    visualize_trees(args.model)
