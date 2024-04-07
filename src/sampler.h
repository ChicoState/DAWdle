#ifndef AUDIOINPUTNODE_H
#define AUDIOINPUTNODE_H

#include <QtNodes/NodeDelegateModel>
#include <QPushButton>
#include "bufferdata.h"
#include "audioinput.h"

extern float sampleRate;

class Sampler : public QtNodes::NodeDelegateModel, AudioInput {
    Q_OBJECT

public:
    Sampler();
    ~Sampler();

    QString caption() const override;
    QString name() const override;
    bool captionVisible() const override;

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override;
    void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex portIndex) override;
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override;

    QWidget* embeddedWidget() override;

    QJsonObject save() const override;
    void load(const QJsonObject& json) override;

private slots:
    void buttonPressed();

private:
    void loadFromFile();
    void refreshStream() override;

    std::shared_ptr<BufferData> m_bufferData;
    std::vector<float> m_audioData;
    QPushButton* m_button;
    std::string m_path;
    size_t m_bufPtr = 0;
};

#endif // AUDIOINPUTNODE_H
