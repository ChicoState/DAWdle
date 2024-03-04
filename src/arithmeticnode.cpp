#include "arithmeticnode.h"
#include <QDebug>

ArithmeticNode::ArithmeticNode() {
    // Initialize buffer and result buffer
    _buffer1 = std::make_shared<BufferData>();
    _buffer2 = std::make_shared<BufferData>();
    _resultBuffer = std::make_shared<BufferData>();

    // Assuming setAll is a function in BufferData to set all elements to a given value
    _buffer1->setAll(0.0f);
    _buffer2->setAll(0.0f);
    _resultBuffer->setAll(0.0f);
}

ArithmeticNode::~ArithmeticNode() {
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

    if (!bufferData) {
        Q_EMIT dataInvalidated(portIndex);
        return;
    }

    if (portIndex == 0) {
        _buffer1 = bufferData;
    } else if (portIndex == 1) {
        _buffer2 = bufferData;
    }

    compute();
}

std::shared_ptr<QtNodes::NodeData> ArithmeticNode::outData(QtNodes::PortIndex) {
    return _resultBuffer;
}

QWidget* ArithmeticNode::embeddedWidget() {
    return nullptr;
}
