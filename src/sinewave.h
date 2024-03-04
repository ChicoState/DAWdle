#ifndef SINEWAVE_H
#define SINEWAVE_H

#include <QtNodes/NodeDelegateModel>
#include <QtWidgets/QLineEdit>
#include <QtCore/QObject>

#include "bufferdata.h"

class SineWave : public QtNodes::NodeDelegateModel {
    Q_OBJECT

public:
    SineWave();

    QString caption() const override;
    QString name() const override;
    bool captionVisible() const override;

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override;
    void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex) override;
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override;

    QWidget* embeddedWidget() override;

    void updateBufferData(const QString& value);

private:
    std::shared_ptr<BufferData> m_inBuffer;
    std::shared_ptr<BufferData> m_outBuffer;
    std::unordered_map<float, float> m_timesteps;
    void generateWave();
};

#endif
