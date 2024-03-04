#include "arithmeticnode.h"
#include <QDebug>

ArithmeticNode::ArithmeticNode(const char* name) : m_name{name} {
    // Initialize buffer and result buffer
    m_inBuffer1 = std::make_shared<BufferData>();
    m_inBuffer2 = std::make_shared<BufferData>();
    m_outBuffer = std::make_shared<BufferData>();

    // Assuming setAll is a function in BufferData to set all elements to a given value
    m_inBuffer1->setAll(0.0f);
    m_inBuffer2->setAll(0.0f);
    m_outBuffer->setAll(0.0f);
}

QString ArithmeticNode::caption() const {
    return m_name + QStringLiteral(" Node");
}

QString ArithmeticNode::name() const {
    return m_name + QStringLiteral("Node");
}

bool ArithmeticNode::captionVisible() const {
    return true;
}

unsigned int ArithmeticNode::nPorts(QtNodes::PortType portType) const {
    if (portType == QtNodes::PortType::In) return 2;
    else if (portType == QtNodes::PortType::Out) return 1;
    return 0;
}

QtNodes::NodeDataType ArithmeticNode::dataType(QtNodes::PortType, QtNodes::PortIndex) const {
    return BufferData{}.type();
}

void ArithmeticNode::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex portIndex) {
    auto bufferData = std::dynamic_pointer_cast<BufferData>(data);

    if (portIndex == 0) {
        for(size_t i = 0; i < BUFFERSIZE; i++) {
            m_inBuffer1->m_buffer[i] = bufferData->m_buffer[i];
        }
    }
    else if (portIndex == 1) {
        for(size_t i = 0; i < BUFFERSIZE; i++) {
            m_inBuffer2->m_buffer[i] = bufferData->m_buffer[i];
        }
    }

    compute();
    Q_EMIT dataUpdated(0);
}

std::shared_ptr<QtNodes::NodeData> ArithmeticNode::outData(QtNodes::PortIndex) {
    return m_outBuffer;
}

QWidget* ArithmeticNode::embeddedWidget() {
    return nullptr;
}

AdditionNode::AdditionNode() : ArithmeticNode("Addition") {}
SubtractionNode::SubtractionNode() : ArithmeticNode("Subtraction") {}
MultiplicationNode::MultiplicationNode() : ArithmeticNode("Multiplication") {}
DivisionNode::DivisionNode() : ArithmeticNode("Division") {}

void AdditionNode::compute() {
    for (int i = 0; i < BUFFERSIZE; i++){
        m_outBuffer->m_buffer[i] = (m_inBuffer1->m_buffer[i] + m_inBuffer2->m_buffer[i]);
    }
}

void SubtractionNode::compute() {
    for (int i = 0; i < BUFFERSIZE; i++){
        m_outBuffer->m_buffer[i] = (m_inBuffer1->m_buffer[i] + m_inBuffer2->m_buffer[i]);
    }
}

void MultiplicationNode::compute() {
    for (int i = 0; i < BUFFERSIZE; i++){
        m_outBuffer->m_buffer[i] = (m_inBuffer1->m_buffer[i] + m_inBuffer2->m_buffer[i]);
    }
}

void DivisionNode::compute() {
    for (int i = 0; i < BUFFERSIZE; i++){
        m_outBuffer->m_buffer[i] = (m_inBuffer1->m_buffer[i] + m_inBuffer2->m_buffer[i]);
    }
}
