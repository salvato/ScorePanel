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
    TimedScorePanel(QString _serverUrl, QFile *_logFile, QWidget *parent = Q_NULLPTR);
    ~TimedScorePanel();
    void closeEvent(QCloseEvent *event);

signals:
    void newTimeValue(QString);
    void newPeriodValue(QString);
    void arduinoFound();

public slots:
    void onSerialDataAvailable();
    void onArduinoConnectionTimerTimeout();
    virtual void onArduinoFound();


protected:
    void ConnectToArduino();
    int  writeSerialRequest(QByteArray requestData);
    QByteArray decodeResponse(QByteArray response);
    bool executeCommand(QByteArray command);

protected:
    QSerialPort            serialPort;
    QSerialPortInfo        serialPortinfo;
    QList<QSerialPortInfo> serialPorts;
    QSerialPort::BaudRate  baudRate;
    int                    currentPort;
    int                    waitTimeout;
    QByteArray             requestData;
    QByteArray             responseData;
    QTimer                 arduinoConnectionTimer;

    const quint8           startMarker = quint8(0xFF);
    const quint8           endMarker   = quint8(0xFE);
    const quint8           specialByte = quint8(0xFD);
    const quint8           ack         = quint8(0xFF);
};

#endif // TIMEDSCOREPANEL_H
