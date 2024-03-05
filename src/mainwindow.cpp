#include "mainwindow.h"
#include "decimalinput.h"
#include "wavenode.h"
#include "audiooutput.h"
#include "arithmeticnode.h"

MainWindow::MainWindow() {
    // Create a toolbar
    toolbar = new QToolBar(this);
    addToolBar(toolbar);

    // Create a tab widget
    tabWidget = new QTabWidget(this);

    // Register node models
    registry = std::make_shared<QtNodes::NodeDelegateModelRegistry>();
    registry->registerModel<DecimalInput>("Input");
    registry->registerModel<AdditionNode>("Arithmetic");
    registry->registerModel<SubtractionNode>("Arithmetic");
    registry->registerModel<MultiplicationNode>("Arithmetic");
    registry->registerModel<DivisionNode>("Arithmetic");
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

    // Create menu items
    QMenu* inputMenu = new QMenu("Input", this);
    inputMenu->addAction("Add Decimal", this, SLOT(createDecimalNode()));

    QMenu* audioMenu = new QMenu("Audio Generation", this);
    audioMenu->addAction("Add Sine Wave", this, SLOT(createSineWave()));
    audioMenu->addAction("Add Saw Wave", this, SLOT(createSawWave()));
    audioMenu->addAction("Add Square Wave", this, SLOT(createSquareWave()));
    audioMenu->addAction("Add Triangle Wave", this, SLOT(createTriangleWave()));
    audioMenu->addAction("Add Noise Wave", this, SLOT(createNoiseWave()));

    QMenu* arithmeticMenu = new QMenu("Arithmetic", this);
    arithmeticMenu->addAction("Add Addition Node", this, SLOT(createAdditionNode()));
    arithmeticMenu->addAction("Add Subtraction Node", this, SLOT(createSubtractNode()));
    arithmeticMenu->addAction("Add Multiply Node", this, SLOT(createMultiplyNode()));
    arithmeticMenu->addAction("Add Divide Node", this, SLOT(createDivideNode()));

    // Set up the layout
    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(tabWidget);
    layout->addWidget(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    //Add menus to toolbar
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

void MainWindow::createSineWave() {
    createNode<SineWave>();
}

void MainWindow::createSawWave() {
    createNode<SawWave>();
}

void MainWindow::createSquareWave() {
    createNode<SquareWave>();
}

void MainWindow::createTriangleWave() {
    createNode<TriangleWave>();
}

void MainWindow::createNoiseWave() {
    createNode<NoiseWave>();
}

void MainWindow::createAdditionNode() {
    createNode<AdditionNode>();
}

void MainWindow::createSubtractNode() {
    createNode<SubtractionNode>();
}

void MainWindow::createMultiplyNode() {
    createNode<MultiplicationNode>();
}

void MainWindow::createDivideNode() {
    createNode<DivisionNode>();
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

