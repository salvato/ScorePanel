#ifndef TIMEDSCOREPANEL_H
#define TIMEDSCOREPANEL_H

#include <QObject>
#include <QWidget>
#include <QSerialPort>

#include "scorepanel.h"

class TimedScorePanel : public ScorePanel
{
    Q_OBJECT

public:
    TimedScorePanel(QUrl _serverUrl, QFile *_logFile, QWidget *parent = 0);
    ~TimedScorePanel();
    void closeEvent(QCloseEvent *event);

signals:
    void newTimeValue(QString);

public slots:
    void onSerialDataAvailable();

protected:
    int                    ConnectToArduino();
    int                    WriteSerialRequest(QByteArray requestData);

protected:
    QSerialPort            serialPort;
    QSerialPort::BaudRate  baudRate;
    int                    waitTimeout;
    QByteArray             responseData;
};

#endif // TIMEDSCOREPANEL_H
