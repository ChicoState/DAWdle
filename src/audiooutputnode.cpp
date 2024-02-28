#include "audiooutputnode.h"

AudioOutputNode::AudioOutputNode() {
    m_bufferData = std::make_shared<BufferData>();
    m_bufferData->setAll(0.0f);

    m_playButton = new QPushButton("Play");
    connect(m_playButton, &QPushButton::clicked, this, &AudioOutputNode::playButtonClicked);

    initializePortAudio();
}

AudioOutputNode::~AudioOutputNode() {
    cleanupPortAudio();
}

QString AudioOutputNode::caption() const {
    return QStringLiteral("Audio Output");
}

QString AudioOutputNode::name() const {
    return QStringLiteral("AudioOutputNode");
}

bool AudioOutputNode::captionVisible() const {
    return true;
}

unsigned int AudioOutputNode::nPorts(QtNodes::PortType portType) const {
    if (portType == QtNodes::PortType::In) return 1;
    return 0;
}

QtNodes::NodeDataType AudioOutputNode::dataType(QtNodes::PortType, QtNodes::PortIndex) const {
    return BufferData{}.type();
}

void AudioOutputNode::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex) {
    if (auto inputData = std::dynamic_pointer_cast<BufferData>(data)) {
        for(size_t i = 0; i < BUFFERSIZE; i++) {
            m_bufferData->m_buffer[i] = inputData->m_buffer[i];
        }
    }
}

std::shared_ptr<QtNodes::NodeData> AudioOutputNode::outData(QtNodes::PortIndex) {
    return m_bufferData;
}

QWidget* AudioOutputNode::embeddedWidget() {
    return m_playButton;
}

void AudioOutputNode::playButtonClicked() {
    //Pa_StartStream(m_paStream);  // seg faults
}

int AudioOutputNode::paCallback(const void* inputBuffer, void* outputBuffer,
                                unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo,
                                PaStreamCallbackFlags statusFlags, void* userData) {
    AudioOutputNode* audioOutputNode = static_cast<AudioOutputNode*>(userData);
    float* buffer = audioOutputNode->m_bufferData->m_buffer;
    memcpy(outputBuffer, buffer, framesPerBuffer * sizeof(float));

    return paContinue;
}

void AudioOutputNode::initializePortAudio() {
    Pa_Initialize();

    PaStreamParameters outputParameters;
    outputParameters.device = Pa_GetDefaultOutputDevice();
    outputParameters.channelCount = 1;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.hostApiSpecificStreamInfo = nullptr;

    Pa_OpenStream(
        &m_paStream,
        nullptr,
        &outputParameters,
        44100,
        paFramesPerBufferUnspecified,
        paNoFlag,
        &AudioOutputNode::paCallback,
        this
    );

    //Pa_SetStreamFinishedCallback(m_paStream, nullptr);
}

void AudioOutputNode::cleanupPortAudio() {
    //Pa_StopStream(m_paStream);
    //Pa_CloseStream(m_paStream);
    //Pa_Terminate();
}
