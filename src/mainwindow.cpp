#include "mainwindow.h"
#include "decimalinput.h"
#include "wavenode.h"
#include "audiooutput.h"

MainWindow::MainWindow() {
    // Create a toolbar
    toolbar = new QToolBar(this);
    addToolBar(toolbar);

    // Register node models
    registry = std::make_shared<QtNodes::NodeDelegateModelRegistry>();
    registry->registerModel<DecimalInput>("Sources");
    registry->registerModel<SineWave>("Oscillators");
    registry->registerModel<SawWave>("Oscillators");
    registry->registerModel<SquareWave>("Oscillators");
    registry->registerModel<TriangleWave>("Oscillators");
    registry->registerModel<NoiseWave>("Oscillators");

    // Create the graph and scene
    graph = new QtNodes::DataFlowGraphModel{ registry };
    scene = new QtNodes::DataFlowGraphicsScene{ *graph, this };

    // Create the view
    view = new QtNodes::GraphicsView{ scene };

    // Add an initial node to the graph
    graph->addNode<AudioOutput>();

    // Set up the layout
    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create toolbar actions
    addDecimalInputButton = toolbar->addAction("Add Decimal Input");
    addSineWaveButton = toolbar->addAction("Add Sine Wave");
    addSawWaveButton = toolbar->addAction("Add Saw Wave");
    addSquareWaveButton = toolbar->addAction("Add Square Wave");
    addTriangleWaveButton = toolbar->addAction("Add Triangle Wave");
    addNoiseWaveButton = toolbar->addAction("Add Noise Wave");

    // Connect toolbar actions to slots
    connect(addDecimalInputButton, &QAction::triggered, this, [this]() {
        createNode<DecimalInput>();
    });
    connect(addSineWaveButton, &QAction::triggered, this, [this]() {
        createNode<SineWave>();
    });
    connect(addSawWaveButton, &QAction::triggered, this, [this]() {
        createNode<SawWave>();
    });
    connect(addSquareWaveButton, &QAction::triggered, this, [this]() {
        createNode<SquareWave>();
    });
    connect(addTriangleWaveButton, &QAction::triggered, this, [this]() {
        createNode<TriangleWave>();
    });
    connect(addNoiseWaveButton, &QAction::triggered, this, [this]() {
        createNode<NoiseWave>();
    });

    // Set up the main window
    setCentralWidget(new QWidget);
    centralWidget()->setLayout(layout);
    setWindowTitle("DAWdle");
    showNormal();
}

template <typename NodeType>
void MainWindow::createNode() {
    graph->addNode(NodeType{}.name());
}

