#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtWidgets/QApplication>
#include <QtWidgets/QVBoxLayout>
#include <QScreen>

#include "decimalinput.h"

int main(int argc, char** argv) {
    QApplication app{ argc, argv };
    QWidget window;
    std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registry = std::make_shared<QtNodes::NodeDelegateModelRegistry>();
    registry->registerModel<DecimalInput>("Sources");

    QtNodes::DataFlowGraphModel graph{ registry };
    QtNodes::DataFlowGraphicsScene* scene = new QtNodes::DataFlowGraphicsScene{ graph, &window };
    QtNodes::GraphicsView* view = new QtNodes::GraphicsView{ scene };

    graph.addNode("");

    QVBoxLayout* layout = new QVBoxLayout{ &window };
    layout->addWidget(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    window.setWindowTitle("DAWdle");
    window.showNormal();

    return app.exec();
}
