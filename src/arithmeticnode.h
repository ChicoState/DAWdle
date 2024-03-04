#ifndef ARITHMETICNODE_H
#define ARITHMETICNODE_H

#include <QtNodes/NodeDelegateModel>
#include "bufferdata.h"

class ArithmeticNode : public QtNodes::NodeDelegateModel {
    Q_OBJECT

public:
    ArithmeticNode();
    ~ArithmeticNode();

    virtual QString caption() const = 0;
    virtual QString name() const = 0;
    bool captionVisible() const override;
    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override;
    void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex portIndex) override;
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override;

    QWidget* embeddedWidget() override;

protected:
    virtual void compute() = 0;

protected:
    std::shared_ptr<BufferData> _buffer1;
    std::shared_ptr<BufferData> _buffer2;

    std::shared_ptr<BufferData> _resultBuffer;
};

class AdditionNode : public ArithmeticNode {
public:
    QString caption() const override { return QStringLiteral("Addition"); }
    QString name() const override { return QStringLiteral("AdditionNode"); }

protected:
    void compute() override {
        _resultBuffer->m_buffer[0] = (_buffer1->m_buffer[0] + _buffer2->m_buffer[0]);
        Q_EMIT dataUpdated(0);
    }
};

class SubtractNode : public ArithmeticNode {
public:
    QString caption() const override { return QStringLiteral("Subtract"); }
    QString name() const override { return QStringLiteral("SubtractNode"); }

protected:
    void compute() override {
        _resultBuffer->m_buffer[0] = (_buffer1->m_buffer[0] - _buffer2->m_buffer[0]);
        Q_EMIT dataUpdated(0);
    }
};

class MultiplyNode : public ArithmeticNode {
public:
    QString caption() const override { return QStringLiteral("Multiply"); }
    QString name() const override { return QStringLiteral("MultiplyNode"); }

protected:
    void compute() override {
        _resultBuffer->m_buffer[0] = (_buffer1->m_buffer[0] * _buffer2->m_buffer[0]);
        Q_EMIT dataUpdated(0);
    }
};

class DivideNode : public ArithmeticNode {
public:
    QString caption() const override { return QStringLiteral("Division"); }
    QString name() const override { return QStringLiteral("DivisionNode"); }

protected:
    void compute() override {
        if (_buffer2->m_buffer[0] != 0) {
            _resultBuffer->m_buffer[0] = (_buffer1->m_buffer[0] + _buffer2->m_buffer[0]);
        } else {
            _resultBuffer->m_buffer[0] = 0;
        }
        Q_EMIT dataUpdated(0);
    }
};

#endif // ARITHMETICNODE_H
