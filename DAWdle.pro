QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

DEFINES += NODE_EDITOR_SHARED

SOURCES += \
    src/arithmeticnode.cpp \
    src/audioinput.cpp \
    src/audiooutput.cpp \
    src/boxlayoutwrapper.cpp \
    src/decimalinput.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/sampler.cpp \
    src/wavenode.cpp

HEADERS += \
    src/arithmeticnode.h \
    src/audioinput.h \
    src/audiooutput.h \
    src/boxlayoutwrapper.h \
    src/bufferdata.h \
    src/decimalinput.h \
    src/mainwindow.h \
    src/sampler.h \
    src/util.h \
    src/wavenode.h

FORMS +=

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

INCLUDEPATH += $$PWD/include
DEPENDPATH += $$PWD/include

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/lib/ -lQtNodes
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/lib/ -lQtNodesd

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/./release/ -lportaudio_x64
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/./debug/ -lportaudio_x64

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/lib/ -llibsoundwave
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/lib/ -llibsoundwaved

win32:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/lib/libsoundwave.lib
else:win32:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/lib/libsoundwaved.lib

LIBS += -luser32
