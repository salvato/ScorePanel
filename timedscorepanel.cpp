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

    ConnectToArduino();
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


void
TimedScorePanel::ConnectToArduino() {
    QString sFunctionName = " TimedScorePanel::ConnectToArduino ";
    Q_UNUSED(sFunctionName)
    // Get a list of available serial ports
    serialPorts = QSerialPortInfo::availablePorts();
    // Remove from the list NON tty and already opened devices
    for(int i=0; i<serialPorts.count(); i++) {
        serialPortinfo = serialPorts.at(i);
        if(!serialPortinfo.portName().contains("tty"))
            serialPorts.removeAt(i);
        serialPort.setPortName(serialPortinfo.portName());
        if(serialPort.isOpen())
            serialPorts.removeAt(i);
    }
    // Do we have still serial ports ?
    if(serialPorts.isEmpty()) {
        logMessage(logFile,
                   sFunctionName,
                   QString("No serial port available"));
        return;
    }
    // Yes we have serial ports available:
    // Search for the one connected to Arduino
    baudRate = QSerialPort::Baud115200;
    waitTimeout = 3000;
    connect(&arduinoConnectionTimer, SIGNAL(timeout()),
            this, SLOT(onConnectionTimerTimeout()));

    requestData.clear();
    requestData.append(quint8(startMarker));
    requestData.append(quint8(4));
    requestData.append(quint8(AreYouThere));
    requestData.append(quint8(endMarker));

    for(currentPort=0; currentPort<serialPorts.count(); currentPort++) {
        serialPortinfo = serialPorts.at(currentPort);
        serialPort.setPortName(serialPortinfo.portName());
        serialPort.setBaudRate(baudRate);
        serialPort.setDataBits(QSerialPort::Data8);
        if(serialPort.open(QIODevice::ReadWrite)) {
            // Arduino will be reset upon a serial connectiom
            // so give time to set it up before communicating.
            QThread::sleep(3);
            serialPort.clear();
            connect(&serialPort, SIGNAL(readyRead()),
                    this, SLOT(onSerialDataAvailable()));
            writeSerialRequest(requestData);
            arduinoConnectionTimer.start(waitTimeout);
            break;
        }
    }
    if(currentPort >= serialPorts.count()) {
        logMessage(logFile,
                   sFunctionName,
                   QString("Error: No Arduino ready to use !"));
        return;
    }
}


void
TimedScorePanel::onArduinoFound() {
}


void
TimedScorePanel::onConnectionTimerTimeout() {
    QString sFunctionName(" TimedScorePanel::onConnectionTimerTimeout ");
    arduinoConnectionTimer.stop();
    disconnect(&serialPort, 0, 0, 0);
    serialPort.close();
    for(++currentPort; currentPort<serialPorts.count(); currentPort++) {
        serialPortinfo = serialPorts.at(currentPort);
        serialPort.setPortName(serialPortinfo.portName());
        serialPort.setBaudRate(baudRate);
        serialPort.setDataBits(QSerialPort::Data8);
        if(serialPort.open(QIODevice::ReadWrite)) {
            // Arduino will be reset upon a serial connectiom
            // so give time to set it up before communicating.
            QThread::sleep(3);
            serialPort.clear();
            connect(&serialPort, SIGNAL(readyRead()),
                    this, SLOT(onSerialDataAvailable()));
            writeSerialRequest(requestData);
            arduinoConnectionTimer.start(waitTimeout);
            break;
        }
    }
    if(currentPort>=serialPorts.count()) {
        logMessage(logFile,
                   sFunctionName,
                   QString("Error: No Arduino ready to use !"));
        return;
    }
}


int
TimedScorePanel::writeSerialRequest(QByteArray requestData) {
    QString sFunctionName = " TimedScorePanel::WriteRequest ";
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
    serialPort.write(requestData);
    return 0;
}


void
TimedScorePanel::onSerialDataAvailable() {
    QSerialPort *testPort = static_cast<QSerialPort *>(sender());
    responseData.append(testPort->readAll());
    while(!testPort->atEnd()) {
        responseData.append(testPort->readAll());
    }
    while(responseData.count() > 0) {
        // Do we have a complete command ?
        int iStart = responseData.indexOf(quint8(startMarker));
        if(iStart == -1) {
            responseData.clear();
            return;
        }
        if(iStart > 0)
            responseData.remove(0, iStart);
        int iEnd   = responseData.indexOf(quint8(endMarker));
        if(iEnd == -1) return;
        executeCommand(decodeResponse(responseData.left(responseData[1])));
        responseData.remove(0, responseData[1]);
    }
}


QByteArray
TimedScorePanel::decodeResponse(QByteArray response) {
    QByteArray decodedResponse;
    int iStart;
    for(iStart=0; iStart<response.count(); iStart++) {
        if(quint8(response[iStart]) == quint8(startMarker)) break;
    }
    for(int i=iStart+1; i<response.count(); i++) {
        if(quint8(response[i]) == endMarker) {
            break;
        }
        if((quint8(response[i]) == quint8(specialByte))) {
            i++;
            if(i<response.count()) {
                decodedResponse.append(response[i-1]+response[i]);
            }
            else { // Not enough character received
                decodedResponse.clear();
                return decodedResponse;
           }
        }
        decodedResponse.append(response[i]);
    }
    if(decodedResponse.count() != 0)
        decodedResponse[0] = decodedResponse[0]-2;
    return decodedResponse;
}


bool
TimedScorePanel::executeCommand(QByteArray command) {
    QString sFunctionName = " TimedScorePanel::executeCommand ";
    Q_UNUSED(sFunctionName)
    if(quint8(command[1]) == quint8(AreYouThere)) {
        arduinoConnectionTimer.stop();
        logMessage(logFile,
                   sFunctionName,
                   QString("Arduino found at: %1")
                   .arg(serialPort.portName()));
        emit arduinoFound();
        return true;
    }

    if(quint8(command[1]) == quint8(Time)) {
        if(quint8(command[0]) != quint8(6)) return false;
        quint32 time = 0;
        for(int i=2; i<6; i++) {
            time += quint32(quint8(command[i])) << ((i-2)*8);
        }
        int imin = time/6000;
        int isec = (time-imin*6000)/100;
        int icent = 10*((time - isec*100)/10);
        QString sVal;
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
        emit newTimeValue(sVal);
        return true;
    }

    if(quint8(command[1]) == quint8(Possess)) {
        if(quint8(command[0]) != quint8(6)) return false;
        quint32 time = 0;
        for(int i=2; i<6; i++) {
            time += quint32(quint8(command[i])) << ((i-2)*8);
        }
        int isec = time/100;
        int icent = 10*((time - isec*100)/10);
        QString sVal;
        if(isec > 0) {
            sVal = QString("%1:%2")
                    .arg(isec, 2, 10, QLatin1Char('0'))
                    .arg(icent, 2, 10, QLatin1Char('0'));
        }
        else {
            sVal = QString("%1:%2")
                    .arg(icent, 2, 10, QLatin1Char('0'))
                    .arg(0, 2, 10, QLatin1Char('0'));
        }
        emit newPeriodValue(sVal);
        return true;
    }
    return false;
}
