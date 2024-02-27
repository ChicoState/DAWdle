#include "sinewavenode.h"
#include <QtGui/QDoubleValidator>

SineWaveNode::SineWaveNode() {
    m_bufferData = std::make_shared<BufferData>();
    m_bufferData->setAll(0.0f);
}

QString SineWaveNode::caption() const {
    return QStringLiteral("Sine Wave Oscillator");
}

QString SineWaveNode::name() const {
    return QStringLiteral("SineWaveNode");
}

bool SineWaveNode::captionVisible() const {
    return true;
}

unsigned int SineWaveNode::nPorts(QtNodes::PortType) const {
    return 1;
}

QtNodes::NodeDataType SineWaveNode::dataType(QtNodes::PortType, QtNodes::PortIndex) const {
    return BufferData{}.type();
}

void SineWaveNode::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex) {
    if (auto inputData = std::dynamic_pointer_cast<BufferData>(data)) {
        for(size_t i = 0; i < BUFFERSIZE; i++) {
            m_bufferData->m_buffer[i] = inputData->m_buffer[i];
        }
        generateWave();
    }
}

std::shared_ptr<QtNodes::NodeData> SineWaveNode::outData(QtNodes::PortIndex) {
    return m_bufferData;
}

QWidget* SineWaveNode::embeddedWidget() {
    return nullptr;
}

void SineWaveNode::generateWave() {
    for (size_t i = 0; i < BUFFERSIZE; i++) {
        m_bufferData->m_buffer[i] = std::sin(2.0f * M_PI * m_bufferData->m_buffer[i] / static_cast<float>(BUFFERSIZE));
    }
    Q_EMIT dataUpdated(0);
}
