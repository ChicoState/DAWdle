#include "audiooutput.h"
#include "audioinput.h"
#include "mainwindow.h"
#include "boxlayoutwrapper.h"
#include "qtimer.h"
#include "qmutex.h"

float sampleRate;
QMutex audioOutputMutex;
const uint32_t audioOutputBufferSize = BUFFERSIZE * 4;
float audioOutputBuffer[audioOutputBufferSize];
uint32_t audioOutputBufferReadPos;
uint32_t audioOutputBufferWritePos;

AudioOutput::AudioOutput() {
    m_bufferData[0] = std::make_shared<BufferData>();
    m_bufferData[0]->setAll(0.0f);

    m_bufferData[1] = std::make_shared<BufferData>();
    m_bufferData[1]->setAll(0.0f);

    m_needsAnotherBuffer = true;
    m_currentBuffer = 1;
    m_playing = false;

    QPushButton* playButton = new QPushButton("Play");
    connect(playButton, &QPushButton::clicked, this, &AudioOutput::playButtonClicked);

    QPushButton* pauseButton = new QPushButton("Pause");
    connect(pauseButton, &QPushButton::clicked, this, &AudioOutput::pauseButtonClicked);

    m_buttons = new BoxLayoutWrapper({ playButton, pauseButton });

    // Drillgon (2024-03-12): This should be replaced later
    m_timer = new QTimer();
    m_timer->setTimerType(Qt::TimerType::PreciseTimer);
    m_timer->setInterval(std::chrono::milliseconds(1));
    m_timer->callOnTimeout([this](){
        while (audioOutputBufferWritePos - audioOutputBufferReadPos < audioOutputBufferSize - BUFFERSIZE) {
            this->m_currentBuffer = (this->m_currentBuffer + 1) & 1;
            AudioInput::refreshStreams();

            audioOutputMutex.lock();
            memmove(audioOutputBuffer, audioOutputBuffer + audioOutputBufferReadPos, (audioOutputBufferWritePos - audioOutputBufferReadPos) * sizeof(audioOutputBuffer[0]));
            audioOutputBufferWritePos -= audioOutputBufferReadPos;
            audioOutputBufferReadPos = 0;
            memcpy(audioOutputBuffer + audioOutputBufferWritePos, this->m_bufferData[this->m_currentBuffer]->m_buffer, BUFFERSIZE * sizeof(audioOutputBuffer[0]));
            audioOutputBufferWritePos += BUFFERSIZE;
            audioOutputMutex.unlock();
        }
    });
}

AudioOutput::~AudioOutput() {
    m_timer->stop();
    delete m_timer;
    delete m_buttons;
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
    } else {
        m_bufferData[0]->setAll(0.0F);
        m_bufferData[1]->setAll(0.0F);
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
    return m_buttons;
}

void AudioOutput::playButtonClicked() {
    if (m_playing) return;
    m_playing = true;
    Pa_StartStream(MainWindow::getStream());
    m_timer->start();
}

void AudioOutput::pauseButtonClicked() {
    if (!m_playing) return;
    m_playing = false;
    Pa_StopStream(MainWindow::getStream());
    m_timer->stop();
}

int AudioOutput::paCallback(const void* inputBuffer, void* outputBuffer,
                                unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo,
                                PaStreamCallbackFlags statusFlags, void* userData) {
    Q_UNUSED(inputBuffer);
    Q_UNUSED(timeInfo);
    Q_UNUSED(statusFlags);
    Q_UNUSED(userData);
    audioOutputMutex.lock();
    for(size_t i = 0; i < framesPerBuffer; i++) {
        if (audioOutputBufferReadPos < audioOutputBufferWritePos) {
            static_cast<float*>(outputBuffer)[i] = audioOutputBuffer[audioOutputBufferReadPos++];
        } else {
            static_cast<float*>(outputBuffer)[i] = 0.0F;
        }
    }
    audioOutputMutex.unlock();
    return paContinue;
}
