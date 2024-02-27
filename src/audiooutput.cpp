#include "audiooutput.h"

AudioOutput::AudioOutput() {
    m_bufferData = std::make_shared<BufferData>();
    m_bufferData->setAll(0.0f);

    m_playButton = new QPushButton("Play");
    connect(m_playButton, &QPushButton::clicked, this, &AudioOutput::playButtonClicked);

    initializePortAudio();
}

AudioOutput::~AudioOutput() {
    cleanupPortAudio();
}

QString AudioOutput::caption() const {
    return QStringLiteral("Audio Output");
}

QString AudioOutput::name() const {
    return QStringLiteral("AudioOutput");
}

bool AudioOutput::captionVisible() const {
    return true;
}

unsigned int AudioOutput::nPorts(QtNodes::PortType portType) const {
    if (portType == QtNodes::PortType::In) return 1;
    return 0;
}

QtNodes::NodeDataType AudioOutput::dataType(QtNodes::PortType, QtNodes::PortIndex) const {
    return BufferData{}.type();
}

void AudioOutput::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex) {
    if (auto inputData = std::dynamic_pointer_cast<BufferData>(data)) {
        for(size_t i = 0; i < BUFFERSIZE; i++) {
            m_bufferData->m_buffer[i] = inputData->m_buffer[i];
        }
    }
}

std::shared_ptr<QtNodes::NodeData> AudioOutput::outData(QtNodes::PortIndex) {
    return m_bufferData;
}

QWidget* AudioOutput::embeddedWidget() {
    return m_playButton;
}

void AudioOutput::playButtonClicked() {
    //Pa_StartStream(m_paStream);  // seg faults
}

int AudioOutput::paCallback(const void* inputBuffer, void* outputBuffer,
                                unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo,
                                PaStreamCallbackFlags statusFlags, void* userData) {
    AudioOutput* audioOutputNode = static_cast<AudioOutput*>(userData);
    float* buffer = audioOutputNode->m_bufferData->m_buffer;
    memcpy(outputBuffer, buffer, framesPerBuffer * sizeof(float));

    return paContinue;
}

void AudioOutput::initializePortAudio() {
    Pa_Initialize();

    PaStreamParameters outputParameters;
    outputParameters.device = Pa_GetDefaultOutputDevice();
    outputParameters.channelCount = 1;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = nullptr;

    Pa_OpenStream(
        &m_paStream,
        nullptr,
        &outputParameters,
        44100,
        paFramesPerBufferUnspecified,
        paNoFlag,
        &AudioOutput::paCallback,
        this
    );

    //Pa_SetStreamFinishedCallback(m_paStream, nullptr);
}

void AudioOutput::cleanupPortAudio() {
    Pa_StopStream(m_paStream);
    Pa_CloseStream(m_paStream);
    Pa_Terminate();
}
