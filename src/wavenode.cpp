#include "wavenode.h"
#include <QtGui/QDoubleValidator>
#include <QDebug>
#include <random>

WaveNode::WaveNode(const char* name) : m_name{ name } {
    m_inBuffer = std::make_shared<BufferData>();
    m_inBuffer->setAll(0.0f);

    m_outBuffer = std::make_shared<BufferData>();
    m_outBuffer->setAll(0.0f);
}

QString WaveNode::caption() const {
    return m_name + QStringLiteral(" Wave Oscillator");
}

QString WaveNode::name() const {
    return m_name + QStringLiteral("WaveOscillator");
}

bool WaveNode::captionVisible() const {
    return true;
}

unsigned int WaveNode::nPorts(QtNodes::PortType) const {
    return 1;
}

QtNodes::NodeDataType WaveNode::dataType(QtNodes::PortType, QtNodes::PortIndex) const {
    return BufferData{}.type();
}

void WaveNode::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex) {
    if (auto inputData = std::dynamic_pointer_cast<BufferData>(data)) {
        for(size_t i = 0; i < BUFFERSIZE; i++) {
            m_inBuffer->m_buffer[i] = inputData->m_buffer[i];
        }
        generateWave();
        Q_EMIT dataUpdated(0);
    }
}

std::shared_ptr<QtNodes::NodeData> WaveNode::outData(QtNodes::PortIndex) {
    return m_outBuffer;
}

QWidget* WaveNode::embeddedWidget() {
    return nullptr;
}

// individual derived class stuff:
SineWave::SineWave() : WaveNode{"Sine"} {}
SawWave::SawWave() : WaveNode{"Saw"} {}
SquareWave::SquareWave() : WaveNode{"Square"} {}
TriangleWave::TriangleWave() : WaveNode{"Triangle"} {}
NoiseWave::NoiseWave() : WaveNode{"Noise"} {}

void SineWave::generateWave() {
    for (size_t i = 0; i < BUFFERSIZE; i++) {
        float frequency = m_inBuffer->m_buffer[i];
        m_outBuffer->m_buffer[i] = sinf(m_timesteps[frequency]);
        m_timesteps[frequency] += 2.0f * M_PI * frequency / static_cast<float>(SAMPLERATE);
        if(m_timesteps[frequency] >= 2.0f * M_PI) m_timesteps[frequency] -= 2.0f * M_PI;
    }
}

void SawWave::generateWave() {
    for (size_t i = 0; i < BUFFERSIZE; i++) {
        float frequency = m_inBuffer->m_buffer[i];
        m_outBuffer->m_buffer[i] = 2.0f * (m_timesteps[frequency] / (2.0f * M_PI)) - 1.0f;
        m_timesteps[frequency] += 2.0f * M_PI * frequency / static_cast<float>(SAMPLERATE);
        if(m_timesteps[frequency] >= 2.0f * M_PI) m_timesteps[frequency] -= 2.0f * M_PI;
    }
}

void SquareWave::generateWave() {
    for (size_t i = 0; i < BUFFERSIZE; i++) {
        float frequency = m_inBuffer->m_buffer[i];
        m_outBuffer->m_buffer[i] = sinf(m_timesteps[frequency]) > 0 ? 1.0f : -1.0f;
        m_timesteps[frequency] += 2.0f * M_PI * frequency / static_cast<float>(SAMPLERATE);
        if(m_timesteps[frequency] >= 2.0f * M_PI) m_timesteps[frequency] -= 2.0f * M_PI;
    }
}

void TriangleWave::generateWave() {
    for (size_t i = 0; i < BUFFERSIZE; i++) {
        float frequency = m_inBuffer->m_buffer[i];
        m_outBuffer->m_buffer[i] = 2.0f * abs(2.0f * (m_timesteps[frequency] / (2.0f * M_PI)) - 1.0f) - 1.0f;
        m_timesteps[frequency] += 2.0f * M_PI * frequency / static_cast<float>(SAMPLERATE);
        if(m_timesteps[frequency] >= 2.0f * M_PI) m_timesteps[frequency] -= 2.0f * M_PI;
    }
}

void NoiseWave::generateWave() {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    for (size_t i = 0; i < BUFFERSIZE; i++) {
        m_outBuffer->m_buffer[i] = dis(gen);
    }
}
