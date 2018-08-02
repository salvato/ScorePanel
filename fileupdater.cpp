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
    bTrasferError = false;
    destinationDir = QString(".");

    bytesReceived = 0;

    connect(this, SIGNAL(openFileError()),
            this, SLOT(onOpenFileError()));
    connect(this, SIGNAL(writeFileError()),
            this, SLOT(onWriteFileError()));
}


FileUpdater::~FileUpdater() {
    if(pUpdateSocket != Q_NULLPTR) {
        pUpdateSocket->disconnect();
        delete pUpdateSocket;
        pUpdateSocket = Q_NULLPTR;
    }
}


bool
FileUpdater::setDestination(QString _destinationDir, QString sExtensions) {
    destinationDir = _destinationDir;
    sFileExtensions = sExtensions;
    QDir outDir(destinationDir);
    if(!outDir.exists()) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Creating new directory: %1")
                   .arg(destinationDir));
        if(!outDir.mkdir(destinationDir)) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to create directory: %1")
                       .arg(destinationDir));
            return false;
        }
    }
    return true;
}


void
FileUpdater::startUpdate() {
    connectToServer();
}


void
FileUpdater::connectToServer() {
    logMessage(logFile,
               Q_FUNC_INFO,
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
}


void
FileUpdater::onUpdateSocketConnected() {
    logMessage(logFile,
               Q_FUNC_INFO,
               sMyName +
               QString(" Connected to: %1")
               .arg(pUpdateSocket->peerAddress().toString()));
    // Query the file's list
    askFileList();
}


void
FileUpdater::askFileList() {
    if(pUpdateSocket->isValid()) {
        QString sMessage;
        sMessage = QString("<send_file_list>1</send_file_list>");
        qint64 bytesSent = pUpdateSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       sMyName +
                       QString(" Unable to ask for file list"));
            pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString(tr("Impossibile ottenere la lista dei files")));
            thread()->exit(0);
        }
        else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       sMyName +
                       QString(" Sent %1 to %2")
                       .arg(sMessage)
                       .arg(pUpdateSocket->peerAddress().toString()));
        }
    }
}


void
FileUpdater::onServerDisconnected() {
    logMessage(logFile,
               Q_FUNC_INFO,
               sMyName +
               QString(" WebSocket disconnected from: %1")
               .arg(pUpdateSocket->peerAddress().toString()));
    if(pUpdateSocket) {
        pUpdateSocket->disconnect();
        pUpdateSocket->close();
        pUpdateSocket->deleteLater();
        pUpdateSocket = Q_NULLPTR;
    }
    emit connectionClosed(true);
}


void
FileUpdater::onUpdateSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error)
    
    logMessage(logFile,
               Q_FUNC_INFO,
               sMyName +
               QString(" %1 %2 Error %3")
               .arg(pUpdateSocket->localAddress().toString())
               .arg(pUpdateSocket->errorString())
               .arg(error));
    if(!pUpdateSocket->disconnect()) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   sMyName +
                   QString(" Unable to disconnect signals from WebSocket"));
    }
    pUpdateSocket->abort();
    emit connectionClosed(true);
}


void
FileUpdater::onProcessBinaryFrame(QByteArray baMessage, bool isLastFrame) {
    QString sMessage;
    // Check if the file transfer must be stopped
    if(thread()->isInterruptionRequested()) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   sMyName +
                   QString(" Received an exit request"));
        pUpdateSocket->disconnect();
        pUpdateSocket->close();
        pUpdateSocket->deleteLater();
        pUpdateSocket = Q_NULLPTR;
        thread()->exit(0);
        return;
    }
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
                           Q_FUNC_INFO,
                           sMyName +
                           QString(" File lenght mismatch in: %1 written(%2/%3)")
                           .arg(sFileName)
                           .arg(written)
                           .arg(len));
                emit writeFileError();
            }
        } else {
            logMessage(logFile,
                       Q_FUNC_INFO,
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
                       Q_FUNC_INFO,
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
               Q_FUNC_INFO,
               QString("Received %1 bytes").arg(bytesReceived));
#endif
    if(isLastFrame) {
        if(bytesReceived < queryList.last().fileSize) {
            sMessage = QString("<get>%1,%2,%3</get>")
                       .arg(queryList.last().fileName)
                       .arg(bytesReceived)
                       .arg(CHUNK_SIZE);
            written = pUpdateSocket->sendTextMessage(sMessage);
            if(written != sMessage.length()) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           sMyName +
                           QString(" Error writing %1").arg(sMessage));
                pUpdateSocket->disconnect();
                pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString("Error writing %1").arg(sMessage));
                pUpdateSocket->deleteLater();
                pUpdateSocket = Q_NULLPTR;
                emit connectionClosed(true);
                return;
            }
#ifdef LOG_VERBOSE
            else {
                logMessage(logFile,
                           Q_FUNC_INFO,
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
                               Q_FUNC_INFO,
                               sMyName +
                               QString(" Error writing %1").arg(sMessage));
                    pUpdateSocket->disconnect();
                    pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString(tr("Errore scrivendo %1")).arg(sMessage));
                    pUpdateSocket->deleteLater();
                    pUpdateSocket = Q_NULLPTR;
                    emit connectionClosed(true);
                }
#ifdef LOG_VERBOSE
                else {
                    logMessage(logFile,
                               Q_FUNC_INFO,
                               sMyName +
                               QString(" Sent %1 to: %2")
                               .arg(sMessage)
                               .arg(pUpdateSocket->peerAddress().toString()));
                }
#endif
            }
            else {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           sMyName +
                           QString(" No more file to transfer"));
                pUpdateSocket->disconnect();
                pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString(tr("Trasferimento dei File Completetato")));
                pUpdateSocket->deleteLater();
                pUpdateSocket = Q_NULLPTR;
                emit connectionClosed(false);
            }
        }
    }
}


void
FileUpdater::onWriteFileError() {
    file.close();
    bTrasferError = true;
    pUpdateSocket->disconnect();
    pUpdateSocket->close(QWebSocketProtocol::CloseCodeAbnormalDisconnection, QString(tr("Errore nello scrivere sul file")));
    pUpdateSocket->deleteLater();
    pUpdateSocket = Q_NULLPTR;
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Error writing File: %1")
               .arg(queryList.last().fileName));
    emit connectionClosed(true);
}


void
FileUpdater::onOpenFileError() {
    queryList.removeLast();
    if(!queryList.isEmpty()) {
        QString sMessage = QString("<get>%1,%2,%3</get>")
                           .arg(queryList.last().fileName)
                           .arg(bytesReceived)
                           .arg(CHUNK_SIZE);
        int written = pUpdateSocket->sendTextMessage(sMessage);
        if(written != sMessage.length()) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       sMyName +
                       QString(" Error writing %1").arg(sMessage));
            bTrasferError = true;
            pUpdateSocket->close(QWebSocketProtocol::CloseCodeAbnormalDisconnection, QString(tr("Errore scrivendo %1")).arg(sMessage));
        }
        else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       sMyName +
                       QString(" Sent %1 to: %2")
                       .arg(sMessage)
                       .arg(pUpdateSocket->peerAddress().toString()));
        }
    }
    else {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   sMyName +
                   QString(" No more file to transfer"));
        bTrasferError = false;
        pUpdateSocket->disconnect();
        pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString(tr("Nessun altro file da trasferire")));
        pUpdateSocket->deleteLater();
        pUpdateSocket = Q_NULLPTR;
        emit connectionClosed(false);
    }
}


void
FileUpdater::onProcessTextMessage(QString sMessage) {
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Updating Files"));
    QString sToken;
    QString sNoData = QString("NoData");
    sToken = XML_Parse(sMessage, "file_list");
    logMessage(logFile,
               Q_FUNC_INFO,
               sToken);
    if(sToken != sNoData) {
        QStringList tmpFileList = QStringList(sToken.split(",", QString::SkipEmptyParts));
        remoteFileList.clear();
        QStringList tmpList;
        for(int i=0; i< tmpFileList.count(); i++) {
            tmpList =   QStringList(tmpFileList.at(i).split(";", QString::SkipEmptyParts));
            if(tmpList.count() > 1) {// Both name and size are presents
                files newFile;
                newFile.fileName = tmpList.at(0);
                newFile.fileSize = tmpList.at(1).toLong();
                remoteFileList.append(newFile);
            }
        }
        updateFiles();
    }// file_list
    else {
        bTrasferError = false;
        pUpdateSocket->disconnect();
        pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString(tr("Nessun file da trasferire")));
        pUpdateSocket->deleteLater();
        pUpdateSocket = Q_NULLPTR;
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Nessun file da trasferire"));
        emit connectionClosed(false);
    }
}


void
FileUpdater::updateFiles() {
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
                       Q_FUNC_INFO,
                       QString("Removed %1").arg(localFileInfoList.at(j).absoluteFilePath()));
        }
    }
    if(queryList.isEmpty()) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   sMyName +
                   QString(" All files are up to date !"));
        pUpdateSocket->disconnect();
        pUpdateSocket->close(QWebSocketProtocol::CloseCodeNormal, QString(tr("Tutti i files sono aggiornati !")));
        pUpdateSocket->deleteLater();
        pUpdateSocket = Q_NULLPTR;
        emit connectionClosed(false);
        return;
    }
    else {
        askFirstFile();
    }
}


void
FileUpdater::askFirstFile() {
    QString sMessage = QString("<get>%1,%2,%3</get>")
                           .arg(queryList.last().fileName)
                           .arg(bytesReceived)
                           .arg(CHUNK_SIZE);
    qint64 written = pUpdateSocket->sendTextMessage(sMessage);
    if(written != sMessage.length()) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   sMyName +
                   QString(" Error writing %1").arg(sMessage));
        bTrasferError = true;
        pUpdateSocket->disconnect();
        pUpdateSocket->close(QWebSocketProtocol::CloseCodeAbnormalDisconnection, QString(tr("Errore scrivendo %1")).arg(sMessage));
        pUpdateSocket->deleteLater();
        pUpdateSocket = Q_NULLPTR;
        emit connectionClosed(false);
        return;
    }
    else {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   sMyName +
                   QString(" Sent %1 to: %2")
                   .arg(sMessage)
                   .arg(pUpdateSocket->peerAddress().toString()));
    }
}
