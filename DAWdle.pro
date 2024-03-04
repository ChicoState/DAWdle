QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

DEFINES += NODE_EDITOR_SHARED

SOURCES += \
    src/arithmeticnode.cpp \
    src/audioinput.cpp \
    src/audiooutput.cpp \
    src/decimalinput.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/sinewave.cpp

HEADERS += \
    src/arithmeticnode.h \
    src/audioinput.h \
    src/audiooutput.h \
    src/bufferdata.h \
    src/decimalinput.h \
    src/mainwindow.h \
    src/sinewave.h

FORMS +=

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/lib/ -lQtNodes
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/lib/ -lQtNodesd

INCLUDEPATH += $$PWD/include
DEPENDPATH += $$PWD/include

win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/lib/QtNodes.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/lib/QtNodesd.lib

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/./release/ -lportaudio_x64
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/./debug/ -lportaudio_x64

INCLUDEPATH += $$PWD/include
DEPENDPATH += $$PWD/include
