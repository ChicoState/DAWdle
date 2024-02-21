#include <QtWidgets/QApplication>
#include <QWidget>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget window;

    window.showNormal();
    return app.exec();
}
