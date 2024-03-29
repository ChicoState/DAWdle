#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/NodeDelegateModelRegistry>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();

private slots:
    void createDecimalNode();
    void createSineWave();
    void createSawWave();
    void createSquareWave();
    void createTriangleWave();
    void createNoiseWave();
    void createAdditionNode();
    void createSubtractNode();
    void createMultiplyNode();
    void createDivideNode();

private:

    template <typename NodeType>
    void createNode();

    QToolBar* toolbar;
    std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registry;
    QtNodes::DataFlowGraphModel* graph;
    QtNodes::DataFlowGraphicsScene* scene;
    QtNodes::GraphicsView* view;
};

#endif // MAINWINDOW_H
