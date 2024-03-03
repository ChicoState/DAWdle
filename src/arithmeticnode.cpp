// arithmeticnode.cpp
#include "arithmeticnode.h"
#include <QtGui/QDoubleValidator>

ArithmeticNode::ArithmeticNode(const QString& operationName) : m_operationName(operationName) {
    m_bufferData = std::make_shared<BufferData>();
    m_bufferData->setAll(0.0f);
}

QString ArithmeticNode::caption() const {
    return m_operationName + " Node";
}

QString ArithmeticNode::name() const {
    return m_operationName + "Node";
}

bool ArithmeticNode::captionVisible() const {
    return true;
}

unsigned int ArithmeticNode::nPorts(QtNodes::PortType portType) const {
    if (portType == QtNodes::PortType::Out) return 1;
    return 2;  // Two input ports
}

QtNodes::NodeDataType ArithmeticNode::dataType(QtNodes::PortType, QtNodes::PortIndex) const {
    return BufferData{}.type();
}

void ArithmeticNode::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex portIndex) {
    if (auto inputData = std::dynamic_pointer_cast<BufferData>(data)) {
        m_bufferData->m_buffer[portIndex] = inputData->m_buffer[0];

        // Perform the operation
        float result = performOperation(m_bufferData->m_buffer[0], m_bufferData->m_buffer[1]);

        m_bufferData->setAll(result);
        Q_EMIT dataUpdated(0);
    }
}

std::shared_ptr<QtNodes::NodeData> ArithmeticNode::outData(QtNodes::PortIndex) {
    return m_bufferData;
}

QWidget* ArithmeticNode::embeddedWidget() {
    return nullptr;
}

AddNode::AddNode() : ArithmeticNode("Addition") {}

float AddNode::performOperation(float operand1, float operand2) const {
    if (operand1 && !operand2){
        return operand1;
    } else if (!operand1 && operand2){
        return operand2;
    } else {
        return operand1 + operand2;
    }
}

SubtractNode::SubtractNode() : ArithmeticNode("Subtraction") {}

float SubtractNode::performOperation(float operand1, float operand2) const {
    if (operand1 && !operand2){
        return operand1;
    } else if (!operand1 && operand2){
        return operand2;
    } else {
        return float(operand1 - operand2);
    }
}

MultiplyNode::MultiplyNode() : ArithmeticNode("Multiply") {}

float MultiplyNode::performOperation(float operand1, float operand2) const {
    if (operand1 && !operand2){
        return operand1;
    } else if (!operand1 && operand2){
        return operand2;
    } else {
        return operand1 * operand2;
    }
}

DivideNode::DivideNode() : ArithmeticNode("Divide") {}

float DivideNode::performOperation(float operand1, float operand2) const {
    if (operand1 && !operand2){
        return operand1;
    } else if (!operand1 && operand2){
        return operand2;
    } else {
        return operand1 / operand2;
    }
}
