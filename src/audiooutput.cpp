#include "audiooutput.h"
#include "audioinput.h"

AudioOutput::AudioOutput() {
    m_bufferData[0] = std::make_shared<BufferData>();
    m_bufferData[0]->setAll(0.0f);

    m_bufferData[1] = std::make_shared<BufferData>();
    m_bufferData[1]->setAll(0.0f);

    m_needsAnotherBuffer = true;
    m_currentBuffer = 1;

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
            m_bufferData[abs(m_currentBuffer - 1)]->m_buffer[i] = inputData->m_buffer[i];
        }
    }
    if(m_needsAnotherBuffer) {
        if(m_currentBuffer == 0) {
            m_needsAnotherBuffer = false;
        }
        else {
            m_currentBuffer = 0;
            AudioInput::refreshStreams();
        }
    }
}

std::shared_ptr<QtNodes::NodeData> AudioOutput::outData(QtNodes::PortIndex) {
    return nullptr;
}

QWidget* AudioOutput::embeddedWidget() {
    return m_playButton;
}

void AudioOutput::playButtonClicked() {
    Pa_StartStream(m_paStream);
}

int AudioOutput::paCallback(const void* inputBuffer, void* outputBuffer,
                                unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo,
                                PaStreamCallbackFlags statusFlags, void* userData) {
    static unsigned long framesElapsed = 0;
    AudioOutput* audioOutputNode = static_cast<AudioOutput*>(userData);
    for(size_t i = 0; i < framesPerBuffer; i++) {
        if(framesElapsed == BUFFERSIZE) {
            framesElapsed = 0;
            audioOutputNode->m_currentBuffer = abs(audioOutputNode->m_currentBuffer - 1);
            AudioInput::refreshStreams();
        }
        static_cast<float*>(outputBuffer)[i] = audioOutputNode->m_bufferData[audioOutputNode->m_currentBuffer]->m_buffer[framesElapsed];
        framesElapsed++;
    }
    return paContinue;
}

void AudioOutput::initializePortAudio() {
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
        SAMPLERATE,
        paFramesPerBufferUnspecified,
        paNoFlag,
        &AudioOutput::paCallback,
        this
    );

    Pa_SetStreamFinishedCallback(m_paStream, nullptr);
}

void AudioOutput::cleanupPortAudio() {
    Pa_StopStream(m_paStream);
    Pa_CloseStream(m_paStream);
    Pa_Terminate();
}
