#ifndef ARITHMETICNODE_H
#define ARITHMETICNODE_H

#include <QtNodes/NodeDelegateModel>
#include "bufferdata.h"

class ArithmeticNode : public QtNodes::NodeDelegateModel {
    Q_OBJECT

    public:
        ArithmeticNode(const char* name);

        QString caption() const override;
        QString name() const override;
        bool captionVisible() const override;

        unsigned int nPorts(QtNodes::PortType portType) const override;
        QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override;
        void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex portIndex) override;
        std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override;

        QWidget* embeddedWidget() override;

    protected:
        virtual void compute() = 0;

        std::shared_ptr<BufferData> m_inBuffer1;
        std::shared_ptr<BufferData> m_inBuffer2;
        std::shared_ptr<BufferData> m_outBuffer;

    private:
        QString m_name;
};

class AdditionNode : public ArithmeticNode {
    public:
        AdditionNode();
    private:
        void compute() override;
};

class SubtractionNode : public ArithmeticNode {
    public:
        SubtractionNode();
    private:
        void compute() override;
};

class MultiplicationNode : public ArithmeticNode {
    public:
        MultiplicationNode();
    private:
        void compute() override;
};

class DivisionNode : public ArithmeticNode {
    public:
        DivisionNode();
    private:
        void compute() override;
};

#endif
