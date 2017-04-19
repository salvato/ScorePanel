#include "fileupdater.h"
#include <QtNetwork>
#include <QTime>
#include <QTimer>


#define LOG_MESG


#define RETRY_TIME 15000
#define CHUNK_SIZE 1024*1024


FileUpdater::FileUpdater(QUrl _serverUrl, QFile *_logFile, QObject *parent)
    : QObject(parent)
    , logFile(_logFile)
    , serverUrl(_serverUrl)
{
    pUpdateSocket = NULL;
    bTrasferError = false;
    fileList = QList<spot>();
    destinationDir = QString(".");
    sNoData = QString(tr("NoData"));
    sDebugInformation.setString(&sDebugMessage);
    bytesReceived = 0;

    pReconnectionTimer = new QTimer(this);
    connect(pReconnectionTimer, SIGNAL(timeout()),
            this, SLOT(retryReconnection()));

    connect(this, SIGNAL(openFileError()),
            this, SLOT(onOpenFileError()));
    connect(this, SIGNAL(writeFileError()),
            this, SLOT(onWriteFileError()));
}


FileUpdater::~FileUpdater() {
    disconnect(pReconnectionTimer, 0, 0, 0);
    pReconnectionTimer->stop();
}


void
FileUpdater::stop() {
    disconnect(pReconnectionTimer, 0, 0, 0);
    pReconnectionTimer->stop();
    disconnect(pUpdateSocket, 0,0,0);
    pUpdateSocket->deleteLater();
}


void
FileUpdater::logMessage(QString sFunctionName, QString sMessage) {
    Q_UNUSED(sFunctionName)
    Q_UNUSED(sMessage)
#ifdef LOG_MESG
    sDebugMessage = QString();
    sDebugInformation << dateTime.currentDateTime().toString()
                      << sFunctionName
                      << sMessage;
    if(logFile) {
      logFile->write(sDebugMessage.toUtf8().data());
      logFile->write("\n");
      logFile->flush();
    }
    else {
        qDebug() << sDebugMessage;
    }
#endif
}


void
FileUpdater::setFileList(QList<spot> _fileList) {
    fileList = _fileList;
}


void
FileUpdater::setDestinationDir(QString _destinationDir) {
    QString sFunctionName = " FileUpdater::setDestinationDir ";
    destinationDir = _destinationDir;
    QDir outDir(destinationDir);
    if(!outDir.exists()) {
        logMessage(sFunctionName,
                   QString("Creating new directory: %1")
                   .arg(destinationDir));
        if(!outDir.mkdir(destinationDir)) {
            logMessage(sFunctionName,
                       QString("Unable to create directory: %1")
                       .arg(destinationDir));
        }
    }
}


void
FileUpdater::updateFiles() {
    QString sFunctionName = " FileUpdater::updateFiles ";
    Q_UNUSED(sFunctionName)
    int retryTime = int(0.2*RETRY_TIME * (1.0 + double(qrand())/double(RAND_MAX)));
    pReconnectionTimer->start(retryTime);
}


void
FileUpdater::retryReconnection() {
    QString sFunctionName = " FileUpdater::retryReconnection ";
    logMessage(sFunctionName, QString("Trying to connect..."));
    if(pUpdateSocket != Q_NULLPTR) {
        QAbstractSocket::SocketState socketState = pUpdateSocket->state();
        if((socketState == QAbstractSocket::UnconnectedState) ||
           (socketState == QAbstractSocket::ClosingState)) {
            pUpdateSocket->deleteLater();
            connectToServer();
        }
        else {
            return;
        }
     }
    else {
        connectToServer();
    }
}


void
FileUpdater::connectToServer() {
    QString sFunctionName = " FileUpdater::connectToServer ";
    logMessage(sFunctionName,
               QString("Connecting to file server: %1")
               .arg(serverUrl.toString()));

    pUpdateSocket = new QWebSocket();
    previousSocketState = pUpdateSocket->state();

    connect(pUpdateSocket, SIGNAL(connected()),
            this, SLOT(onUpdateSocketConnected()));

    connect(pUpdateSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onUpdateSocketError(QAbstractSocket::SocketError)));

    connect(pUpdateSocket, SIGNAL(textMessageReceived(QString)),
            this, SLOT(onProcessTextMessage(QString)));

    connect(pUpdateSocket, SIGNAL(binaryFrameReceived(QByteArray, bool)),
            this,SLOT(onProcessBinaryFrame(QByteArray, bool)));

    connect(pUpdateSocket, SIGNAL(disconnected()),
            this, SLOT(onServerDisconnected()));

    connect(pUpdateSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
            this, SLOT(onUpdateSocketChangedState(QAbstractSocket::SocketState)));

    pUpdateSocket->open(QUrl(serverUrl));
}


void
FileUpdater::onUpdateSocketChangedState(QAbstractSocket::SocketState newSocketState) {
    QString sFunctionName = " FileUpdater::onUpdateSocketChangedState ";
    QString stateString;

    switch(newSocketState) {
    case QAbstractSocket::UnconnectedState:
        if(previousSocketState == QAbstractSocket::ConnectingState) {
            pUpdateSocket->deleteLater();
            connectToServer();
        }
        previousSocketState = newSocketState;
        stateString = QString("QAbstractSocket::UnconnectedState");
        break;
    case QAbstractSocket::HostLookupState:
        previousSocketState = newSocketState;
        stateString = QString("QAbstractSocket::HostLookupState");
        break;
    case QAbstractSocket::ConnectingState:
        previousSocketState = newSocketState;
        stateString = QString("QAbstractSocket::ConnectingState");
        pReconnectionTimer->stop();
        break;
    case QAbstractSocket::ConnectedState:
        previousSocketState = newSocketState;
        stateString = QString("QAbstractSocket::ConnectedState");
        break;
    case QAbstractSocket::BoundState:
        previousSocketState = newSocketState;
        stateString = QString("QAbstractSocket::BoundState");
        break;
    case QAbstractSocket::ClosingState:
        previousSocketState = newSocketState;
        stateString = QString("QAbstractSocket::ClosingState");
        break;
    case QAbstractSocket::ListeningState:
        previousSocketState = newSocketState;
        stateString = QString("QAbstractSocket::ListeningState");
        break;
    default:
        stateString = QString("QAbstractSocket::UnknownState");
    }
//    logMessage(sFunctionName,
//               QString("The state of the socket is now: %1")
//               .arg(stateString));
}


void
FileUpdater::onUpdateSocketConnected() {
    QString sFunctionName = " FileUpdater::onUpdateSocketConnected ";
    logMessage(sFunctionName,
               QString("FileUpdater connected to: %1")
               .arg(pUpdateSocket->peerAddress().toString()));
    askSpotList();
}


void
FileUpdater::askSpotList() {
    QString sFunctionName = " FileUpdater::askSpotList ";
    Q_UNUSED(sFunctionName)
    if(pUpdateSocket->isValid()) {
        QString sMessage;
        sMessage = QString("<send_spot_list>1</send_spot_list>");
        qint64 bytesSent = pUpdateSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(sFunctionName, QString("Unable to ask for spot list"));
            pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString("Unable to ask for spot list"));
        }
        else {
            logMessage(sFunctionName,
                       QString("Sent %1 to %2")
                       .arg(sMessage)
                       .arg(pUpdateSocket->peerAddress().toString()));
        }
    }
}


void
FileUpdater::onServerDisconnected() {
    QString sFunctionName = " FileUpdater::onServerDisconnected ";
    logMessage(sFunctionName,
               QString("WebSocket disconnected from: %1")
               .arg(pUpdateSocket->peerAddress().toString()));
    disconnect(pReconnectionTimer, 0, 0, 0);
    pReconnectionTimer->stop();
    disconnect(pUpdateSocket, 0,0,0);
    pUpdateSocket->deleteLater();
}


void
FileUpdater::onUpdateSocketError(QAbstractSocket::SocketError error) {
    QString sFunctionName = " FileUpdater::onUpdateSocketError ";
    Q_UNUSED(error)
    Q_UNUSED(sFunctionName)
    logMessage(sFunctionName,
               QString("%1 %2 Error %3")
               .arg(pUpdateSocket->localAddress().toString())
               .arg(pUpdateSocket->errorString())
               .arg(error));
    if(!disconnect(pUpdateSocket, 0, 0, 0)) {
        logMessage(sFunctionName, QString("Unable to disconnect signals from WebSocket"));
    }
    pUpdateSocket->abort();
    if((error == QAbstractSocket::RemoteHostClosedError) ||
       (error == QAbstractSocket::SocketTimeoutError)    ||
       (error == QAbstractSocket::NetworkError)          ||
       (error == QAbstractSocket::TemporaryError)        ||
       (error == QAbstractSocket::UnknownSocketError))
    {
        int retryTime = int(RETRY_TIME * (1.0 + double(qrand())/double(RAND_MAX)));
        pReconnectionTimer->start(retryTime);
    }
    else {
        emit connectionClosed(true);
    }
}


void
FileUpdater::onProcessBinaryFrame(QByteArray baMessage, bool isLastFrame) {
    QString sFunctionName = " FileUpdater::onProcessBinaryFrame ";
    int len, written;
    if(bytesReceived == 0) {// It's a new file...
        QByteArray header = baMessage.left(1024);// Get the header...
        int iComma = header.indexOf(",");
        sFileName = QString(header.left(iComma));
        header = header.mid(iComma+1);
        iComma = header.indexOf('\0');
        len = header.left(iComma).toInt();// Get the file length...
        QStringList sTemp = sFileName.split(".", QString::SkipEmptyParts);
        QString sExtension = sTemp.last();// Get the file extension...
        if(sExtension == QString("mp4")) {
            file.setFileName(destinationDir + sFileName);
            file.remove();
            file.setFileName(destinationDir + sFileName + QString(".temp"));
            file.remove();// Just in case of a previous aborted transfer
            if(file.open(QIODevice::Append)) {
                len = baMessage.size()-1024;
                written = file.write(baMessage.mid(1024));
                bytesReceived += written;
                if(len != written) {
                    logMessage(sFunctionName,
                               QString("File lenght mismatch in: %1 written(%2/%3)")
                               .arg(sFileName)
                               .arg(written)
                               .arg(len));
                    emit writeFileError();
                }
            } else {
                logMessage(sFunctionName,
                           QString("Unable to write file: %1")
                           .arg(sFileName));
                emit openFileError();
                return;
            }
        }
    }
    else {// It's a new frame for an already opened file...
        len = baMessage.size();
        written = file.write(baMessage);
        bytesReceived += written;
        if(len != written) {
            logMessage(sFunctionName,
                       QString("File lenght mismatch in: %1 written(%2/%3)")
                       .arg(sFileName)
                       .arg(written)
                       .arg(len));
            emit writeFileError();
            return;
        }
    }
    logMessage(sFunctionName, QString("Received %1 bytes").arg(bytesReceived));
    if(isLastFrame) {
        if(bytesReceived < fileList.last().spotFileSize) {
            QString sMessage = QString("<get>%1,%2,%3</get>")
                               .arg(fileList.last().spotFilename)
                               .arg(bytesReceived)
                               .arg(CHUNK_SIZE);
            written = pUpdateSocket->sendTextMessage(sMessage);
            if(written != sMessage.length()) {
                logMessage(sFunctionName, QString("Error writing %1").arg(sMessage));
                pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString("Error writing %1").arg(sMessage));
            }
            else {
                logMessage(sFunctionName,
                           QString("Sent %1 to: %2")
                           .arg(sMessage)
                           .arg(pUpdateSocket->peerAddress().toString()));
            }
        }
        else {
            bytesReceived = 0;
            file.close();
            QDir renamed;
            renamed.rename(destinationDir + sFileName + QString(".temp"), destinationDir + sFileName);
            fileList.removeLast();
            if(!fileList.isEmpty()) {
                QString sMessage = QString("<get>%1,%2,%3</get>")
                                   .arg(fileList.last().spotFilename)
                                   .arg(bytesReceived)
                                   .arg(CHUNK_SIZE);
                written = pUpdateSocket->sendTextMessage(sMessage);
                if(written != sMessage.length()) {
                    logMessage(sFunctionName, QString("Error writing %1").arg(sMessage));
                    pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString("Error writing %1").arg(sMessage));
                }
                else {
                    logMessage(sFunctionName,
                               QString("Sent %1 to: %2")
                               .arg(sMessage)
                               .arg(pUpdateSocket->peerAddress().toString()));
                }
            }
            else {
                logMessage(sFunctionName, QString("No more file to transfer"));
                pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString("File transfer completed"));
            }
        }
    }
}


void
FileUpdater::onWriteFileError() {
    file.close();
    bTrasferError = true;
    pUpdateSocket->close(QWebSocketProtocol::CloseCodeAbnormalDisconnection, QString("Error writing File"));
}


void
FileUpdater::onOpenFileError() {
    QString sFunctionName = " FileUpdater::openFileError ";
    fileList.removeLast();
    if(!fileList.isEmpty()) {
        QString sMessage = QString("<get>%1,%2,%3</get>")
                           .arg(fileList.last().spotFilename)
                           .arg(bytesReceived)
                           .arg(CHUNK_SIZE);
        int written = pUpdateSocket->sendTextMessage(sMessage);
        if(written != sMessage.length()) {
            logMessage(sFunctionName, QString("Error writing %1").arg(sMessage));
            bTrasferError = true;
            pUpdateSocket->close(QWebSocketProtocol::CloseCodeAbnormalDisconnection, QString("Error writing %1").arg(sMessage));
        }
        else {
            logMessage(sFunctionName,
                       QString("Sent %1 to: %2")
                       .arg(sMessage)
                       .arg(pUpdateSocket->peerAddress().toString()));
        }
    }
    else {
        logMessage(sFunctionName, QString("No more file to transfer"));
        bTrasferError = false;
        pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString("No more files to transfer"));
    }
}


void
FileUpdater::onProcessTextMessage(QString sMessage) {
    QString sFunctionName = " FileUpdater::onTextMessageReceived ";
    Q_UNUSED(sFunctionName)
    QString sToken;
    sToken = XML_Parse(sMessage, "spot_list");
    if(sToken != sNoData) {
        QStringList spotFileList = QStringList(sToken.split(tr(","), QString::SkipEmptyParts));
        availabeSpotList.clear();
        QStringList tmpList;
        for(int i=0; i< spotFileList.count(); i++) {
            tmpList =   QStringList(spotFileList.at(i).split(tr(";"), QString::SkipEmptyParts));
            if(tmpList.count() > 1) {
                spot* newSpot = new spot();
                newSpot->spotFilename = tmpList.at(0);
                newSpot->spotFileSize = tmpList.at(1).toLong();
                availabeSpotList.append(*newSpot);
            }
        }
        updateSpots();
    }// spot_list
}


QString
FileUpdater::XML_Parse(QString input_string, QString token) {
    QString sFunctionName = " FileUpdater::XML_Parse ";
    Q_UNUSED(sFunctionName)
    // simple XML parser
    //   XML_Parse("<score1>10</score1>","beam")   will return "10"
    // returns "" on error
    QString start_token, end_token, result = sNoData;
    start_token = "<" + token + ">";
    end_token = "</" + token + ">";

    int start_pos=-1, end_pos=-1, len=0;

    start_pos = input_string.indexOf(start_token);
    end_pos   = input_string.indexOf(end_token);

    if(start_pos < 0 || end_pos < 0) {
        result = sNoData;
    } else {
        start_pos += start_token.length();
        len = end_pos - start_pos;
        if(len > 0)
            result = input_string.mid(start_pos, len);
        else
            result = "";
    }

    return result;
}


void
FileUpdater::updateSpots() {
    QString sFunctionName = " FileUpdater::updateSpots ";
    QDir spotDir(destinationDir);
    spotList = QFileInfoList();
    if(spotDir.exists()) {// Get the list of the spots already present
        QStringList nameFilter(QStringList() << "*.mp4");
        spotDir.setNameFilters(nameFilter);
        spotDir.setFilter(QDir::Files);
        spotList = spotDir.entryInfoList();
    }
    bool bFound;
    // build the list of files to copy from server
    QList<spot> queryList = QList<spot>();
    for(int i=0; i<availabeSpotList.count(); i++) {
        bFound = false;
        for(int j=0; j<spotList.count(); j++) {
            if(availabeSpotList.at(i).spotFilename == spotList.at(j).fileName() &&
               availabeSpotList.at(i).spotFileSize == spotList.at(j).size()) {
                bFound = true;
                break;
            }
        }
        if(!bFound) {
            queryList.append(availabeSpotList.at(i));
        }
    }
    // Remove the local files not anymore requested
    for(int j=0; j<spotList.count(); j++) {
        bFound = false;
        for(int i=0; i<availabeSpotList.count(); i++) {
            if(availabeSpotList.at(i).spotFilename == spotList.at(j).fileName() &&
               availabeSpotList.at(i).spotFileSize == spotList.at(j).size()) {
                bFound = true;
                break;
            }
        }
        if(!bFound) {
            QFile::remove(spotList.at(j).absoluteFilePath());
            logMessage(sFunctionName, QString("Removed %1").arg(spotList.at(j).absoluteFilePath()));
        }
    }
    if(queryList.isEmpty()) {
        logMessage(sFunctionName, QString("All files are up to date !"));
        pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString("All files are up to date !"));
        emit connectionClosed(false);
    }
    else {
        setFileList(queryList);
        askFirstFile();
    }
}


void
FileUpdater::askFirstFile() {
    QString sFunctionName = QString(" FileUpdater::askFirstFile ");
    QString sMessage = QString("<get>%1,%2,%3</get>")
                           .arg(fileList.last().spotFilename)
                           .arg(bytesReceived)
                           .arg(CHUNK_SIZE);
    qint64 written = pUpdateSocket->sendTextMessage(sMessage);
    if(written != sMessage.length()) {
        logMessage(sFunctionName, QString("Error writing %1").arg(sMessage));
        bTrasferError = true;
        disconnect(pUpdateSocket, 0, 0, 0);
        pUpdateSocket->abort();
        int retryTime = int(RETRY_TIME * (1.0 + double(qrand())/double(RAND_MAX)));
        pReconnectionTimer->start(retryTime);
    }
    else {
        logMessage(sFunctionName,
                   QString("Sent %1 to: %2")
                   .arg(sMessage)
                   .arg(pUpdateSocket->peerAddress().toString()));
    }
}
