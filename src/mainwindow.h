#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QTabWidget>
#include <QMenu>
#include <memory>

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/NodeDelegateModelRegistry>

#include "decimalinput.h"
#include "sinewavenode.h"
#include "audiooutputnode.h"
#include "arithmeticnode.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();

private slots:
    void createDecimalNode();
    void createSineWaveNode();
    void createAudioOutputNode();
    void createAddNode();
    void createSubtractNode();
    void createMultiplyNode();
    void createDivideNode();

private:
    template <typename NodeType>
    void createNode();

    QToolBar* toolbar;
    QTabWidget* tabWidget;
    std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registry;
    QtNodes::DataFlowGraphModel* graph;
    QtNodes::DataFlowGraphicsScene* scene;
    QtNodes::GraphicsView* view;
};

#endif // MAINWINDOW_H
