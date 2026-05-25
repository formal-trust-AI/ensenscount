import re
import networkx as nx
import pydot
import os
import argparse

def parse_dot_file(dot_file_path):
    # Read .dot file and parse using pydot
    with open(dot_file_path, 'r') as file:
        dot_data = file.read()
    
    graph = pydot.graph_from_dot_data(dot_data)[0]  # Extract first graph
    nx_graph = nx.nx_pydot.from_pydot(graph)  # Convert to NetworkX graph
    
    return nx_graph, dot_data

def extract_constants(dot_data):
    # Extract constants from the DOT file
    constants = {}
    pattern = re.compile(r'"(x\d+_[0-9\.]+|0x[0-9A-Fa-f]+)" \[label = "([-0-9\.]+)"\];')
    for match in pattern.findall(dot_data):
        node_name, value = match
        constants[node_name] = float(value)
    return constants

def extract_labels(dot_data):
    # Extract human-readable names for hex IDs
    labels = {}
    
    # Extract labels from "rank = same" structures
    rank_pattern = re.compile(r'\{ rank = same; "([^"]+)"[^}]*\}')
    for match in rank_pattern.findall(dot_data):
        label = match.strip()  # The label (e.g., "x9_0.500000")
        
        # Extract all node IDs under this rank
        node_pattern = re.compile(rf'"{label}"\s*;\s*"([^"]+)"')
        for node_match in node_pattern.findall(dot_data):
            node_id = node_match.strip()  # The node ID (e.g., "0x51")
            labels[node_id] = label  # Map node ID to label
    
    return labels

def build_tree(graph, labels, constants, root='F0'):
    tree = {}
    
    def traverse(node, parent_dict):
        # Replace node ID with label if available
        readable_node = labels.get(node, constants.get(node, node))  # Prioritize x* names
        children = list(graph.successors(node))
        if children:
            parent_dict[readable_node] = {}
            for child in children:
                traverse(child, parent_dict[readable_node])
        else:
            parent_dict[readable_node] = None  # Leaf node
    
    traverse(root, tree)
    return tree

def print_tree(tree, constants, prefix="", is_last=True):
    # Pretty print the tree using ├──, └──, │
    last_key = list(tree.keys())[-1] if tree else None
    
    for idx, (node, subtree) in enumerate(tree.items()):
        value = constants.get(node, '')
        connector = "└── " if node == last_key else "├── "
        print(prefix + connector + f"{node}: {value}")
        
        if isinstance(subtree, dict) and subtree:
            new_prefix = prefix + ("    " if node == last_key else "│   ")
            print_tree(subtree, constants, new_prefix, node == last_key)

if __name__ == "__main__": 
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=str, help='Directory containing .dot files')
    parser.add_argument('--show_tree', action='store_true', default=False, help='Show the tree representation of the ADD')
    args = parser.parse_args()

    directory = args.directory
    for file in os.listdir(directory):
        if file.endswith(".dot"):
            dot_file = os.path.join(directory, file)
            print(f"\nProcessing file: {dot_file}")

            # Read file contents
            with open(dot_file, 'r') as file:
                dot_data = file.read()
            
            graph, dot_data = parse_dot_file(dot_file)
            constants = extract_constants(dot_data)
            labels = extract_labels(dot_data)
            
            # Output the number of nodes in the original ADD
            num_nodes = graph.number_of_nodes()
            print(f"Number of nodes in the ADD: {num_nodes}")
            
            if args.show_tree:
                tree = build_tree(graph, labels, constants)
                print("\nAlgebraic Decision Diagram (ADD) as a Readable Tree:")
                print_tree(tree, constants)