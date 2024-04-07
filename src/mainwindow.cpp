#include <QtNodes/DataFlowGraphicsScene>

#include "mainwindow.h"
#include "decimalinput.h"
#include "sampler.h"
#include "wavenode.h"
#include "audiooutput.h"
#include "arithmeticnode.h"
#include "util.h"

MainWindow::MainWindow() {
    // Create a toolbar
    toolbar = new QToolBar(this);
    addToolBar(toolbar);

    // Register node models
    registry = std::make_shared<QtNodes::NodeDelegateModelRegistry>();
    registry->registerModel<DecimalInput>("Input");
    registry->registerModel<Sampler>("Input");
    registry->registerModel<AdditionNode>("Arithmetic");
    registry->registerModel<SubtractionNode>("Arithmetic");
    registry->registerModel<MultiplicationNode>("Arithmetic");
    registry->registerModel<DivisionNode>("Arithmetic");
    registry->registerModel<DecimalInput>("Sources");
    registry->registerModel<SineWave>("Oscillators");
    registry->registerModel<SawWave>("Oscillators");
    registry->registerModel<SquareWave>("Oscillators");
    registry->registerModel<TriangleWave>("Oscillators");
    registry->registerModel<AudioOutput>("Output");

    // Create the graph and scene
    graph = new QtNodes::DataFlowGraphModel{ registry };
    scene = new QtNodes::DataFlowGraphicsScene{ *graph, this };

    // Create the view
    view = new QtNodes::GraphicsView{ scene };

    // Create menu items
    QMenu* inputMenu = new QMenu("Input", this);
    inputMenu->addAction("Add Decimal", this, SLOT(createDecimalNode()));
    inputMenu->addAction("Add Sampler", this, SLOT(createSamplerNode()));

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
    layout->addWidget(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Add menus to toolbar
    toolbar->addAction(inputMenu->menuAction());
    toolbar->addAction(audioMenu->menuAction());
    toolbar->addAction(arithmeticMenu->menuAction());

    // Save and load functionality
    QAction* saveAction = toolbar->addAction("Save Project");
    QAction* loadAction = toolbar->addAction("Load Project");
    QObject::connect(saveAction, &QAction::triggered, scene, &QtNodes::DataFlowGraphicsScene::save);
    QObject::connect(loadAction, &QAction::triggered, scene, &QtNodes::DataFlowGraphicsScene::load);

    // Initialize PortAudio
    initializePortAudio();

    // Set up the main window
    setCentralWidget(new QWidget);
    centralWidget()->setLayout(layout);
    setWindowTitle("DAWdle");
    showNormal();
}

MainWindow::~MainWindow() {
    cleanupPortAudio();
}

PaStream* MainWindow::getStream() {
    return m_paStream;
}

// Define slot functions for menu actions
void MainWindow::createDecimalNode() {
    createNode<DecimalInput>();
}

void MainWindow::createSamplerNode() {
    createNode<Sampler>();
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

void MainWindow::initializePortAudio() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "Pa_Initialize failed with error: %s\n", Pa_GetErrorText(err));
        exit(EXIT_FAILURE);
    }
    static uint32_t apiRanking[]{
        /*paInDevelopment*/   0xFFFFFFFF,
        /*paDirectSound*/     10,
        /*paMME*/             11,
        /*paASIO*/            0,
        /*paSoundManager*/    10,
        /*paCoreAudio*/       10,
        /*dummy value*/       0xFFFFFFFF,
        /*paOSS*/             10,
        /*paALSA*/            10,
        /*paAL*/              10,
        /*paBeOS*/            10,
        /*paWDMKS*/           10,
        /*paJACK*/            10,
        /*paWASAPI*/          1,
        /*paAudioScienceHPI*/ 10,
        /*paAudioIO*/         10,
        /*paPulseAudio*/      10
    };


    const PaHostApiInfo* apiInfo = Pa_GetHostApiInfo(0);
    PaHostApiIndex hostAPIs = Pa_GetHostApiCount();
    for (PaHostApiIndex api = 1; api < hostAPIs; api++) {
        const PaHostApiInfo* checkAPIInfo = Pa_GetHostApiInfo(api);
        if (checkAPIInfo->type < ARRAY_COUNT(apiRanking) && apiRanking[checkAPIInfo->type] < apiRanking[apiInfo->type]) {
            apiInfo = checkAPIInfo;
        }
    }
    qDebug("DAWdle using API: %s", apiInfo->name);

    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(apiInfo->defaultOutputDevice);
    sampleRate = float(deviceInfo->defaultSampleRate);

    PaStreamParameters outputParameters;
    outputParameters.device = apiInfo->defaultOutputDevice;
    outputParameters.channelCount = 1;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.hostApiSpecificStreamInfo = nullptr;
    outputParameters.suggestedLatency = deviceInfo->defaultLowOutputLatency;

    err = Pa_OpenStream(
        &m_paStream,
        nullptr,
        &outputParameters,
        double(sampleRate),
        paFramesPerBufferUnspecified,
        paNoFlag,
        &AudioOutput::paCallback,
        nullptr
    );

    if (err != paNoError) {
        fprintf(stderr, "Pa_OpenStream failed with error: %s\n", Pa_GetErrorText(err));
        exit(EXIT_FAILURE);
    }

    Pa_SetStreamFinishedCallback(m_paStream, nullptr);
}

void MainWindow::cleanupPortAudio() {
    Pa_StopStream(m_paStream);
    Pa_CloseStream(m_paStream);
    Pa_Terminate();
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

