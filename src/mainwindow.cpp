#include "MainWindow.h"

MainWindow::MainWindow() {
    // Create a toolbar
    toolbar = new QToolBar(this);
    addToolBar(toolbar);

    // Register node models
    registry = std::make_shared<QtNodes::NodeDelegateModelRegistry>();
    registry->registerModel<DecimalInput>("Sources");
    registry->registerModel<SineWaveNode>("Oscillators");
    registry->registerModel<AudioOutputNode>("");

    // Create the graph and scene
    graph = new QtNodes::DataFlowGraphModel{ registry };
    scene = new QtNodes::DataFlowGraphicsScene{ *graph, this };

    // Create the view
    view = new QtNodes::GraphicsView{ scene };

    // Add an initial node to the graph
    graph->addNode(AudioOutputNode{}.name());

    // Set up the layout
    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create toolbar actions
    addDecimalButton = toolbar->addAction("Add Decimal");
    addSineWaveButton = toolbar->addAction("Add Sine Wave");
    addAudioOutputButton = toolbar->addAction("Add Audio Output");

    // Connect toolbar actions to slots
    connect(addDecimalButton, &QAction::triggered, this, [this]() {
        createNode<DecimalInput>();
    });

    connect(addSineWaveButton, &QAction::triggered, this, [this]() {
        createNode<SineWaveNode>();
    });

    connect(addAudioOutputButton, &QAction::triggered, this, [this]() {
        createNode<AudioOutputNode>();
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

