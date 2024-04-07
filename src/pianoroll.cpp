#include "pianoroll.h"
#include <QDebug>

std::unordered_map<int, std::unordered_map<std::string, float>> noteToFrequency = {
    {0,
     {{"C", 16.35f},
      {"C#", 17.32f},
      {"D", 18.35f},
      {"D#", 19.45f},
      {"E", 20.60f},
      {"F", 21.83f},
      {"F#", 23.12f},
      {"G", 24.50f},
      {"G#", 25.96f},
      {"A", 27.50f},
      {"A#", 29.14f},
      {"B", 30.87f}}},
    {1,
     {{"C", 32.70f},
      {"C#", 34.65f},
      {"D", 36.71f},
      {"D#", 38.89f},
      {"E", 41.20f},
      {"F", 43.65f},
      {"F#", 46.25f},
      {"G", 49.00f},
      {"G#", 51.91f},
      {"A", 55.00f},
      {"A#", 58.27f},
      {"B", 61.74f}}},
    {2,
     {{"C", 65.41f},
      {"C#", 69.30f},
      {"D", 73.32f},
      {"D#", 77.78f},
      {"E", 82.41f},
      {"F", 87.41f},
      {"F#", 92.50f},
      {"G", 98.00f},
      {"G#", 103.83f},
      {"A", 110.00f},
      {"A#", 116.54f},
      {"B", 123.47f}}},
    {3,
     {{"C", 130.81f},
      {"C#", 138.59f},
      {"D", 146.83f},
      {"D#", 155.56f},
      {"E", 164.81f},
      {"F", 174.61f},
      {"F#", 185.00f},
      {"G", 196.00f},
      {"G#", 207.65f},
      {"A", 220.00f},
      {"A#", 233.08f},
      {"B", 246.94f}}},
    {4,
     {{"C", 261.63f},
      {"C#", 277.18f},
      {"D", 293.66f},
      {"D#", 311.13f},
      {"E", 329.63f},
      {"F", 349.23f},
      {"F#", 369.99f},
      {"G", 392.00f},
      {"G#", 415.30f},
      {"A", 440.00f},
      {"A#", 466.16f},
      {"B", 493.88f}}},
    {5,
     {{"C", 523.25f},
      {"C#", 554.37f},
      {"D", 587.33f},
      {"D#", 622.25f},
      {"E", 659.26f},
      {"F", 698.46f},
      {"F#", 739.99f},
      {"G", 783.99f},
      {"G#", 830.61f},
      {"A", 880.00f},
      {"A#", 932.33f},
      {"B", 987.77f}}},
    {6,
     {{"C", 1046.50f},
      {"C#", 1108.73f},
      {"D", 1174.66f},
      {"D#", 1244.51f},
      {"E", 1318.51f},
      {"F", 1396.91f},
      {"F#", 1479.98f},
      {"G", 1567.98f},
      {"G#", 1661.22f},
      {"A", 1760.00f},
      {"A#", 1864.66f},
      {"B", 1975.53f}}},
    {7,
     {{"C", 2093.00f},
      {"C#", 2217.46f},
      {"D", 2349.32f},
      {"D#", 2489.02f},
      {"E", 2637.02f},
      {"F", 2783.83f},
      {"F#", 2959.96f},
      {"G", 3135.95f},
      {"G#", 3322.44f},
      {"A", 3520.00f},
      {"A#", 3729.31f},
      {"B", 3951.07f}}},
    {8,
     {{"C", 4186.01f},
      {"C#", 4434.92f},
      {"D", 4698.64f},
      {"D#", 4978.03f},
      {"E", 5274.02f},
      {"F", 5587.65f},
      {"F#", 5919.91f},
      {"G", 6271.93f},
      {"G#", 6644.88f},
      {"A", 7040.00f},
      {"A#", 7458.62f},
      {"B", 7902.13f}}},
    };

PianoRoll::PianoRoll() {
    m_bpm = std::make_shared<BufferData>();
    m_bpm->setAll(0.0f);

    m_outBuffer = std::make_shared<BufferData>();
    m_outBuffer->setAll(0.0f);

    for (int i = 0; i < 108; i++){
        m_noteBuffers[i] = std::make_shared<BufferData>();
        m_noteBuffers[i]->setAll(0.0f);
    }

    m_pianoRollWindow = nullptr;
}


QString PianoRoll::caption() const {
    return QStringLiteral("Piano Roll");
}

QString PianoRoll::name() const {
    return QStringLiteral("PianoRoll");
}

bool PianoRoll::captionVisible() const {
    return true;
}

unsigned int PianoRoll::nPorts(QtNodes::PortType portType) const {
    if (portType == QtNodes::PortType::In) return 1;
    else if (portType == QtNodes::PortType::Out) return 1;
    return 0;}

QtNodes::NodeDataType PianoRoll::dataType(QtNodes::PortType, QtNodes::PortIndex) const {
    return BufferData{}.type();
}

void PianoRoll::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex) {
    if (auto inputData = std::dynamic_pointer_cast<BufferData>(data)) {
        for(size_t i = 0; i < BUFFERSIZE; i++) {
            m_bpm->m_buffer[i] = inputData->m_buffer[i];
        }
        Q_EMIT dataUpdated(0);
    }
}

std::shared_ptr<QtNodes::NodeData> PianoRoll::outData(QtNodes::PortIndex) {
    return m_outBuffer;
}

void PianoRoll::openWindow() {
    if (!m_pianoRollWindow) {
        m_pianoRollWindow = new QWidget();
        m_pianoRollWindow->setWindowTitle("Piano Roll Window");

        // Create a scroll area to contain the piano roll layout
        QScrollArea *scrollArea = new QScrollArea(m_pianoRollWindow);
        scrollArea->setWidgetResizable(true);
        m_pianoRollWindow->setLayout(new QVBoxLayout(m_pianoRollWindow));
        m_pianoRollWindow->layout()->addWidget(scrollArea);

        QWidget *contentWidget = new QWidget(scrollArea);
        QVBoxLayout *layout = new QVBoxLayout(contentWidget);
        scrollArea->setWidget(contentWidget);

        // Create labels and clickable areas for piano keys and add them to the layout
        int index = 0;
        for (const auto &octave : noteToFrequency) {
            for (const auto &note : octave.second) {
                // Create an instance of PianoKeyInfo for each key
                PianoKeyInfo *keyInfo = new PianoKeyInfo(QString::fromStdString(note.first), octave.first, note.second, index);
                index++;

                // Create a label representing the piano key
                QLabel *key = new QLabel(QString("%1 %2").arg(octave.first).arg(note.first.c_str()));
                key->setFixedSize(50, 40); // Set the size of each key
                key->setStyleSheet("border: 1px solid black; background-color: white;"); // Customize the key appearance
                key->setAlignment(Qt::AlignLeft); // Center the text horizontally

                // Create a layout for the key and clickable area
                QHBoxLayout *keyLayout = new QHBoxLayout();
                keyLayout->addWidget(key);
                keyLayout->setAlignment(Qt::AlignLeft); // Center the layout horizontally

                // Create four clickable buttons
                for (int i = 0; i < 4; ++i) {
                    QPushButton *button = new QPushButton();
                    button->setFixedSize(50, 40); // Set the size of each button
                    button->setStyleSheet("border: 1px solid black; background-color: white;"); // Customize the button appearance
                    button->setCheckable(true); // Make the button checkable

                    // Connect the button's clicked signal to a slot to handle its state change
                    connect(button, &QPushButton::clicked, [=]() {
                        if (button->isChecked()) {
                            button->setStyleSheet("border: 1px solid black; background-color: green;"); // Turn the button green if clicked
                        } else {
                            button->setStyleSheet("border: 1px solid black; background-color: white;"); // Turn the button white if not clicked
                        }
                    });

                    // Add the button to the layout
                    keyLayout->addWidget(button);
                }

                // Add the layout to the main layout
                layout->addLayout(keyLayout);

                // Store the PianoKeyInfo instance for future access
                m_keyInfoMap[key] = keyInfo;
            }
        }

        // Create a plus button in the upper right corner
        QPushButton *plusButton = new QPushButton("+", m_pianoRollWindow);
        plusButton->setFixedSize(20, 20);
        plusButton->move(300, 20); // Position the button in the upper right corner
        connect(plusButton, &QPushButton::clicked, [=]() {
            // Add another four buttons to each row
            for (auto layout : contentWidget->findChildren<QHBoxLayout*>()) {
                for (int i = 0; i < 4; ++i) {
                    QPushButton *button = new QPushButton();
                    button->setFixedSize(50, 40);
                    button->setStyleSheet("border: 1px solid black; background-color: white;");
                    button->setCheckable(true);
                    connect(button, &QPushButton::clicked, [=]() {
                        if (button->isChecked()) {
                            button->setStyleSheet("border: 1px solid black; background-color: green;");
                        } else {
                            button->setStyleSheet("border: 1px solid black; background-color: white;");
                        }
                    });
                    layout->addWidget(button);
                }
            }
            plusButton->move(plusButton->x()+225,plusButton->y());
        });
    }
    m_pianoRollWindow->showMaximized();
}

QWidget* PianoRoll::embeddedWidget() {
    QPushButton* button = new QPushButton("Open");
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    button->setMaximumSize(50, 20);
    connect(button, &QPushButton::clicked, this, &PianoRoll::openWindow);
    return button;
}
