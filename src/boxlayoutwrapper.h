#ifndef BOXLAYOUTWRAPPER_H
#define BOXLAYOUTWRAPPER_H

#include <QWidget>
#include <QVBoxLayout>

class BoxLayoutWrapper : public QWidget {
    Q_OBJECT
public:
    explicit BoxLayoutWrapper(const std::vector<QWidget*>& children, QWidget* parent = nullptr);
    ~BoxLayoutWrapper();

private:
    QVBoxLayout* m_layout;
};

#endif // BOXLAYOUTWRAPPER_H
