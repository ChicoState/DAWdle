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
        std::map<NodeHeader*, U32> nodeIndices;
        U32 nodeIndex = 0;
        for (NodeHeader* node = graph.nodesFirst; node; node = node->next) {
            outFile.write(reinterpret_cast<char*>(&node->type), sizeof(node->type));
            outFile.write(reinterpret_cast<char*>(&node->offset), sizeof(node->offset));
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