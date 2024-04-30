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

    std::vector<std::pair<NodeType, V2F32>> nodeBasicData;
    std::vector<std::tuple<U32, const char*, U32>> samplerData;
    std::vector<U32> inputCounts;
    std::vector<std::pair<U32, U32>> connectionIndices;

    std::map<NodeHeader*, U32> nodeIndices;
    U32 nodeIndex = 0;

    for (NodeHeader* node = graph.nodesFirst; node; node = node->next) {
        nodeBasicData.emplace_back(node->type, node->offset);
        nodeIndices[node] = nodeIndex++;

        if (node->type == NODE_SAMPLER) {
            NodeSampler& samplerNode = *reinterpret_cast<NodeSampler*>(node);
            NodeWidgetSamplerButton& button = *samplerNode.header.get_samplerbutton(0);
            U32 pathLength = strlen(button.path);
            samplerData.emplace_back(pathLength, button.path, pathLength);
        }

        std::vector<NodeWidgetInput*> inputs;
        for (NodeWidgetHeader* widget = node->widgetBegin; widget; widget = widget->next) {
            if (widget->type == NODE_WIDGET_INPUT) {
                inputs.push_back(reinterpret_cast<NodeWidgetInput*>(widget));
            }
        }
        inputCounts.push_back(inputs.size());

        for (auto* input : inputs) {
            U32 outputNodeIndex = INVALID_NODE_IDX;
            U32 outputWidgetIndex = INVALID_NODE_IDX;

            if (input->inputHandle.get()) {
                NodeHeader* connectedNode = input->inputHandle.get()->header.parent;
                outputNodeIndex = nodeIndices[connectedNode];
                outputWidgetIndex = 0;

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

    // write
    size_t connectionIndex = 0;
    for (size_t i = 0; i < nodeBasicData.size(); ++i) {
        const auto& [type, offset] = nodeBasicData[i];
        outFile.write(reinterpret_cast<const char*>(&type), sizeof(type));
        outFile.write(reinterpret_cast<const char*>(&offset), sizeof(offset));

        if (type == NODE_SAMPLER) {
            const auto& [pathLength, pathData, pathLengthRepeat] = samplerData[i];
            outFile.write(reinterpret_cast<const char*>(&pathLength), sizeof(pathLength));
            outFile.write(pathData, pathLength);
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