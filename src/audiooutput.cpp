#include "audiooutput.h"
#include "audioinput.h"
#include "qtimer.h"
#include "qmutex.h"
#include "util.h"

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
    return m_playButton;
}

void AudioOutput::playButtonClicked() {
    static bool started = false;
    static AudioOutput* audioOutputNode;
    if (started) {
        return;
    }
    started = true;
    audioOutputNode = this;
    AudioInput::refreshStreams();
    Pa_StartStream(m_paStream);
    // Drillgon (2024-03-12): This should be replaced later
    QTimer* timer = new QTimer();
    timer->setTimerType(Qt::TimerType::PreciseTimer);
    timer->setInterval(std::chrono::milliseconds(1));
    timer->callOnTimeout([](){
        while (audioOutputBufferWritePos - audioOutputBufferReadPos < audioOutputBufferSize - BUFFERSIZE) {
            audioOutputNode->m_currentBuffer = (audioOutputNode->m_currentBuffer + 1) & 1;
            AudioInput::refreshStreams();

            audioOutputMutex.lock();
            memmove(audioOutputBuffer, audioOutputBuffer + audioOutputBufferReadPos, (audioOutputBufferWritePos - audioOutputBufferReadPos) * sizeof(audioOutputBuffer[0]));
            audioOutputBufferWritePos -= audioOutputBufferReadPos;
            audioOutputBufferReadPos = 0;
            memcpy(audioOutputBuffer + audioOutputBufferWritePos, audioOutputNode->m_bufferData[audioOutputNode->m_currentBuffer]->m_buffer, BUFFERSIZE * sizeof(audioOutputBuffer[0]));
            audioOutputBufferWritePos += BUFFERSIZE;
            audioOutputMutex.unlock();
        }
    });
    timer->start();
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

void AudioOutput::initializePortAudio() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "Pa_Initialize failed with error: %s\n", Pa_GetErrorText(err));
        exit(EXIT_FAILURE);
    }
    static uint32_t apiRanking[]{
        /*paInDevelopment*/   0xFFFFFFFF,
        /*paDirectSound*/     10,
        /*paMME*/             11,
        /*paASIO*/            0,
        /*paSoundManager*/    10,
        /*paCoreAudio*/       10,
        /*dummy value*/       0xFFFFFFFF,
        /*paOSS*/             10,
        /*paALSA*/            10,
        /*paAL*/              10,
        /*paBeOS*/            10,
        /*paWDMKS*/           10,
        /*paJACK*/            10,
        /*paWASAPI*/          1,
        /*paAudioScienceHPI*/ 10,
        /*paAudioIO*/         10,
        /*paPulseAudio*/      10
    };


    const PaHostApiInfo* apiInfo = Pa_GetHostApiInfo(0);
    PaHostApiIndex hostAPIs = Pa_GetHostApiCount();
    for (PaHostApiIndex api = 1; api < hostAPIs; api++) {
        const PaHostApiInfo* checkAPIInfo = Pa_GetHostApiInfo(api);
        if (checkAPIInfo->type < ARRAY_COUNT(apiRanking) && apiRanking[checkAPIInfo->type] < apiRanking[apiInfo->type]) {
            apiInfo = checkAPIInfo;
        }
    }
    qDebug("DAWdle using API: %s", apiInfo->name);

    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(apiInfo->defaultOutputDevice);
    sampleRate = float(deviceInfo->defaultSampleRate);

    PaStreamParameters outputParameters;
    outputParameters.device = apiInfo->defaultOutputDevice;
    outputParameters.channelCount = 1;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.hostApiSpecificStreamInfo = nullptr;
    outputParameters.suggestedLatency = deviceInfo->defaultLowOutputLatency;

    err = Pa_OpenStream(
        &m_paStream,
        nullptr,
        &outputParameters,
        double(sampleRate),
        paFramesPerBufferUnspecified,
        paNoFlag,
        &AudioOutput::paCallback,
        this
        );
    if (err != paNoError) {
        fprintf(stderr, "Pa_OpenStream failed with error: %s\n", Pa_GetErrorText(err));
        exit(EXIT_FAILURE);
    }

    Pa_SetStreamFinishedCallback(m_paStream, nullptr);
}

void AudioOutput::cleanupPortAudio() {
    Pa_StopStream(m_paStream);
    Pa_CloseStream(m_paStream);
    Pa_Terminate();
}
