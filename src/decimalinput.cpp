#include "decimalinput.h"

#include <QtGui/QDoubleValidator>

DecimalInput::DecimalInput() {
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::TimerType::PreciseTimer);

    m_lineEdit = new QLineEdit;
    m_lineEdit->setValidator(new QDoubleValidator);
    m_lineEdit->setMaximumSize(m_lineEdit->sizeHint());

    m_bufferData = std::make_shared<BufferData>();
    m_bufferData->setAll(0.0f);

    connect(m_lineEdit, &QLineEdit::textChanged, this, &DecimalInput::updateBufferData);
    connect(m_timer, &QTimer::timeout, this, &DecimalInput::repropogateData);
}

DecimalInput::~DecimalInput() {
    delete m_lineEdit;
    delete m_timer;
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

void DecimalInput::updateBufferData(const QString& value) {
    m_bufferData->setAll(value.toFloat());
    m_timer->start(static_cast<double>(BUFFERSIZE) / SAMPLERATE * 1000);
    Q_EMIT dataUpdated(0);
}

void DecimalInput::repropogateData() {
    Q_EMIT dataUpdated(0);
}
