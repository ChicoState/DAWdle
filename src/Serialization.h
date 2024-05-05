#include <fstream>
#include "Nodes.h"

const U32 SERIALIZE_FILE_MAGIC = 0x44574144;
const U32 CURRENT_SERIALIZE_VERSION = DRILL_LIB_MAKE_VERSION(1, 1, 0);

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
        std::vector<MathOp> mathOps;
        std::vector<Waveform> waveforms;
        std::vector<std::pair<char*, U32>> inputStrs;

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
            if (node->type == NODE_MATH) { 
                NodeMathOp& mathNode = *reinterpret_cast<NodeMathOp*>(node);
                mathOps.push_back(mathNode.op); 
            }
            if (node->type == NODE_WAVE) {
                NodeWave& waveNode = *reinterpret_cast<NodeWave*>(node);
                waveforms.push_back(waveNode.waveform);
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
                    UI::Box* inputBox = input->sliderHandle.unsafeBox;
                    inputStrs.emplace_back(inputBox->typedTextBuffer, inputBox->numTypedCharacters);
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
        outFile.write(reinterpret_cast<const char*>(&SERIALIZE_FILE_MAGIC), sizeof(SERIALIZE_FILE_MAGIC));
        outFile.write(reinterpret_cast<const char*>(&CURRENT_SERIALIZE_VERSION), sizeof(CURRENT_SERIALIZE_VERSION));
        size_t connectionIndex = 0;
        size_t inputStrIndex = 0;
        size_t samplerIndex = 0;
        size_t mathNodeIndex = 0;
        size_t waveNodeIndex = 0;
        for (size_t i = 0; i < nodeBasicData.size(); ++i) {
            const auto& [type, offset] = nodeBasicData[i];
            outFile.write(reinterpret_cast<const char*>(&type), sizeof(type));
            outFile.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
            U32 inputCount = inputCounts[i];
            outFile.write(reinterpret_cast<const char*>(&inputCount), sizeof(inputCount));
            for (U32 j = 0; j < inputCount; ++j) {
                const auto& [outputNodeIndex, outputWidgetIndex] = connectionIndices[connectionIndex++];
                outFile.write(reinterpret_cast<const char*>(&outputNodeIndex), sizeof(outputNodeIndex));
                outFile.write(reinterpret_cast<const char*>(&outputWidgetIndex), sizeof(outputWidgetIndex));

                const auto& [inputStr, inputStrLen] = inputStrs[inputStrIndex++];
                outFile.write(reinterpret_cast<const char*>(&inputStrLen), sizeof(inputStrLen));
                outFile.write(inputStr, inputStrLen);
            }
            if (type == NODE_SAMPLER) {
                const auto& [pathLength, pathData] = samplerData[samplerIndex++];
                outFile.write(reinterpret_cast<const char*>(&pathLength), sizeof(pathLength));
                outFile.write(reinterpret_cast<const char*>(pathData), pathLength);
            }
            if (type == NODE_MATH) {
                outFile.write(reinterpret_cast<const char*>(&mathOps[mathNodeIndex++]), sizeof(mathOps[mathNodeIndex++]));
            }
            if (type == NODE_WAVE) {
                outFile.write(reinterpret_cast<const char*>(&waveforms[waveNodeIndex++]), sizeof(waveforms[waveNodeIndex++]));
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
        U32 fileMagic, fileVersion;
        inFile.read(reinterpret_cast<char*>(&fileMagic), sizeof(fileMagic));
        inFile.read(reinterpret_cast<char*>(&fileVersion), sizeof(fileVersion));
        if (fileMagic != SERIALIZE_FILE_MAGIC) {
            MessageBox(nullptr, "Not a valid DAWdle file.", "File Error", MB_OK | MB_ICONERROR);
            return;
        }
        if (fileVersion != CURRENT_SERIALIZE_VERSION) {
            MessageBox(nullptr, "Incompatible file version.", "Version Error", MB_OK | MB_ICONERROR);
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

            U32 inputCount;
            inFile.read(reinterpret_cast<char*>(&inputCount), sizeof(inputCount));
            std::vector<std::pair<U32, U32>> nodeConnections;
            for (U32 i = 0; i < inputCount; i++) {
                U32 outputNodeIndex, outputWidgetIndex;
                inFile.read(reinterpret_cast<char*>(&outputNodeIndex), sizeof(outputNodeIndex));
                inFile.read(reinterpret_cast<char*>(&outputWidgetIndex), sizeof(outputWidgetIndex));
                nodeConnections.push_back({ outputNodeIndex, outputWidgetIndex });

                NodeWidgetInput* input = node->get_input(i);
                UI::Box* slider = input->sliderHandle.unsafeBox;
                inFile.read(reinterpret_cast<char*>(&slider->numTypedCharacters), sizeof(slider->numTypedCharacters));
                inFile.read(slider->typedTextBuffer, slider->numTypedCharacters);
                StrA parseStr{ slider->typedTextBuffer, slider->numTypedCharacters };
                tbrs::parse_program(&input->program, parseStr);
            }
            connections.push_back(nodeConnections);

            if (type == NODE_SAMPLER) {
                NodeSampler& samplerNode = *reinterpret_cast<NodeSampler*>(node);
                NodeWidgetSamplerButton& button = *samplerNode.header.get_samplerbutton(0);

                U32 pathLength;
                inFile.read(reinterpret_cast<char*>(&pathLength), sizeof(pathLength));
                inFile.read(reinterpret_cast<char*>(button.path), pathLength);
                button.path[pathLength] = '\0';
                button.loadFromFile();
            }
            if (type == NODE_MATH) {
                NodeMathOp& mathNode = *reinterpret_cast<NodeMathOp*>(node);
                MathOp op;
                inFile.read(reinterpret_cast<char*>(&op), sizeof(op));
                mathNode.set_op(op);
            }
            if (type == NODE_WAVE) {
                NodeWave& waveNode = *reinterpret_cast<NodeWave*>(node);
                Waveform waveform;
                inFile.read(reinterpret_cast<char*>(&waveform), sizeof(waveform));
                waveNode.set_waveform(waveform);
            }
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