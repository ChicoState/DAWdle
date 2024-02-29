#include "sinewave.h"
#include <QtGui/QDoubleValidator>

SineWave::SineWave() {
    m_bufferData = std::make_shared<BufferData>();
    m_bufferData->setAll(0.0f);
}

QString SineWave::caption() const {
    return QStringLiteral("Sine Wave Oscillator");
}

QString SineWave::name() const {
    return QStringLiteral("SineWave");
}

bool SineWave::captionVisible() const {
    return true;
}

unsigned int SineWave::nPorts(QtNodes::PortType) const {
    return 1;
}

QtNodes::NodeDataType SineWave::dataType(QtNodes::PortType, QtNodes::PortIndex) const {
    return BufferData{}.type();
}

void SineWave::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex) {
    if (auto inputData = std::dynamic_pointer_cast<BufferData>(data)) {
        for(size_t i = 0; i < BUFFERSIZE; i++) {
            m_bufferData->m_buffer[i] = inputData->m_buffer[i];
        }
        generateWave();
    }
}

std::shared_ptr<QtNodes::NodeData> SineWave::outData(QtNodes::PortIndex) {
    return m_bufferData;
}

QWidget* SineWave::embeddedWidget() {
    return nullptr;
}

void SineWave::generateWave() {
    for (size_t i = 0; i < BUFFERSIZE; i++) {
        float frequency = m_bufferData->m_buffer[i];
        m_bufferData->m_buffer[i] = sinf(m_timesteps[frequency]);
        m_timesteps[frequency] += 2.0f * M_PI * frequency / static_cast<float>(SAMPLERATE);
    }
    Q_EMIT dataUpdated(0);
}
