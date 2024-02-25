#include "sinewavenode.h"
#include <QtGui/QDoubleValidator>

SineWaveNode::SineWaveNode() {
    m_lineEdit = new QLineEdit;
    m_lineEdit->setValidator(new QDoubleValidator);
    m_lineEdit->setMaximumSize(m_lineEdit->sizeHint());

    m_bufferData = std::make_shared<BufferData>();
    m_bufferData->setAll(0.0f);

    connect(m_lineEdit, &QLineEdit::textChanged, this, &SineWaveNode::updateBufferData);
}

SineWaveNode::~SineWaveNode() {
    delete m_lineEdit;
}

QString SineWaveNode::caption() const {
    return QStringLiteral("Sine Wave Generator");
}

QString SineWaveNode::name() const {
    return QStringLiteral("SineWaveNode");
}

bool SineWaveNode::captionVisible() const {
    return true;
}

unsigned int SineWaveNode::nPorts(QtNodes::PortType portType) const {
    if (portType == QtNodes::PortType::Out) return 1;
    if (portType == QtNodes::PortType::In) return 1;
    return 0;
}

QtNodes::NodeDataType SineWaveNode::dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const {
    if (portType == QtNodes::PortType::Out) return BufferData{}.type();
    if (portType == QtNodes::PortType::In && portIndex == 0) return BufferData{}.type();
    return QtNodes::NodeDataType{};
}

void SineWaveNode::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex portIndex) {
    if (portIndex == 0) {
        if (auto inputData = std::dynamic_pointer_cast<BufferData>(data)) {
            m_bufferData->setAll(inputData->m_buffer[0]);
            generateSineWave();
        }
    }
}

std::shared_ptr<QtNodes::NodeData> SineWaveNode::outData(QtNodes::PortIndex portIndex) {
    return m_bufferData;
}

QWidget* SineWaveNode::embeddedWidget() {
    m_lineEdit->setText(QString::number(m_bufferData->m_buffer[0]));
    return m_lineEdit;
}

void SineWaveNode::updateBufferData(const QString& value) {
    m_bufferData->setAll(value.toFloat());
    generateSineWave();
    Q_EMIT dataUpdated(0);
}

void SineWaveNode::generateSineWave() {
    float inputValue = m_bufferData->m_buffer[0];

    for (size_t i = 0; i < BUFFERSIZE; i++) {
        m_bufferData->m_buffer[i] = inputValue * std::sin(2.0 * M_PI * i / static_cast<float>(BUFFERSIZE));
    }
}
