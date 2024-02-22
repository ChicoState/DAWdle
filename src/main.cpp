#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtWidgets/QApplication>
#include <QtWidgets/QVBoxLayout>
#include <QScreen>

int main(int argc, char** argv) {
    QApplication app{ argc, argv };
    QWidget window;

    QtNodes::DataFlowGraphModel dataFlowGraphModel{ std::make_shared<QtNodes::NodeDelegateModelRegistry>() };
    QtNodes::DataFlowGraphicsScene* scene = new QtNodes::DataFlowGraphicsScene{ dataFlowGraphModel, &window };
    QtNodes::GraphicsView* view = new QtNodes::GraphicsView{ scene };

    QVBoxLayout* layout = new QVBoxLayout{ &window };
    layout->addWidget(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    window.setWindowTitle("DAWdle");
    window.showNormal();

    return app.exec();
}
