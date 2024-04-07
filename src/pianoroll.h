#ifndef PIANOROLL_H
#define PIANOROLL_H

#include <QtNodes/NodeDelegateModel>
#include <QWidget>
#include <QPushButton>
#include "bufferdata.h"
#include <map>
#include <QString>
#include <QObject>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>
#include "audiooutput.h"

class PianoKeyInfo : public QObject {
    Q_OBJECT
public:
    PianoKeyInfo(const QString& note, int octave, float frequency, int index, QObject* parent = nullptr)
        : QObject(parent), m_note(note), m_octave(octave), m_frequency(frequency), m_index (index) {}

    int index() const {return m_index; }
    QString note() const { return m_note; }
    int octave() const { return m_octave; }
    float frequency() const { return m_frequency; }

private:
    int m_index;
    QString m_note;
    int m_octave;
    float m_frequency;
};

class PianoRoll : public QtNodes::NodeDelegateModel {
    Q_OBJECT
public:
    PianoRoll();

    QString caption() const override;
    QString name() const override;
    bool captionVisible() const override;
    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override;
    void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex) override;
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override;
    QWidget* embeddedWidget() override;

private slots:
    void openWindow();

private:
    std::shared_ptr<BufferData> m_bpm;
    std::shared_ptr<BufferData> m_noteBuffers[108];
    std::shared_ptr<BufferData> m_outBuffer;
    QWidget* m_pianoRollWindow;
    std::map<QLabel*, PianoKeyInfo*> m_keyInfoMap;
};

#endif // PIANOROLL_H
