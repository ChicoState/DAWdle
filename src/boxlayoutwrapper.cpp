#include "boxlayoutwrapper.h"

BoxLayoutWrapper::BoxLayoutWrapper(const std::vector<QWidget*>& children, QWidget* parent) : QWidget{parent} {
    m_layout = new QVBoxLayout();
    for(QWidget* w : children) {
        m_layout->addWidget(w);
    }
    setLayout(m_layout);
}

BoxLayoutWrapper::~BoxLayoutWrapper() {
    delete m_layout;
}
