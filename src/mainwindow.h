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

#include "decimalinput.h"
#include "sinewavenode.h"
#include "audiooutputnode.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();

private:

    template <typename NodeType>
    void createNode();

    QToolBar* toolbar;
    std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registry;
    QtNodes::DataFlowGraphModel* graph;
    QtNodes::DataFlowGraphicsScene* scene;
    QtNodes::GraphicsView* view;
    QAction* addDecimalButton;
    QAction* addSineWaveButton;
    QAction* addAudioOutputButton;
};

#endif // MAINWINDOW_H
