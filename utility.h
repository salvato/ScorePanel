/*
 *
Copyright (C) 2016  Gabriele Salvato

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/
#ifndef UTILITY_H
#define UTILITY_H

#include <QString>
#include <QFile>

//#define LOG_MESG
#define LOG_VERBOSE


#define VOLLEY_PANEL   0
#define FIRST_PANEL  VOLLEY_PANEL
#define BASKET_PANEL   1
#define HANDBALL_PANEL 2
#define LAST_PANEL   HANDBALL_PANEL


enum commands {
    AreYouThere    = 0xAA,
    Stop           = 0x01,
    Start          = 0x02,
    Start14        = 0x04,
    NewGame        = 0x11,
    RadioInfo      = 0x21,
    Configure      = 0x31,
    Time           = 0x41,
    Possess        = 0x42,
    StartSending   = 0x81,
    StopSending    = 0x82
};


QString XML_Parse(QString input_string, QString token);
void logMessage(QFile *logFile, QString sFunctionName, QString sMessage);

#endif // UTILITY_H
