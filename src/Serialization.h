#include <fstream>
#include "Nodes.h"

Nodes::NodeHeader* createNodeByType(Nodes::NodeGraph& graph, Nodes::NodeType type, V2F32 pos) {
    using namespace Nodes;
    switch (type) {
    case NODE_TIME_IN:
        return &graph.create_node<NodeTimeIn>(pos).header;
    case NODE_SINE:
        return &graph.create_node<NodeSine>(pos).header;
    case NODE_CHANNEL_OUT:
        return &graph.create_node<NodeChannelOut>(pos).header;
    case NODE_MATH:
        return &graph.create_node<NodeMathOp>(pos).header;
    case NODE_OSCILLOSCOPE:
        return &graph.create_node<NodeOscilloscope>(pos).header;
    case NODE_SAMPLER:
        return &graph.create_node<NodeSampler>(pos).header;
    default:
        print("node cannot be loaded. It must be added to the switch statement in serialization.h, function createNodeByType()");
        break;
    }
}

namespace Serialization {
    void SaveNodeGraph(const Nodes::NodeGraph& graph, const std::string& filePath) {
        using namespace Nodes;
        std::ofstream outFile(filePath, std::ios::binary);
        if (!outFile.is_open()) {
            std::cerr << "Failed to open file for writing: " << filePath << std::endl;
            return;
        }
        for (NodeHeader* node = graph.nodesFirst; node; node = node->next) {
            outFile.write(reinterpret_cast<char*>(&node->type), sizeof(node->type));
            outFile.write(reinterpret_cast<char*>(&node->offset), sizeof(node->offset));
        }

        outFile.close();
    }
    void LoadNodeGraph(Nodes::NodeGraph& graph, const std::string& filePath) {
        using namespace Nodes;
        std::ifstream inFile(filePath, std::ios::binary);
        if (!inFile.is_open()) {
            std::cerr << "Failed to open file for reading: " << filePath << std::endl;
            return;
        }
        graph.destroy();
        while (inFile.peek() != EOF) {
            NodeType type;
            V2F32 pos;
            inFile.read(reinterpret_cast<char*>(&type), sizeof(type));
            inFile.read(reinterpret_cast<char*>(&pos), sizeof(pos));
            Node* node = alloc_node();
            node->header.type = type;
            node->header.offset = pos;
            createNodeByType(graph, type, pos);
        }

        inFile.close();
    }
}