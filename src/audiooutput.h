#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <QtNodes/NodeDelegateModel>
#include <QtWidgets/QPushButton>
#include <QtCore/QObject>
#include <portaudio/portaudio.h>

#include "bufferdata.h"

extern float sampleRate;

class AudioOutput : public QtNodes::NodeDelegateModel {
    Q_OBJECT

public:
    AudioOutput();
    ~AudioOutput();

    QString caption() const override;
    QString name() const override;
    bool captionVisible() const override;

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override;
    void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex portIndex) override;
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override;

    QWidget* embeddedWidget() override;

private:
    std::shared_ptr<BufferData> m_bufferData[2];
    QPushButton* m_playButton;
    int m_currentBuffer;
    bool m_needsAnotherBuffer;

    PaStream* m_paStream;

    static int paCallback(const void* inputBuffer, void* outputBuffer,
                          unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags, void* userData);

    void initializePortAudio();
    void cleanupPortAudio();

private slots:
    void playButtonClicked();
};

#endif
