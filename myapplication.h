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
#ifndef MYAPPLICATION_H
#define MYAPPLICATION_H

#include <QApplication>
#include <QTranslator>
#include <QTimer>
#include <QDateTime>


QT_FORWARD_DECLARE_CLASS(QSettings)
QT_FORWARD_DECLARE_CLASS(ServerDiscoverer)
QT_FORWARD_DECLARE_CLASS(MessageWindow)
QT_FORWARD_DECLARE_CLASS(QFile)


class MyApplication : public QApplication
{
    // Every class that implements its own slots/signals needs that macro
    Q_OBJECT
public:
    MyApplication(int& argc, char ** argv);

private slots:
    void onTimeToCheckNetwork();
    void onRecheckNetwork();

private:
    bool isConnectedToNetwork();
    bool PrepareLogFile();

private:
    QSettings         *pSettings;
    QFile             *logFile;
    ServerDiscoverer  *pServerDiscoverer;
    MessageWindow     *pNoNetWindow;
    QTranslator        Translator;
    QString            sLanguage;
    QString            logFileName;
    QTimer             networkReadyTimer;
};

#endif // MYAPPLICATION_H
