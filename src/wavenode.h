#ifndef WAVENODE_H
#define WAVENODE_H

#include <QtNodes/NodeDelegateModel>
#include <QtWidgets/QLineEdit>
#include <QtCore/QObject>

#include "bufferdata.h"

class WaveNode : public QtNodes::NodeDelegateModel {
    Q_OBJECT

public:
    WaveNode(const char* name);

    QString caption() const override;
    QString name() const override;
    bool captionVisible() const override;

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override;
    void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex) override;
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override;

    QWidget* embeddedWidget() override;

protected:
    std::shared_ptr<BufferData> m_inBuffer;
    std::shared_ptr<BufferData> m_outBuffer;
    std::unordered_map<float, float> m_timesteps;

    virtual void generateWave() = 0;

private:
    QString m_name;
};


class SineWave : public WaveNode {
    public:
        SineWave();
    private:
        void generateWave() override;
};

class SawWave : public WaveNode {
    public:
        SawWave();
    private:
        void generateWave() override;
};

class SquareWave : public WaveNode {
    public:
        SquareWave();
    private:
        void generateWave() override;
};

class TriangleWave : public WaveNode {
    public:
        TriangleWave();
    private:
        void generateWave() override;
};

class NoiseWave : public WaveNode {
    public:
        NoiseWave();
    private:
        void generateWave() override;
};

#endif // WAVENODE_H
