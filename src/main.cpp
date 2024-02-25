#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtWidgets/QApplication>
#include <QtWidgets/QVBoxLayout>
#include <QScreen>

#include "decimalinput.h"
#include "sinewavenode.h"
#include "audiooutputnode.h"

int main(int argc, char** argv) {
    QApplication app{ argc, argv };
    QWidget window;
    std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registry = std::make_shared<QtNodes::NodeDelegateModelRegistry>();
    registry->registerModel<DecimalInput>("Sources");
    registry->registerModel<SineWaveNode>("Sources");
    registry->registerModel<AudioOutputNode>("Sources");

    QtNodes::DataFlowGraphModel graph{ registry };
    QtNodes::DataFlowGraphicsScene* scene = new QtNodes::DataFlowGraphicsScene{ graph, &window };
    QtNodes::GraphicsView* view = new QtNodes::GraphicsView{ scene };



    // DEMO ================================
    auto inputNode = std::make_shared<DecimalInput>();
    QString inputNodeName = inputNode->name();
    graph.addNode(inputNodeName);

    auto sinNode = std::make_shared<SineWaveNode>();
    QString sinNodeName = sinNode->name();
    graph.addNode(sinNodeName);

    auto audioOutputNode = std::make_shared<AudioOutputNode>();
    QString audioOutputNodeName = audioOutputNode->name();
    graph.addNode(audioOutputNodeName);


    //=====================================
    QVBoxLayout* layout = new QVBoxLayout{ &window };
    layout->addWidget(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    window.setWindowTitle("DAWdle");
    window.showNormal();

    return app.exec();
}
