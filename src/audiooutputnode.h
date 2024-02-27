#ifndef AUDIOOUTPUTNODE_H
#define AUDIOOUTPUTNODE_H

#include <QtNodes/NodeDelegateModel>
#include <QtWidgets/QPushButton>
#include <QtCore/QObject>
#include <portaudio/portaudio.h>

#include "bufferdata.h"

class AudioOutputNode : public QtNodes::NodeDelegateModel {
    Q_OBJECT

public:
    AudioOutputNode();
    ~AudioOutputNode();

    QString caption() const override;
    QString name() const override;
    bool captionVisible() const override;

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override;
    void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex portIndex) override;

    QWidget* embeddedWidget() override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override;

private:
    std::shared_ptr<BufferData> m_bufferData;
    QPushButton* m_playButton;

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
