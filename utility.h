#ifndef UTILITY_H
#define UTILITY_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>



QString XML_Parse(QString input_string, QString token);
void logMessage(QFile *logFile, QString sFunctionName, QString sMessage);

#endif // UTILITY_H
