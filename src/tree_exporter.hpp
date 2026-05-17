#ifndef TREE_EXPORTER_HPP
#define TREE_EXPORTER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include "utils.hpp" // <-- assuming Node struct/class is declared here or elsewhere

#include <sstream> // Required for std::ostringstream

class DecisionTreeExporter
{
public:
    static void saveAsDot(TreeNode *root, const std::string &filename)
    {
        std::ofstream out(filename);
        if (!out)
        {
            std::cerr << "Error: Could not open file " << filename << " for writing.\n";
            return;
        }

        out << "digraph DecisionTree {\n";
        out << "    node [shape=box, style=rounded, color=blue, fontname=\"Helvetica\"];\n";
        writeDot(root, out);
        out << "}\n";
        out.close();
    }

private:
    static void writeDot(TreeNode *node, std::ofstream &out)
    {
        if (!node)
            return;

        std::ostringstream node_id;
        node_id << "node" << node;

        if (node->is_leaf)
        {
            out << "    " << node_id.str()
                << " [label=\"" << node->upLeaf << "," << node->downLeaf
                << "\", shape=ellipse, style=filled, fillcolor=lightgreen];\n";
        }
        else
        {
            out << "    " << node_id.str()
                << " [label=\"Feature " << node->feature
                << " <= " << node->split_condition << "\"];\n";
        }

        if (node->yes)
        {
            std::ostringstream yes_child_id;
            yes_child_id << "node" << node->yes;
            out << "    \"" << node_id.str() << "\" -> \"" << yes_child_id.str() << "\" [label=\"True\"];\n";
            writeDot(node->yes, out);
        }

        if (node->no)
        {
            std::ostringstream no_child_id;
            no_child_id << "node" << node->no;
            out << "    \"" << node_id.str() << "\" -> \"" << no_child_id.str() << "\" [label=\"False\"];\n";
            writeDot(node->no, out);
        }
    }
};

#endif // TREE_EXPORTER_HPP
