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

#ifndef SEGNAPUNTIHANDBALL_H
#define SEGNAPUNTIHANDBALL_H


#include <QObject>
#include <QWidget>
#include <QSerialPort>


#include "slidewindow.h"
#include "nonetwindow.h"
#include "serverdiscoverer.h"
#include "scorepanel.h"


QT_FORWARD_DECLARE_CLASS(QSettings)
QT_FORWARD_DECLARE_CLASS(QLCDNumber)
QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QGridLayout)


class SegnapuntiHandball : public ScorePanel
{
private:
    Q_OBJECT
public:
    SegnapuntiHandball(QUrl _serverUrl, QFile *_logFile);
    ~SegnapuntiHandball();
    void closeEvent(QCloseEvent *event);

private:
    QLabel            *team[2];
    QLCDNumber        *score[2];
    QLCDNumber        *period;
    QLabel            *timeLabel;
    QLabel            *timeout[2];
    QSettings         *pSettings;
    QPalette           pal;
    int                maxTeamNameLen;
    int                iTimeoutFontSize;
    int                iTimeFontSize;
    int                iTeamFontSize;

public slots:
    void resizeEvent(QResizeEvent *event);
    void onSerialDataAvailable();
    void onTextMessageReceived(QString sMessage);
    void onBinaryMessageReceived(QByteArray baMessage);

protected:
    void                   buildFontSizes();
    void                   createPanelElements();
    QGridLayout           *createPanel();
    void                   buildLayout();
    int                    ConnectToArduino();
    int                    writeSerialRequest(QByteArray requestData);

    QSerialPort            serialPort;
    QSerialPort::BaudRate  baudRate;
    int                    waitTimeout;
    QByteArray             responseData;
};

#endif // SEGNAPUNTIHANDBALL_H
