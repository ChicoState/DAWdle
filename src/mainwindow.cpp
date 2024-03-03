#include "MainWindow.h"

MainWindow::MainWindow() {
    // Create a toolbar
    toolbar = new QToolBar(this);
    addToolBar(toolbar);

    // Create a tab widget
    tabWidget = new QTabWidget(this);

    // Register node models
    registry = std::make_shared<QtNodes::NodeDelegateModelRegistry>();
    registry->registerModel<DecimalInput>("Input");
    registry->registerModel<SineWaveNode>("Audio Generation");
    registry->registerModel<AddNode>("Arithmetic");
    registry->registerModel<SubtractNode>("Arithmetic");
    registry->registerModel<MultiplyNode>("Arithmetic");
    registry->registerModel<DivideNode>("Arithmetic");
    registry->registerModel<AudioOutputNode>("Audio Generation");

    // Create the graph and scene
    graph = new QtNodes::DataFlowGraphModel{ registry };
    scene = new QtNodes::DataFlowGraphicsScene{ *graph, this };

    // Create the view
    view = new QtNodes::GraphicsView{ scene };

    // Add an initial node to the graph
    graph->addNode(AudioOutputNode{}.name());

    // Set up the layout
    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(tabWidget);
    layout->addWidget(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create menu items
    QMenu* inputMenu = new QMenu("Input", this);
    inputMenu->addAction("Add Decimal", this, SLOT(createDecimalNode()));

    QMenu* audioMenu = new QMenu("Audio Generation", this);
    audioMenu->addAction("Add Sine Wave", this, SLOT(createSineWaveNode()));
    audioMenu->addAction("Add Audio Output", this, SLOT(createAudioOutputNode()));

    QMenu* arithmeticMenu = new QMenu("Arithmetic", this);
    arithmeticMenu->addAction("Add Addition Node", this, SLOT(createAddNode()));
    arithmeticMenu->addAction("Add Subtraction Node", this, SLOT(createSubtractNode()));
    arithmeticMenu->addAction("Add Multiply Node", this, SLOT(createMultiplyNode()));
    arithmeticMenu->addAction("Add Divide Node", this, SLOT(createDivideNode()));

    // Add menus to toolbar
    toolbar->addAction(inputMenu->menuAction());
    toolbar->addAction(audioMenu->menuAction());
    toolbar->addAction(arithmeticMenu->menuAction());

    // Set up the main window
    setCentralWidget(new QWidget);
    centralWidget()->setLayout(layout);
    setWindowTitle("DAWdle");
    showNormal();
}

// Define slot functions for menu actions
void MainWindow::createDecimalNode() {
    createNode<DecimalInput>();
}

void MainWindow::createSineWaveNode() {
    createNode<SineWaveNode>();
}

void MainWindow::createAudioOutputNode() {
    createNode<AudioOutputNode>();
}

void MainWindow::createAddNode() {
    createNode<AddNode>();
}

void MainWindow::createSubtractNode() {
    createNode<SubtractNode>();
}

void MainWindow::createMultiplyNode() {
    createNode<MultiplyNode>();
}

void MainWindow::createDivideNode() {
    createNode<DivideNode>();
}

template <typename NodeType>
void MainWindow::createNode() {
    try {
        graph->addNode(NodeType{}.name());
        scene->update();
    } catch (const std::exception& e) {
        qDebug() << "Error creating node: " << e.what();
    }
}
