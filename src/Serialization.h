#include <fstream>
#include "Nodes.h"

namespace Nodes {

    template<NodeType T>
    NodeHeader* createNode(NodeGraph& graph, V2F32 pos) {
        static_assert(T != NODE_COUNT, "createNode function must be specialized for each NodeType.");
        return nullptr;
    }
#define X(enumName, typeName) \
template<> \
NodeHeader* createNode<NODE_##enumName>(NodeGraph& graph, V2F32 pos) { \
    return &graph.create_node<typeName>(pos).header; \
}
    NODES
#undef X
        NodeHeader* createNodeByType(NodeGraph& graph, NodeType type, V2F32 pos) {
        switch (type) {
#define X(enumName, typeName) case NODE_##enumName: return createNode<NODE_##enumName>(graph, pos);
            NODES
#undef X
        default:
            print("Node cannot be loaded. It must be defined in NODES macro.");
            return nullptr;
        }
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
        std::map<NodeHeader*, U32> nodeIndices;
        U32 nodeIndex = 0;
        for (NodeHeader* node = graph.nodesFirst; node; node = node->next) {
            outFile.write(reinterpret_cast<char*>(&node->type), sizeof(node->type));
            outFile.write(reinterpret_cast<char*>(&node->offset), sizeof(node->offset));
            
            if (node->type == NODE_SAMPLER) {
                NodeSampler& samplerNode = *reinterpret_cast<NodeSampler*>(node);
                NodeWidgetSamplerButton& button = *samplerNode.header.get_samplerbutton(0);
                U32 pathLength = strlen(button.path);
                outFile.write(reinterpret_cast<char*>(&pathLength), sizeof(pathLength));
                outFile.write(button.path, pathLength);
            }
            
            nodeIndices[node] = nodeIndex++;
            std::vector<NodeWidgetInput*> inputs;
            for (NodeWidgetHeader* widget = node->widgetBegin; widget; widget = widget->next) {
                if (widget->type == NODE_WIDGET_INPUT) {
                    inputs.push_back(reinterpret_cast<NodeWidgetInput*>(widget));
                }
            }
            U32 inputCount = inputs.size();
            outFile.write(reinterpret_cast<char*>(&inputCount), sizeof(inputCount));
            for (auto* input : inputs) {
                if (input->inputHandle.get()) {
                    NodeHeader* connectedNode = input->inputHandle.get()->header.parent;
                    U32 outputNodeIndex = nodeIndices[connectedNode];
                    U32 outputWidgetIndex = 0;
                    for (NodeWidgetHeader* outputWidget = connectedNode->widgetBegin; outputWidget; outputWidget = outputWidget->next) {
                        if (outputWidget == &input->inputHandle.get()->header) {
                            break;
                        }
                        if (outputWidget->type == NODE_WIDGET_OUTPUT) {
                            outputWidgetIndex++;
                        }
                    }
                    outFile.write(reinterpret_cast<char*>(&outputNodeIndex), sizeof(outputNodeIndex));
                    outFile.write(reinterpret_cast<char*>(&outputWidgetIndex), sizeof(outputWidgetIndex));
                }
                else {
                    U32 invalidIndex = INVALID_NODE_IDX;
                    outFile.write(reinterpret_cast<char*>(&invalidIndex), sizeof(invalidIndex));
                    outFile.write(reinterpret_cast<char*>(&invalidIndex), sizeof(invalidIndex));
                }
            }
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
        graph.delete_all_nodes();
        std::vector<NodeHeader*> nodeHeaders;
        while (inFile.peek() != EOF) {
            NodeType type;
            V2F32 pos;
            inFile.read(reinterpret_cast<char*>(&type), sizeof(type));
            inFile.read(reinterpret_cast<char*>(&pos), sizeof(pos));
            NodeHeader* node = createNodeByType(graph, type, pos);
            nodeHeaders.push_back(node);

            if (type == NODE_SAMPLER) {
                NodeSampler& samplerNode = *reinterpret_cast<NodeSampler*>(node);
                NodeWidgetSamplerButton& button = *samplerNode.header.get_samplerbutton(0);

                U32 pathLength;
                inFile.read(reinterpret_cast<char*>(&pathLength), sizeof(pathLength));
                inFile.read(button.path, pathLength);
                button.path[pathLength] = '\0';
                button.loadFromFile();
            }

            U32 inputCount;
            inFile.read(reinterpret_cast<char*>(&inputCount), sizeof(inputCount));
            for (U32 i = 0; i < inputCount; i++) {
                U32 outputNodeIndex, outputWidgetIndex;
                inFile.read(reinterpret_cast<char*>(&outputNodeIndex), sizeof(outputNodeIndex));
                inFile.read(reinterpret_cast<char*>(&outputWidgetIndex), sizeof(outputWidgetIndex));
                if (outputNodeIndex != INVALID_NODE_IDX) {
                    NodeHeader* outputNode = nodeHeaders[outputNodeIndex];
                    NodeWidgetOutput* output = outputNode->get_output(outputWidgetIndex);
                    NodeWidgetInput* input = node->get_input(i);
                    input->connect(output);
                }
            }
        }

        inFile.close();
    }
}