#include "sampler.h"
#include <libsoundwave/AudioDecoder.h>
#include <libsoundwave/PostProcess.h>
#include <QFileDialog>

Sampler::Sampler() {
    m_bufferData = std::make_shared<BufferData>();
    m_bufferData->setAll(0.0f);

    m_button = new QPushButton("Load File");
    connect(m_button, &QPushButton::clicked, this, &Sampler::buttonPressed);
}

Sampler::~Sampler() {
    delete m_button;
}

QString Sampler::caption() const {
    return QStringLiteral("Sampler");
}

QString Sampler::name() const {
    return QStringLiteral("Sampler");
}

bool Sampler::captionVisible() const {
    return true;
}

unsigned int Sampler::nPorts(QtNodes::PortType portType) const {
    if(portType == QtNodes::PortType::Out) return 1;
    return 0;
}

QtNodes::NodeDataType Sampler::dataType(QtNodes::PortType, QtNodes::PortIndex) const {
    return BufferData{}.type();
}

void Sampler::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex) {}

std::shared_ptr<QtNodes::NodeData> Sampler::outData(QtNodes::PortIndex) {
    return m_bufferData;
}

QWidget* Sampler::embeddedWidget() {
    return m_button;
}

QJsonObject Sampler::save() const {
    QJsonObject modelJson = NodeDelegateModel::save();
    modelJson["path"] = QString(m_path.c_str());
    return modelJson;
}

void Sampler::load(const QJsonObject& json) {
    QJsonValue val = json["path"];

    if (val.isUndefined()) return;
    m_path = val.toString().toStdString();

    loadFromFile();
}

void Sampler::buttonPressed() {
    m_path = QFileDialog::getOpenFileName(
        nullptr,
        QStringLiteral("Open File"),
        QStringLiteral("C:/"),
        QStringLiteral("Sound Files (*.flac, *.ogg, *.opus, *.wav)")
    ).toStdString();
    loadFromFile();
}

void Sampler::loadFromFile() {
    if(!std::filesystem::exists(m_path)) return;
    soundwave::SoundwaveIO loader;
    auto data = std::make_unique<soundwave::AudioData>();
    loader.Load(data.get(), m_path);

    float curSampleRate = data->sampleRate;
    std::vector<float> intermediateBuffer;
    if(curSampleRate != sampleRate) {
        intermediateBuffer.reserve(data->samples.size() * sampleRate / curSampleRate);
        soundwave::hermite_resample(sampleRate / curSampleRate, data->samples, intermediateBuffer, data->samples.size());
    }
    else {
        intermediateBuffer = data->samples;
    }

    // cuz we're still only doing mono audio lol
    if(data->channelCount == 2) {
        m_audioData = std::vector<float>(intermediateBuffer.size() / 2);
        soundwave::StereoToMono(intermediateBuffer.data(), m_audioData.data(), intermediateBuffer.size());
    }
    else {
        m_audioData = intermediateBuffer;
    }

    for(size_t i = 0; i < BUFFERSIZE; i++) {
        m_bufferData->m_buffer[i] = m_audioData[m_bufPtr];
        m_bufPtr = (m_bufPtr + 1) % m_audioData.size();
    }
}

void Sampler::refreshStream() {
    if(m_audioData.size() == 0) return;
    for(size_t i = 0; i < BUFFERSIZE; i++) {
        m_bufferData->m_buffer[i] = m_audioData[m_bufPtr];
        m_bufPtr = (m_bufPtr + 1) % m_audioData.size();
    }
    Q_EMIT dataUpdated(0);
}
