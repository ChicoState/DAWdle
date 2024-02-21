QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

DEFINES += NODE_EDITOR_SHARED

SOURCES += \
    src/main.cpp

HEADERS +=

FORMS +=

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/lib/ -lQtNodes
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/lib/ -lQtNodesd

INCLUDEPATH += $$PWD/include
DEPENDPATH += $$PWD/include

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/lib/libQtNodes.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/lib/libQtNodesd.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/lib/QtNodes.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/lib/QtNodesd.lib
