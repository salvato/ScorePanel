# Copyright (C) 2016  Gabriele Salvato

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

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
#contains(QMAKE_HOST.arch, "armv8l") ||
#contains(QMAKE_HOST.arch, "armv7l") ||
#contains(QMAKE_HOST.arch, "armv6l"): {
#    QT += dbus
#}

CONFIG += c++11

Linux {
    # to Add a different Build number after a new Build
    build_nr.commands = ../scoreController/build_number.sh
    build_nr.depends = FORCE
    QMAKE_EXTRA_TARGETS += build_nr
    PRE_TARGETDEPS += build_nr
}

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
SOURCES += messagewindow.cpp
SOURCES += scorepanel.cpp
SOURCES += segnapuntibasket.cpp
SOURCES += segnapuntivolley.cpp
SOURCES += segnapuntihandball.cpp
SOURCES += serverdiscoverer.cpp
SOURCES += fileupdater.cpp
SOURCES += utility.cpp
SOURCES += timedscorepanel.cpp
SOURCES += slidewindow.cpp


HEADERS += myapplication.h
HEADERS += build_number.h
HEADERS += timeoutwindow.h
HEADERS += messagewindow.h
HEADERS += scorepanel.h
HEADERS += segnapuntibasket.h
HEADERS += segnapuntivolley.h
HEADERS += segnapuntihandball.h
HEADERS += serverdiscoverer.h
HEADERS += fileupdater.h
HEADERS += utility.h
HEADERS += timedscorepanel.h
HEADERS += panelorientation.h
HEADERS += slidewindow.h


CONFIG += mobility
MOBILITY = 
contains(QMAKE_HOST.arch, "x86_64") {
}

#message("Present Build: " $$cat(../scoreController/build_number))

#contains(QMAKE_HOST.arch, "armv7l") || contains(QMAKE_HOST.arch, "armv6l"): {
#    message("Running on Raspberry: Including Camera libraries")
#    DBUS_INTERFACES += slidewindow.xml
#    CONFIG += c++11
#    INCLUDEPATH += /usr/local/include
#    LIBS += -L"/usr/local/lib" -lpigpiod_if2 # To include libpigpiod_if2.so from /usr/local/lib
#}

##QMAKEarm64-v8a
#contains(QMAKE_HOST.arch, "armv7l") || contains(QMAKE_HOST.arch, "armv6l"): {
#    OTHER_FILES += slidewindow.xml
#}


DISTFILES += \
    android/AndroidManifest.xml \
    android/gradle/wrapper/gradle-wrapper.jar \
    android/gradlew \
    android/res/values/libs.xml \
    android/build.gradle \
    android/gradle/wrapper/gradle-wrapper.properties \
    android/gradlew.bat \
    build_number.sh \
    build_number \

RESOURCES += \
    panelchooser.qrc

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
