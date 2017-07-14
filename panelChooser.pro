#-------------------------------------------------
#
# Project created by QtCreator 2017-04-16T21:22:02
#
#-------------------------------------------------

QT += core
QT += gui
QT += websockets
QT += serialport
QT += dbus

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = panelChooser
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += main.cpp
SOURCES += chooserwidget.cpp
SOURCES += nonetwindow.cpp
SOURCES += scorepanel.cpp
SOURCES += segnapuntibasket.cpp
SOURCES += segnapuntivolley.cpp
SOURCES += segnapuntihandball.cpp
SOURCES += serverdiscoverer.cpp
SOURCES += fileupdater.cpp
SOURCES += utility.cpp
SOURCES += timedscorepanel.cpp

HEADERS += chooserwidget.h
HEADERS += nonetwindow.h
HEADERS += scorepanel.h
HEADERS += segnapuntibasket.h
HEADERS += segnapuntivolley.h
HEADERS += segnapuntihandball.h
HEADERS += serverdiscoverer.h
HEADERS += fileupdater.h
HEADERS += utility.h
HEADERS += timedscorepanel.h
HEADERS += panelorientation.h

RESOURCES += scorepanel.qrc

CONFIG += mobility
MOBILITY = 

contains(QMAKE_HOST.arch, "armv7l") || contains(QMAKE_HOST.arch, "armv6l"): {
    message("Running on Raspberry: Including Camera libraries")
    DBUS_INTERFACES += slidewindow.xml
    DBUS_ADAPTORS   += slidewindow.xml
    CONFIG += c++11
    SOURCES += slidewindow2.cpp
    HEADERS += slidewindow2.h
    INCLUDEPATH += /usr/local/include
    INCLUDEPATH += /opt/vc/include
    LIBS += -L"/usr/local/lib" -lpigpiod_if2 # To include libpigpiod_if2.so from /usr/local/lib
    LIBS += -L"/opt/vc/lib" -lbrcmGLESv2 -lbrcmEGL -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread -lrt -lm
}
else {
    SOURCES += slidewindow.cpp
    HEADERS += slidewindow.h
}

DISTFILES +=

OTHER_FILES += slidewindow.xml

RESOURCES += shaders.qrc
