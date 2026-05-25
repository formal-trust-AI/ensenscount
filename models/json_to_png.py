import json
import sys
from graphviz import Digraph

def draw_tree(tree, tree_idx):
    dot = Digraph(comment=f"Decision Tree {tree_idx}")
    
    def add_nodes_edges(node):
        node_id = f"{tree_idx}_{node['nodeid']}"
        if 'leaf' in node:
            label = f"Leaf: {node['leaf']}"
            dot.node(node_id, label, shape='box')
        else:
            label = f"[x{node['split']} < {node['split_condition']:.3f}]"
            dot.node(node_id, label)
            for child in node['children']:
                child_id = f"{tree_idx}_{child['nodeid']}"
                add_nodes_edges(child)
                edge_label = "Yes" if child['nodeid'] == node['yes'] else "No"
                dot.edge(node_id, child_id, label=edge_label)

    add_nodes_edges(tree)
    dot.render(filename=f"tree_{tree_idx}", format="png", cleanup=True)

def main():
    if len(sys.argv) != 2:
        print("Usage: python render_trees.py <path_to_json_file>")
        sys.exit(1)

    json_file_path = sys.argv[1]

    with open(json_file_path, "r") as f:
        trees = json.load(f)

    for idx, tree in enumerate(trees):
        draw_tree(tree, idx)

if __name__ == "__main__":
    main()
