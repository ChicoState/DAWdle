#include "sinewave.h"
#include <QtGui/QDoubleValidator>
#include <QDebug>

SineWave::SineWave() {
    m_inBuffer = std::make_shared<BufferData>();
    m_inBuffer->setAll(0.0f);

    m_outBuffer = std::make_shared<BufferData>();
    m_outBuffer->setAll(0.0f);
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
            m_inBuffer->m_buffer[i] = inputData->m_buffer[i];
        }
        generateWave();
    }
}

std::shared_ptr<QtNodes::NodeData> SineWave::outData(QtNodes::PortIndex) {
    return m_outBuffer;
}

QWidget* SineWave::embeddedWidget() {
    return nullptr;
}

void SineWave::generateWave() {
    for (size_t i = 0; i < BUFFERSIZE; i++) {
        float frequency = m_inBuffer->m_buffer[i];
        m_outBuffer->m_buffer[i] = sinf(m_timesteps[frequency]);
        m_timesteps[frequency] += 2.0f * M_PI * frequency / static_cast<float>(SAMPLERATE);
        if(m_timesteps[frequency] >= 2.0f * M_PI) m_timesteps[frequency] -= 2.0f * M_PI;
    }
    Q_EMIT dataUpdated(0);
}
