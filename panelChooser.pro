#-------------------------------------------------
#
# Project created by QtCreator 2017-04-16T21:22:02
#
#-------------------------------------------------

QT += core
QT += gui
QT += websockets
QT += serialport

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


SOURCES += main.cpp\
    nonetwindow.cpp \
    scorepanel.cpp \
    segnapuntibasket.cpp \
    segnapuntivolley.cpp \
    serverdiscoverer.cpp \
    slidewindow.cpp \
    fileupdater.cpp \
    chooserwidget.cpp \
    utility.cpp \
    segnapuntihandball.cpp \
    timedscorepanel.cpp

HEADERS  += chooserwidget.h \
    nonetwindow.h \
    scorepanel.h \
    segnapuntibasket.h \
    segnapuntivolley.h \
    serverdiscoverer.h \
    slidewindow.h \
    fileupdater.h \
    utility.h \
    panelorientation.h \
    segnapuntihandball.h \
    timedscorepanel.h

RESOURCES += scorepanel.qrc

CONFIG += mobility
MOBILITY = 


contains(QMAKE_HOST.arch, "armv7l") || contains(QMAKE_HOST.arch, "armv6l"):{
    CONFIG += c++11
    message("Running on Raspberry: Including Camera libraries")
    INCLUDEPATH += /usr/local/include
    LIBS += -L"/usr/local/lib" -lpigpiod_if2 # To include libpigpiod_if2.so from /usr/local/lib
}

DISTFILES +=
