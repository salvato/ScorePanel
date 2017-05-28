#include "fileupdater.h"
#include <QtNetwork>
#include <QTime>
#include <QTimer>

#include "utility.h"



#define RETRY_TIME 15000
#define CHUNK_SIZE 256*1024


FileUpdater::FileUpdater(QString sName, QUrl _serverUrl, QFile *_logFile, QObject *parent)
    : QObject(parent)
    , logFile(_logFile)
    , serverUrl(_serverUrl)
{
    sMyName = sName;
    pUpdateSocket = Q_NULLPTR;
    pReconnectionTimer = Q_NULLPTR;
    bTrasferError = false;
    destinationDir = QString(".");

    bytesReceived = 0;

    connect(this, SIGNAL(openFileError()),
            this, SLOT(onOpenFileError()));
    connect(this, SIGNAL(writeFileError()),
            this, SLOT(onWriteFileError()));
}


FileUpdater::~FileUpdater() {
    if(pReconnectionTimer != Q_NULLPTR) {
        pReconnectionTimer->stop();
        disconnect(pReconnectionTimer, 0, 0, 0);
        delete pReconnectionTimer;
        pReconnectionTimer = Q_NULLPTR;
    }
    if(pUpdateSocket != Q_NULLPTR) {
        disconnect(pUpdateSocket, 0, 0, 0);
        delete pUpdateSocket;
        pUpdateSocket = Q_NULLPTR;
    }
}


bool
FileUpdater::setDestination(QString _destinationDir, QString sExtensions) {
    QString sFunctionName = " FileUpdater::setDestinationDir ";
    destinationDir = _destinationDir;
    sFileExtensions = sExtensions;
    QDir outDir(destinationDir);
    if(!outDir.exists()) {
        logMessage(logFile,
                   sFunctionName,
                   QString("Creating new directory: %1")
                   .arg(destinationDir));
        if(!outDir.mkdir(destinationDir)) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Unable to create directory: %1")
                       .arg(destinationDir));
            return false;
        }
    }
    return true;
}


void
FileUpdater::startUpdate() {
    QString sFunctionName = " FileUpdater::startUpdate ";
    Q_UNUSED(sFunctionName)
    if(pReconnectionTimer == Q_NULLPTR) {
        pReconnectionTimer = new QTimer(this);
        connect(pReconnectionTimer, SIGNAL(timeout()),
                this, SLOT(retryReconnection()));
    }
    connectToServer();
}


void
FileUpdater::retryReconnection() {
    QString sFunctionName = " FileUpdater::retryReconnection ";
    logMessage(logFile,
               sFunctionName,
               QString("Trying to connect..."));
    if(pUpdateSocket != Q_NULLPTR) {
        QAbstractSocket::SocketState socketState = pUpdateSocket->state();
        if((socketState == QAbstractSocket::UnconnectedState) ||
           (socketState == QAbstractSocket::ClosingState)) {
            pUpdateSocket->deleteLater();
            pUpdateSocket = Q_NULLPTR;
            connectToServer();
        }
        else {
            logMessage(logFile,
                       sFunctionName,
                       sMyName +
                       QString(" Error: Update Socket == NULL "));
            pUpdateSocket->deleteLater();
            pUpdateSocket = Q_NULLPTR;
            thread()->exit(0);
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
    pReconnectionTimer->stop();
    logMessage(logFile,
               sFunctionName,
               sMyName +
               QString(" Connecting to file server: %1")
               .arg(serverUrl.toString()));

    pUpdateSocket = new QWebSocket();

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

    pUpdateSocket->open(QUrl(serverUrl));

    int retryTime = int(0.2*RETRY_TIME * (1.0 + double(qrand())/double(RAND_MAX)));
   pReconnectionTimer->start(retryTime);
}


void
FileUpdater::onUpdateSocketConnected() {
    QString sFunctionName = " FileUpdater::onUpdateSocketConnected ";
    pReconnectionTimer->stop();
    logMessage(logFile,
               sFunctionName,
               sMyName +
               QString(" Connected to: %1")
               .arg(pUpdateSocket->peerAddress().toString()));
    // Query the file's list
    askFileList();
}


void
FileUpdater::askFileList() {
    QString sFunctionName = " FileUpdater::askFileList ";
    Q_UNUSED(sFunctionName)
    if(pUpdateSocket->isValid()) {
        QString sMessage;
        sMessage = QString("<send_file_list>1</send_file_list>");
        qint64 bytesSent = pUpdateSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       sFunctionName,
                       sMyName +
                       QString(" Unable to ask for file list"));
            pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString("Unable to ask for file list"));
            thread()->exit(0);
        }
        else {
            logMessage(logFile,
                       sFunctionName,
                       sMyName +
                       QString(" Sent %1 to %2")
                       .arg(sMessage)
                       .arg(pUpdateSocket->peerAddress().toString()));
        }
    }
}


void
FileUpdater::onServerDisconnected() {
    QString sFunctionName = " FileUpdater::onServerDisconnected ";
    logMessage(logFile,
               sFunctionName,
               sMyName +
               QString(" WebSocket disconnected from: %1")
               .arg(pUpdateSocket->peerAddress().toString()));
    pReconnectionTimer->stop();
    if(pUpdateSocket) {
        disconnect(pUpdateSocket, 0,0,0);
        delete pUpdateSocket;
        pUpdateSocket = Q_NULLPTR;
    }
    thread()->exit(0);
}


void
FileUpdater::terminate() {
    QString sFunctionName = " FileUpdater::terminate ";
    logMessage(logFile,
               sFunctionName,
               sMyName +
               QString(" terminating"));
    pReconnectionTimer->stop();
    thread()->exit(0);
}


void
FileUpdater::onUpdateSocketError(QAbstractSocket::SocketError error) {
    QString sFunctionName = " FileUpdater::onUpdateSocketError ";
    Q_UNUSED(error)
    Q_UNUSED(sFunctionName)
    logMessage(logFile,
               sFunctionName,
               sMyName +
               QString(" %1 %2 Error %3")
               .arg(pUpdateSocket->localAddress().toString())
               .arg(pUpdateSocket->errorString())
               .arg(error));
    if(!disconnect(pUpdateSocket, 0, 0, 0)) {
        logMessage(logFile,
                   sFunctionName,
                   sMyName +
                   QString(" Unable to disconnect signals from WebSocket"));
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
        logMessage(logFile,
                   sMyName +
                   sFunctionName,
                   QString("pWebSocket unmanaged error: Exiting"));
        thread()->exit(0);
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
        file.setFileName(destinationDir + sFileName);
        file.remove();
        file.setFileName(destinationDir + sFileName + QString(".temp"));
        file.remove();// Just in case of a previous aborted transfer
        if(file.open(QIODevice::Append)) {
            len = baMessage.size()-1024;
            written = file.write(baMessage.mid(1024));
            bytesReceived += written;
            if(len != written) {
                logMessage(logFile,
                           sFunctionName,
                           sMyName +
                           QString(" File lenght mismatch in: %1 written(%2/%3)")
                           .arg(sFileName)
                           .arg(written)
                           .arg(len));
                emit writeFileError();
            }
        } else {
            logMessage(logFile,
                       sFunctionName,
                       sMyName +
                       QString(" Unable to write file: %1")
                       .arg(sFileName));
            emit openFileError();
            return;
        }
    }
    else {// It's a new frame for an already opened file...
        len = baMessage.size();
        written = file.write(baMessage);
        bytesReceived += written;
        if(len != written) {
            logMessage(logFile,
                       sFunctionName,
                       sMyName +
                       QString(" File lenght mismatch in: %1 written(%2/%3)")
                       .arg(sFileName)
                       .arg(written)
                       .arg(len));
            emit writeFileError();
            return;
        }
    }
#ifdef LOG_VERBOSE
    logMessage(logFile,
               sFunctionName,
               QString("Received %1 bytes").arg(bytesReceived));
#endif
    // Check if the file transfer must be stopped
    if(thread()->isInterruptionRequested()) {
        logMessage(logFile,
                   sFunctionName,
                   sMyName +
                   QString(" Received an exit request"));
        pUpdateSocket->close();
        thread()->exit(0);
        return;
    }
    if(isLastFrame) {
        if(bytesReceived < queryList.last().fileSize) {
            QString sMessage = QString("<get>%1,%2,%3</get>")
                               .arg(queryList.last().fileName)
                               .arg(bytesReceived)
                               .arg(CHUNK_SIZE);
            written = pUpdateSocket->sendTextMessage(sMessage);
            if(written != sMessage.length()) {
                logMessage(logFile,
                           sFunctionName,
                           sMyName +
                           QString(" Error writing %1").arg(sMessage));
                pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString("Error writing %1").arg(sMessage));
            }
#ifdef LOG_VERBOSE
            else {
                logMessage(logFile,
                           sFunctionName,
                           sMyName +
                           QString(" Sent %1 to: %2")
                           .arg(sMessage)
                           .arg(pUpdateSocket->peerAddress().toString()));
            }
#endif
        }
        else {
            bytesReceived = 0;
            file.close();
            QDir renamed;
            renamed.rename(destinationDir + sFileName + QString(".temp"), destinationDir + sFileName);
            queryList.removeLast();
            if(!queryList.isEmpty()) {
                QString sMessage = QString("<get>%1,%2,%3</get>")
                                   .arg(queryList.last().fileName)
                                   .arg(bytesReceived)
                                   .arg(CHUNK_SIZE);
                written = pUpdateSocket->sendTextMessage(sMessage);
                if(written != sMessage.length()) {
                    logMessage(logFile,
                               sFunctionName,
                               sMyName +
                               QString(" Error writing %1").arg(sMessage));
                    pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString("Error writing %1").arg(sMessage));
                }
                else {
                    logMessage(logFile,
                               sFunctionName,
                               sMyName +
                               QString(" Sent %1 to: %2")
                               .arg(sMessage)
                               .arg(pUpdateSocket->peerAddress().toString()));
                }
            }
            else {
                logMessage(logFile,
                           sFunctionName,
                           sMyName +
                           QString(" No more file to transfer"));
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
    thread()->exit(0);
}


void
FileUpdater::onOpenFileError() {
    QString sFunctionName = " FileUpdater::openFileError ";
    queryList.removeLast();
    if(!queryList.isEmpty()) {
        QString sMessage = QString("<get>%1,%2,%3</get>")
                           .arg(queryList.last().fileName)
                           .arg(bytesReceived)
                           .arg(CHUNK_SIZE);
        int written = pUpdateSocket->sendTextMessage(sMessage);
        if(written != sMessage.length()) {
            logMessage(logFile,
                       sFunctionName,
                       sMyName +
                       QString(" Error writing %1").arg(sMessage));
            bTrasferError = true;
            pUpdateSocket->close(QWebSocketProtocol::CloseCodeAbnormalDisconnection, QString("Error writing %1").arg(sMessage));
        }
        else {
            logMessage(logFile,
                       sFunctionName,
                       sMyName +
                       QString(" Sent %1 to: %2")
                       .arg(sMessage)
                       .arg(pUpdateSocket->peerAddress().toString()));
        }
    }
    else {
        logMessage(logFile,
                   sFunctionName,
                   sMyName +
                   QString(" No more file to transfer"));
        bTrasferError = false;
        pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString("No more files to transfer"));
    }
}


void
FileUpdater::onProcessTextMessage(QString sMessage) {
    QString sFunctionName = " FileUpdater::onTextMessageReceived ";
    Q_UNUSED(sFunctionName)
    QString sToken;
    QString sNoData = QString("NoData");
    sToken = XML_Parse(sMessage, "file_list");
    if(sToken != sNoData) {
        QStringList tmpFileList = QStringList(sToken.split(tr(","), QString::SkipEmptyParts));
        remoteFileList.clear();
        QStringList tmpList;
        for(int i=0; i< tmpFileList.count(); i++) {
            tmpList =   QStringList(tmpFileList.at(i).split(tr(";"), QString::SkipEmptyParts));
            if(tmpList.count() > 1) {// Both name and size are presents
                files newFile;
                newFile.fileName = tmpList.at(0);
                newFile.fileSize = tmpList.at(1).toLong();
                remoteFileList.append(newFile);
            }
        }
        updateFiles();
    }// file_list
}


void
FileUpdater::updateFiles() {
    QString sFunctionName = " FileUpdater::updateFiles ";
    bool bFound;
    QDir fileDir(destinationDir);
    QFileInfoList localFileInfoList = QFileInfoList();
    if(fileDir.exists()) {// Get the list of the spots already present
        QStringList nameFilter(sFileExtensions.split(" "));
        fileDir.setNameFilters(nameFilter);
        fileDir.setFilter(QDir::Files);
        localFileInfoList = fileDir.entryInfoList();
    }
    // build the list of files to copy from server
    queryList = QList<files>();
    for(int i=0; i<remoteFileList.count(); i++) {
        bFound = false;
        for(int j=0; j<localFileInfoList.count(); j++) {
            if(remoteFileList.at(i).fileName == localFileInfoList.at(j).fileName() &&
               remoteFileList.at(i).fileSize == localFileInfoList.at(j).size()) {
                bFound = true;
                break;
            }
        }
        if(!bFound) {
            queryList.append(remoteFileList.at(i));
        }
    }
    // Remove the local files not anymore requested
    for(int j=0; j<localFileInfoList.count(); j++) {
        bFound = false;
        for(int i=0; i<remoteFileList.count(); i++) {
            if(remoteFileList.at(i).fileName == localFileInfoList.at(j).fileName() &&
               remoteFileList.at(i).fileSize == localFileInfoList.at(j).size()) {
                bFound = true;
                break;
            }
        }
        if(!bFound) {
            QFile::remove(localFileInfoList.at(j).absoluteFilePath());
            logMessage(logFile,
                       sFunctionName,
                       QString("Removed %1").arg(localFileInfoList.at(j).absoluteFilePath()));
        }
    }
    if(queryList.isEmpty()) {
        logMessage(logFile,
                   sFunctionName,
                   sMyName +
                   QString(" All files are up to date !"));
        pReconnectionTimer->stop();

        disconnect(pUpdateSocket, 0,0,0);
        pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString("All files are up to date !"));
        thread()->exit(0);// We have done !
    }
    else {
        askFirstFile();
    }
}


void
FileUpdater::askFirstFile() {
    QString sFunctionName = QString(" FileUpdater::askFirstFile ");
    QString sMessage = QString("<get>%1,%2,%3</get>")
                           .arg(queryList.last().fileName)
                           .arg(bytesReceived)
                           .arg(CHUNK_SIZE);
    qint64 written = pUpdateSocket->sendTextMessage(sMessage);
    if(written != sMessage.length()) {
        logMessage(logFile,
                   sFunctionName,
                   sMyName +
                   QString(" Error writing %1").arg(sMessage));
        bTrasferError = true;
        disconnect(pUpdateSocket, 0, 0, 0);
        pUpdateSocket->abort();
        int retryTime = int(RETRY_TIME * (1.0 + double(qrand())/double(RAND_MAX)));
        pReconnectionTimer->start(retryTime);
    }
    else {
        logMessage(logFile,
                   sFunctionName,
                   sMyName +
                   QString(" Sent %1 to: %2")
                   .arg(sMessage)
                   .arg(pUpdateSocket->peerAddress().toString()));
    }
}
