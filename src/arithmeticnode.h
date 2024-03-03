// arithmeticnode.h
#ifndef ARITHMETICNODE_H
#define ARITHMETICNODE_H

#include <QtNodes/NodeDelegateModel>
#include <QtWidgets/QLineEdit>
#include <QtCore/QObject>

#include "bufferdata.h"

class ArithmeticNode : public QtNodes::NodeDelegateModel {
    Q_OBJECT

public:
    ArithmeticNode(const QString& operationName);
    ~ArithmeticNode() override = default;

    QString caption() const override;
    QString name() const override;
    bool captionVisible() const override;

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override;
    void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex portIndex) override;
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex portIndex) override;

    QWidget* embeddedWidget() override;
    virtual float performOperation(float operand1, float operand2) const = 0;

private:
    std::shared_ptr<BufferData> m_bufferData;
    QString m_operationName;
};

class AddNode : public ArithmeticNode {
    Q_OBJECT

public:
    AddNode();
    float performOperation(float operand1, float operand2) const override;
};

class SubtractNode : public ArithmeticNode {
    Q_OBJECT

public:
    SubtractNode();
    float performOperation(float operand1, float operand2) const override;
};

class MultiplyNode : public ArithmeticNode {
    Q_OBJECT

public:
    MultiplyNode();
    float performOperation(float operand1, float operand2) const override;
};

class DivideNode : public ArithmeticNode {
    Q_OBJECT

public:
    DivideNode();
    float performOperation(float operand1, float operand2) const override;
};

#endif
