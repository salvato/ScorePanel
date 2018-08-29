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
#ifndef TIMEDSCOREPANEL_H
#define TIMEDSCOREPANEL_H

#include <QObject>
#include <QWidget>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>

#include "scorepanel.h"

class TimedScorePanel : public ScorePanel
{
    Q_OBJECT

public:
    TimedScorePanel(QString myServerUrl, QFile *myLogFile, QWidget *parent = Q_NULLPTR);
    ~TimedScorePanel();
    void closeEvent(QCloseEvent *event);

signals:
#ifndef Q_OS_ANDROID
    void arduinoFound();/*!< Emitted whe the Arduino send the right answer */
    void newTimeValue(QString);/*!< Emitted when the window must show a new time value */
#endif

public slots:
#ifndef Q_OS_ANDROID
    void onSerialDataAvailable();
    void onArduinoConnectionTimerTimeout();
    virtual void onArduinoFound();
#endif

protected:
#ifndef Q_OS_ANDROID
    void ConnectToArduino();
    int writeArduinoSimpleCommand(char command);
    int  writeSerialRequest(QByteArray requestData);
    QByteArray decodeResponse(QByteArray response);
    bool executeCommand(QByteArray command);
#endif

protected:
    bool                   isArduinoFound;
#ifndef Q_OS_ANDROID
    QByteArray             requestData;/*!< The string sent to the Arduino */

    const char             startMarker = char(0xFF);/*!< The start string character sent to the Arduino */
    const char             endMarker   = char(0xFE);/*!< The end string character sent to the Arduino */
    const char             specialByte = char(0xFD);/*!< The pecial continuation character sent to the Arduino */
    const char             ack         = char(0xFF);/*!< The acknowledge character sent to the Arduino */

private:
    QSerialPort            serialPort;
    QSerialPortInfo        serialPortinfo;
    QList<QSerialPortInfo> serialPorts;
    QSerialPort::BaudRate  baudRate;
    int                    currentPort;
    int                    waitTimeout;
    QByteArray             responseData;
    QTimer                 arduinoConnectionTimer;
#endif
};

#endif // TIMEDSCOREPANEL_H
