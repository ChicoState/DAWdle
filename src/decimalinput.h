#ifndef DECIMALINPUT_H
#define DECIMALINPUT_H

#include <QtNodes/NodeDelegateModel>
#include <QtWidgets/QLineEdit>
#include <QtCore/QObject>

#include "bufferdata.h"
#include "audioinput.h"

class DecimalInput : public QtNodes::NodeDelegateModel, AudioInput {
    Q_OBJECT

    public:
        DecimalInput();
        ~DecimalInput();

        QString caption() const override;
        QString name() const override;
        bool captionVisible() const override;

        unsigned int nPorts(QtNodes::PortType portType) const override;
        QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override;
        void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override;
        std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override;

        QWidget* embeddedWidget() override;

        void updateBufferData(const QString& value);

    private:
        std::shared_ptr<BufferData> m_bufferData;
        QLineEdit* m_lineEdit;

        void refreshStream() override;
};

#endif
