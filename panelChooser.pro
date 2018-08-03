#-------------------------------------------------
#
# Project created by QtCreator 2017-04-16T21:22:02
#
#-------------------------------------------------

QT += core
QT += gui
QT += websockets
QT += serialport
QT += widgets
contains(QMAKE_HOST.arch, "armv7l") || contains(QMAKE_HOST.arch, "armv6l"): {
    QT += dbus
}

CONFIG += c++11

TARGET = panelChooser
TEMPLATE = app

TRANSLATIONS = panelChooser_en.ts


# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += main.cpp
SOURCES += myapplication.cpp
SOURCES += timeoutwindow.cpp
SOURCES +=
SOURCES += nonetwindow.cpp
SOURCES += scorepanel.cpp
SOURCES += segnapuntibasket.cpp
SOURCES += segnapuntivolley.cpp
SOURCES += segnapuntihandball.cpp
SOURCES += serverdiscoverer.cpp
SOURCES += fileupdater.cpp
SOURCES += utility.cpp
SOURCES += timedscorepanel.cpp
contains(QMAKE_HOST.arch, "x86_64") {
    SOURCES += slidewindow.cpp
}


HEADERS +=
HEADERS += myapplication.h
HEADERS += timeoutwindow.h
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
contains(QMAKE_HOST.arch, "x86_64") {
    HEADERS += slidewindow.h
}


CONFIG += mobility
MOBILITY = 
contains(QMAKE_HOST.arch, "x86_64") {
}

contains(QMAKE_HOST.arch, "armv7l") || contains(QMAKE_HOST.arch, "armv6l"): {
    message("Running on Raspberry: Including Camera libraries")
    DBUS_INTERFACES += slidewindow.xml
    CONFIG += c++11
    INCLUDEPATH += /usr/local/include
    LIBS += -L"/usr/local/lib" -lpigpiod_if2 # To include libpigpiod_if2.so from /usr/local/lib
}


contains(QMAKE_HOST.arch, "armv7l") || contains(QMAKE_HOST.arch, "armv6l"): {
    OTHER_FILES += slidewindow.xml
}


DISTFILES += \
    android/AndroidManifest.xml \
    android/gradle/wrapper/gradle-wrapper.jar \
    android/gradlew \
    android/res/values/libs.xml \
    android/build.gradle \
    android/gradle/wrapper/gradle-wrapper.properties \
    android/gradlew.bat

RESOURCES += \
    panelchooser.qrc

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
