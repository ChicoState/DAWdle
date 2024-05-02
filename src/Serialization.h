#include <fstream>
#include "Nodes.h"

namespace Nodes {
    NodeHeader* createNodeByType(NodeGraph& graph, NodeType type, V2F32 pos) {
        switch (type) {
#define X(enumName, typeName) case NODE_##enumName: return &graph.create_node<typeName>(pos).header;
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

        std::vector<std::pair<NodeType, V2F32>> nodeBasicData;
        std::vector<std::pair<U32, const char*>> samplerData;
        std::vector<U32> inputCounts;
        std::vector<std::pair<U32, U32>> connectionIndices;

        U32 currentSerializeIndex = 0;
        for (NodeHeader* node = graph.nodesFirst; node; node = node->next) {
            node->serializeIndex = currentSerializeIndex++;
            nodeBasicData.emplace_back(node->type, node->offset);
            if (node->type == NODE_SAMPLER) {
                NodeSampler& samplerNode = *reinterpret_cast<NodeSampler*>(node);
                NodeWidgetSamplerButton& button = *samplerNode.header.get_samplerbutton(0);
                U32 pathLength = strlen(button.path);
                samplerData.emplace_back(pathLength, button.path);
            }
        }
        for (NodeHeader* node = graph.nodesFirst; node; node = node->next) {
            U32 inputWidgetCount = 0;
            for (NodeWidgetHeader* widget = node->widgetBegin; widget; widget = widget->next) {
                inputWidgetCount += widget->type == NODE_WIDGET_INPUT;
            }
            inputCounts.push_back(inputWidgetCount);
            for (NodeWidgetHeader* widget = node->widgetBegin; widget; widget = widget->next) {
                if (widget->type == NODE_WIDGET_INPUT) {
                    NodeWidgetInput* input = reinterpret_cast<NodeWidgetInput*>(widget);
                    U32 outputNodeIndex = INVALID_NODE_IDX;
                    U32 outputWidgetIndex = 0;
                    if (input->inputHandle.get() && input->inputHandle.get()->header.parent) {
                        outputNodeIndex = input->inputHandle.get()->header.parent->serializeIndex;
                        NodeHeader* connectedNode = input->inputHandle.get()->header.parent;
                        for (NodeWidgetHeader* outputWidget = connectedNode->widgetBegin; outputWidget; outputWidget = outputWidget->next) {
                            if (outputWidget == &input->inputHandle.get()->header) {
                                break;
                            }
                            if (outputWidget->type == NODE_WIDGET_OUTPUT) {
                                outputWidgetIndex++;
                            }
                        }
                    }
                    connectionIndices.emplace_back(outputNodeIndex, outputWidgetIndex);
                }
            }
        }

        // write
        size_t connectionIndex = 0;
        size_t samplerIndex = 0;
        for (size_t i = 0; i < nodeBasicData.size(); ++i) {
            const auto& [type, offset] = nodeBasicData[i];
            outFile.write(reinterpret_cast<const char*>(&type), sizeof(type));
            outFile.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
            if (type == NODE_SAMPLER) {
                const auto& [pathLength, pathData] = samplerData[samplerIndex++];
                outFile.write(reinterpret_cast<const char*>(&pathLength), sizeof(pathLength));
                outFile.write(reinterpret_cast<const char*>(pathData), pathLength);
            }
            U32 inputCount = inputCounts[i];
            outFile.write(reinterpret_cast<const char*>(&inputCount), sizeof(inputCount));
            for (U32 j = 0; j < inputCount; ++j) {
                const auto& [outputNodeIndex, outputWidgetIndex] = connectionIndices[connectionIndex++];
                outFile.write(reinterpret_cast<const char*>(&outputNodeIndex), sizeof(outputNodeIndex));
                outFile.write(reinterpret_cast<const char*>(&outputWidgetIndex), sizeof(outputWidgetIndex));
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
        std::vector<std::vector<std::pair<U32, U32>>> connections;

        NodeType type;
        V2F32 pos;
        while (inFile.read(reinterpret_cast<char*>(&type), sizeof(type)) && inFile.read(reinterpret_cast<char*>(&pos), sizeof(pos))) {
            NodeHeader* node = createNodeByType(graph, type, pos);
            nodeHeaders.push_back(node);

            if (type == NODE_SAMPLER) {
                NodeSampler& samplerNode = *reinterpret_cast<NodeSampler*>(node);
                NodeWidgetSamplerButton& button = *samplerNode.header.get_samplerbutton(0);

                U32 pathLength;
                inFile.read(reinterpret_cast<char*>(&pathLength), sizeof(pathLength));
                inFile.read(reinterpret_cast<char*>(button.path), pathLength);
                button.path[pathLength] = '\0';
                button.loadFromFile();
            }

            U32 inputCount;
            inFile.read(reinterpret_cast<char*>(&inputCount), sizeof(inputCount));
            std::vector<std::pair<U32, U32>> nodeConnections;
            for (U32 i = 0; i < inputCount; i++) {
                U32 outputNodeIndex, outputWidgetIndex;
                inFile.read(reinterpret_cast<char*>(&outputNodeIndex), sizeof(outputNodeIndex));
                inFile.read(reinterpret_cast<char*>(&outputWidgetIndex), sizeof(outputWidgetIndex));
                nodeConnections.push_back({outputNodeIndex, outputWidgetIndex});
            }
            connections.push_back(nodeConnections);
        }

        for (size_t i = 0; i < nodeHeaders.size(); i++) {
            NodeHeader* node = nodeHeaders[i];
            const auto& nodeConnections = connections[i];
            for (size_t connIndex = 0; connIndex < nodeConnections.size(); connIndex++) {
                const auto& [outputNodeIndex, outputWidgetIndex] = nodeConnections[connIndex];
                if (outputNodeIndex != INVALID_NODE_IDX && outputNodeIndex < nodeHeaders.size()) {
                    NodeWidgetOutput* output = nodeHeaders[outputNodeIndex]->get_output(outputWidgetIndex);
                    NodeWidgetInput* input = node->get_input(connIndex);
                    if (input && output) {
                        input->connect(output);
                    }
                }
            }
        }

        inFile.close();
    }
}