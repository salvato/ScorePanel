#include <QSerialPortInfo>
#include <QThread>

#include "utility.h"
#include "timedscorepanel.h"

TimedScorePanel::TimedScorePanel(QUrl _serverUrl, QFile *_logFile, QWidget *parent)
    : ScorePanel(_serverUrl, _logFile, parent)
{
    QString sFunctionName = " TimedScorePanel::TimedScorePanel ";
    Q_UNUSED(sFunctionName)
    // Arduino Serial Port
    baudRate = QSerialPort::Baud115200;
    waitTimeout = 300;
    responseData.clear();

    if(ConnectToArduino()) {
        logMessage(logFile,
                   sFunctionName,
                   QString("No Arduino ready to use !"));
    }
    else {
        connect(&serialPort, SIGNAL(readyRead()),
                this, SLOT(onSerialDataAvailable()));
    }
}


TimedScorePanel::~TimedScorePanel() {
    if(serialPort.isOpen()) {
        serialPort.waitForBytesWritten(1000);
        serialPort.close();
    }
}


void
TimedScorePanel::closeEvent(QCloseEvent *event) {
    if(serialPort.isOpen()) {
        QByteArray requestData;
        requestData.append(quint8(StopSending));
        serialPort.write(requestData.append(quint8(127)));
    }
    ScorePanel::closeEvent(event);
    event->accept();
}


int
TimedScorePanel::ConnectToArduino() {
    QString sFunctionName = " SegnapuntiBasket::ConnectToArduino ";
    Q_UNUSED(sFunctionName)
    QList<QSerialPortInfo> serialPorts = QSerialPortInfo::availablePorts();
    if(serialPorts.isEmpty()) {
        logMessage(logFile,
                   sFunctionName,
                   QString("No serial port available"));
        return -1;
    }
    bool found = false;
    QSerialPortInfo info;
    QByteArray requestData;
    for(int i=0; i<serialPorts.size()&& !found; i++) {
        info = serialPorts.at(i);
        if(!info.portName().contains("tty")) continue;
        serialPort.setPortName(info.portName());
        if(serialPort.isOpen()) continue;
        serialPort.setBaudRate(115200);
        serialPort.setDataBits(QSerialPort::Data8);
        if(serialPort.open(QIODevice::ReadWrite)) {
            // Arduino will be reset upon a serial connectiom
            // so give it time to set it up before communicating.
            QThread::sleep(3);
            requestData = QByteArray(2, quint8(AreYouThere));
            if(WriteSerialRequest(requestData) == 0) {
                found = true;
                break;
            }
            else
                serialPort.close();
        }
    }
    if(!found)
        return -1;
    logMessage(logFile,
               sFunctionName,
               QString("Arduino found at: %1")
               .arg(info.portName()));
    return 0;
}


int
TimedScorePanel::WriteSerialRequest(QByteArray requestData) {
    QString sFunctionName = " SegnapuntiBasket::WriteRequest ";
    Q_UNUSED(sFunctionName)
    if(!serialPort.isOpen()) {
        logMessage(logFile,
                   sFunctionName,
                   QString("Serial port %1 has been closed")
                   .arg(serialPort.portName()));
        return -1;
    }
    serialPort.clear();
    responseData.clear();
    serialPort.write(requestData.append(quint8(127)));
    if (serialPort.waitForBytesWritten(waitTimeout)) {
        if (serialPort.waitForReadyRead(waitTimeout)) {
            responseData = serialPort.readAll();
            while(serialPort.waitForReadyRead(1))
                responseData.append(serialPort.readAll());
            if (quint8(responseData[0]) != quint8(Ack)) {
                QString response(responseData);
                logMessage(logFile,
                           sFunctionName,
                           QString("NACK on Command %1: expecting %2 read %3")
                            .arg(quint8(requestData[0]))
                            .arg(quint8(Ack))
                            .arg(quint8(response[0].toLatin1())));
                return -1;
            }
        }
        else {// Read timeout
            logMessage(logFile,
                       sFunctionName,
                       QString(" Wait read response timeout %1 %2")
                       .arg(QTime::currentTime().toString())
                       .arg(serialPort.portName()));
            return -1;
        }
    }
    else {// Write timeout
        logMessage(logFile,
                   sFunctionName,
                   QString(" Wait write request timeout %1 %2")
                   .arg(QTime::currentTime().toString())
                   .arg(serialPort.portName()));
        return -1;
    }
    responseData.remove(0, 1);
    return 0;
}


void
TimedScorePanel::onSerialDataAvailable() {
    responseData.append(serialPort.readAll());
    while(!serialPort.atEnd()) {
        responseData.append(serialPort.readAll());
    }
    qint32 val = 0;
    while(responseData.count() > 8) {
        long imin, isec, icent;
        QString sVal;

//        val = 0;
//        for(int i=0; i<4; i++)
//            val += quint8(responseData.at(i)) << i*8;
//        isec = val/100;
//        icent = 10*((val - int(val/100)*100)/10);
////        icent = val - int(val/100)*100;
//        sVal = QString("%1:%2")
//               .arg(isec, 2, 10, QLatin1Char('0'))
//               .arg(icent, 2, 10, QLatin1Char('0'));
////        timeLabel->setText(QString(sVal));

        val = 0;
        for(int i=4; i<8; i++)
            val += quint8(responseData[i]) << (i-4)*8;
        imin = val/6000;
        isec = (val-imin*6000)/100;
        icent = 10*((val - isec*100)/10);
        if(imin > 0) {
            sVal = QString("%1:%2")
                    .arg(imin, 2, 10, QLatin1Char('0'))
                    .arg(isec, 2, 10, QLatin1Char('0'));
        }
        else {
            sVal = QString("%1:%2")
                    .arg(isec, 2, 10, QLatin1Char('0'))
                    .arg(icent, 2, 10, QLatin1Char('0'));
        }
        responseData.remove(0, 8);
        emit newTimeValue(sVal);
    }
}

