#include <QSerialPortInfo>
#include <QThread>
#include <QCloseEvent>

#include "utility.h"
#include "timedscorepanel.h"

TimedScorePanel::TimedScorePanel(QUrl _serverUrl, QFile *_logFile, QWidget *parent)
    : ScorePanel(_serverUrl, _logFile, parent)
{
    // Arduino Serial Port
    baudRate = QSerialPort::Baud115200;
    waitTimeout = 1000;
    responseData.clear();

    ConnectToArduino();
}


TimedScorePanel::~TimedScorePanel() {
    if(serialPort.isOpen()) {
        requestData.clear();
        requestData.append(quint8(startMarker));
        requestData.append(quint8(4));
        requestData.append(quint8(StopSending));
        requestData.append(quint8(endMarker));
        writeSerialRequest(requestData);
        serialPort.waitForBytesWritten(1000);// in msec
        QThread::sleep(1);// In sec. Give Arduino the time to react
        serialPort.clear();
        serialPort.close();
    }
}


void
TimedScorePanel::closeEvent(QCloseEvent *event) {
    Q_UNUSED(event)
    if(serialPort.isOpen()) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Closing serial port %1")
                   .arg(serialPort.portName()));
        requestData.clear();
        requestData.append(quint8(startMarker));
        requestData.append(quint8(4));
        requestData.append(quint8(StopSending));
        requestData.append(quint8(endMarker));
        writeSerialRequest(requestData);
        if(!serialPort.waitForBytesWritten(1000)) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to Close serial port %1")
                       .arg(serialPort.portName()));
        }
        QThread::sleep(1);// In sec. Give Arduino the time to react
        serialPort.clear();
        serialPort.close();
    }
}


void
TimedScorePanel::ConnectToArduino() {
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
                   Q_FUNC_INFO,
                   QString("No serial port available"));
        return;
    }
    connect(this, SIGNAL(arduinoFound()),
            this, SLOT(onArduinoFound()));
    // Yes we have serial ports available:
    // Search for the one connected to Arduino
    baudRate = QSerialPort::Baud115200;
    waitTimeout = 1000;
    connect(&arduinoConnectionTimer, SIGNAL(timeout()),
            this, SLOT(onArduinoConnectionTimerTimeout()));

    //// The request for connection to the Arduino.
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
#ifdef LOG_VERBOSE
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Trying connection to %1")
                       .arg(serialPortinfo.portName()));
#endif
            // Arduino will be reset upon a serial connectiom
            // so give time to set it up before communicating.
            QThread::sleep(3);// In sec.
            serialPort.clear();
            connect(&serialPort, SIGNAL(readyRead()),
                    this, SLOT(onSerialDataAvailable()));
            writeSerialRequest(requestData);
            arduinoConnectionTimer.start(waitTimeout);
            return;
        }
#ifdef LOG_VERBOSE
        else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to open %1 because %2")
                       .arg(serialPortinfo.portName())
                       .arg(serialPort.errorString()));
        }
#endif
    }
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Error: No Arduino ready to use !"));
}


void
TimedScorePanel::onArduinoFound() {
    // The event is handele in the derived classes
}


void
TimedScorePanel::onArduinoConnectionTimerTimeout() {
    arduinoConnectionTimer.stop();
    serialPort.disconnect();
    serialPort.close();
    for(++currentPort; currentPort<serialPorts.count(); currentPort++) {
        serialPortinfo = serialPorts.at(currentPort);
        serialPort.setPortName(serialPortinfo.portName());
        serialPort.setBaudRate(baudRate);
        serialPort.setDataBits(QSerialPort::Data8);
        if(serialPort.open(QIODevice::ReadWrite)) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Trying connection to %1")
                       .arg(serialPortinfo.portName()));
            // Arduino will be reset upon a serial connectiom
            // so give time to set it up before communicating.
            QThread::sleep(3);
            serialPort.clear();
            connect(&serialPort, SIGNAL(readyRead()),
                    this, SLOT(onSerialDataAvailable()));
            writeSerialRequest(requestData);
            arduinoConnectionTimer.start(waitTimeout);
            return;
        }
#ifdef LOG_VERBOSE
        else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to open %1 because %2")
                       .arg(serialPortinfo.portName())
                       .arg(serialPort.errorString()));
        }
#endif
    }
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Error: No Arduino ready to use !"));
}


int
TimedScorePanel::writeSerialRequest(QByteArray requestData) {
    if(!serialPort.isOpen()) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Serial port %1 has been closed")
                   .arg(serialPort.portName()));
        return -1;
    }
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
    if(quint8(command[1]) == quint8(AreYouThere)) {
        arduinoConnectionTimer.stop();
        logMessage(logFile,
                   Q_FUNC_INFO,
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
        // Per fare in modo che il tempo non decrementi i
        // secondi immediatamente alla pressione del pulsante
        if(time>6000)
            time +=99;
        int imin = time/6000;
        int isec = (time-imin*6000)/100;
        // Eliminare il commento se non si vogliono
        // visualizzare i centesimi di secondo
        //int icent = 10*((time - isec*100)/10);
        int icent = (time - isec*100);
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
        // Eliminare il commento se non si vogliono
        // visualizzare i centesimi di secondo
        //int icent = 10*((time - isec*100)/10);
        int icent = (time - isec*100);
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

