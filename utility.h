#ifndef UTILITY_H
#define UTILITY_H

#include <QString>
#include <QFile>

#define LOG_MESG
//#define LOG_VERBOSE


#define VOLLEY_PANEL   0
#define FIRST_PANEL  VOLLEY_PANEL
#define BASKET_PANEL   1
#define HANDBALL_PANEL 2
#define LAST_PANEL   HANDBALL_PANEL


#define Ack            0xFF
#define AreYouThere    0xAA
#define Stop           0x01
#define Start          0x02
#define NewPeriod      0x11
#define StopSending    0x81


QString XML_Parse(QString input_string, QString token);
void logMessage(QFile *logFile, QString sFunctionName, QString sMessage);

#endif // UTILITY_H
