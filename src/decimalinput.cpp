#include "decimalinput.h"

#include <QtGui/QDoubleValidator>

DecimalInput::DecimalInput() {
    m_lineEdit = new QLineEdit;
    m_lineEdit->setValidator(new QDoubleValidator);
    m_lineEdit->setMaximumSize(m_lineEdit->sizeHint());

    m_bufferData = std::make_shared<BufferData>();
    m_bufferData->setAll(0.0f);

    connect(m_lineEdit, &QLineEdit::textChanged, this, &DecimalInput::updateBufferData);
}

DecimalInput::~DecimalInput() {
    delete m_lineEdit;
}

QString DecimalInput::caption() const {
    return QStringLiteral("Decimal Input");
}

QString DecimalInput::name() const {
    return QStringLiteral("DecimalInput");
}

bool DecimalInput::captionVisible() const {
    return true;
}

unsigned int DecimalInput::nPorts(QtNodes::PortType portType) const {
    if(portType == QtNodes::PortType::Out) return 1;
    return 0;
}

QtNodes::NodeDataType DecimalInput::dataType(QtNodes::PortType, QtNodes::PortIndex) const {
    return BufferData{}.type();
}

void DecimalInput::setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) {}

std::shared_ptr<QtNodes::NodeData> DecimalInput::outData(QtNodes::PortIndex) {
    return m_bufferData;
}

QWidget* DecimalInput::embeddedWidget() {
    m_lineEdit->setText(QString::number(m_bufferData->m_buffer[0]));
    return m_lineEdit;
}

QJsonObject DecimalInput::save() const {
    QJsonObject modelJson = NodeDelegateModel::save();
    modelJson["value"] = QString::number(m_bufferData->m_buffer[0]);
    return modelJson;
}

void DecimalInput::load(const QJsonObject& json) {
    QJsonValue val = json["value"];

    if (!val.isUndefined()) {
        QString strNum = val.toString();

        bool ok;
        double data = strNum.toDouble(&ok);
        if (ok) {
            m_bufferData = std::make_shared<BufferData>();
            m_bufferData->setAll(data);

            if (m_lineEdit) {
                m_lineEdit->setText(strNum);
            }
        }
    }
}

void DecimalInput::updateBufferData(const QString& value) {
    m_bufferData->setAll(value.toFloat());
    Q_EMIT dataUpdated(0);
}

void DecimalInput::refreshStream() {
    Q_EMIT dataUpdated(0);
}
