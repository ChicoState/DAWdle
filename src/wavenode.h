#ifndef WAVENODE_H
#define WAVENODE_H

#include <QtNodes/NodeDelegateModel>
#include <QtWidgets/QLineEdit>
#include <QtCore/QObject>

#include "bufferdata.h"

class WaveNode : public QtNodes::NodeDelegateModel {
    Q_OBJECT

public:
    WaveNode();

    QString caption() const override = 0;
    QString name() const override = 0;
    bool captionVisible() const override;

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override;
    void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex) override;
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override;

    QWidget* embeddedWidget() override;

    void updateBufferData(const QString& value); // why is this here?

protected:
    std::shared_ptr<BufferData> m_inBuffer;
    std::shared_ptr<BufferData> m_outBuffer;
    std::unordered_map<float, float> m_timesteps;

    virtual void generateWave() = 0;
};


class SineWave : public WaveNode {
public:
    QString name() const override;
    QString caption() const override;
private:
    void generateWave() override;
};

class SawWave : public WaveNode {
public:
    QString name() const override;
    QString caption() const override;
private:
    void generateWave() override;
};

class SquareWave : public WaveNode {
public:
    QString name() const override;
    QString caption() const override;
private:
    void generateWave() override;
};

class TriangleWave : public WaveNode {
public:
    QString name() const override;
    QString caption() const override;
private:
    void generateWave() override;
};

class NoiseWave : public WaveNode {
public:
    QString name() const override;
    QString caption() const override;
private:
    void generateWave() override;
};

#endif // WAVENODE_H
