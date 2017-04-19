#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QUdpSocket>
#include <QWebSocket>
#include <QHostInfo>

#include "serverdiscoverer.h"


#define DISCOVERY_PORT 45453
#define SERVER_PORT    45454


ServerDiscoverer::ServerDiscoverer(QFile *_logFile, QObject *parent)
    : QObject(parent)
    , logFile(_logFile)
    , sNoData(tr("NoData"))
    , discoveryPort(DISCOVERY_PORT)
    , serverPort(SERVER_PORT)
    , discoveryAddress(QHostAddress("224.0.0.1"))
{
}


void
ServerDiscoverer::logMessage(QString sFunctionName, QString sMessage) {
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
ServerDiscoverer::Discover() {
    QString sFunctionName = " ServerDiscoverer::Discover ";
    qint64 written;
    Q_UNUSED(sFunctionName)
    Q_UNUSED(written)

    QString sMessage = "<getServer>"+ QHostInfo::localHostName() + "</getServer>";
    QByteArray datagram = sMessage.toUtf8();


    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for(int i=0; i<ifaces.count(); i++) {
        QNetworkInterface iface = ifaces.at(i);
        if(iface.flags().testFlag(QNetworkInterface::IsUp) &&
           iface.flags().testFlag(QNetworkInterface::IsRunning) &&
           iface.flags().testFlag(QNetworkInterface::CanMulticast) &&
          !iface.flags().testFlag(QNetworkInterface::IsLoopBack))
        {
            QUdpSocket* pDiscoverySocket = new QUdpSocket(this);
            discoverySocketArray.append(pDiscoverySocket);
            connect(pDiscoverySocket, SIGNAL(error(QAbstractSocket::SocketError)),
                    this, SLOT(onDiscoverySocketError(QAbstractSocket::SocketError)));
            connect(pDiscoverySocket, SIGNAL(readyRead()),
                    this, SLOT(onProcessPendingDatagrams()));
            pDiscoverySocket->bind();
            pDiscoverySocket->setMulticastInterface(iface);
            pDiscoverySocket->setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
            written = pDiscoverySocket->writeDatagram(datagram.data(), datagram.size(),
                                                      discoveryAddress, discoveryPort);
            logMessage(sFunctionName,
                       QString("Writing %1 to %2")
                       .arg(sMessage)
                       .arg(discoveryAddress.toString()));
            if(written != datagram.size()) {
                logMessage(sFunctionName,
                           QString("Unable to write to Discovery Socket"));
            }
        }
    }
}


void
ServerDiscoverer::onDiscoverySocketError(QAbstractSocket::SocketError socketError) {
    Q_UNUSED(socketError)
    QString sFunctionName = " ServerDiscoverer::onDiscoverySocketError ";
    QUdpSocket *pClient = qobject_cast<QUdpSocket *>(sender());
    logMessage(sFunctionName, pClient->errorString());
    return;
}


void
ServerDiscoverer::onProcessPendingDatagrams() {
    QString sFunctionName = " ServerDiscoverer::onProcessPendingDatagrams ";
    Q_UNUSED(sFunctionName)
    QUdpSocket* pSocket = qobject_cast<QUdpSocket*>(sender());
    QByteArray datagram = QByteArray();
    QString sToken;
    while(pSocket->hasPendingDatagrams()) {
        datagram.resize(pSocket->pendingDatagramSize());
        if(pSocket->readDatagram(datagram.data(), datagram.size()) == -1) {
            logMessage(sFunctionName,
                       QString("Error reading from udp socket: %1")
                       .arg(serverUrl));
        }
        logMessage(sFunctionName,
                   QString("pDiscoverySocket Received: %1")
                   .arg(datagram.data()));
        sToken = XML_Parse(datagram.data(), "serverIP");
        if(sToken != sNoData) {
            ipList = QStringList(sToken.split(tr(","),QString::SkipEmptyParts));
            if(ipList.isEmpty())
                return;
            logMessage(sFunctionName,
                       QString("Found %1 addresses")
                       .arg(ipList.count()));
            for(int i=0; i<ipList.count(); i++) {
                serverUrl= QString("ws://%1:%2").arg(ipList.at(i)).arg(serverPort);
                logMessage(sFunctionName,
                           QString("Trying sServer URL: %1")
                           .arg(serverUrl));
                QWebSocket* pWebSocket = new QWebSocket();
                webSocketList.append(pWebSocket);
                connect(pWebSocket, SIGNAL(connected()),
                        this, SLOT(onWebSocketConnected()));
                connect(pWebSocket, SIGNAL(error(QAbstractSocket::SocketError)),
                        this, SLOT(onWebSocketError(QAbstractSocket::SocketError)));
#ifndef Q_OS_WIN // No support for secure websocket exists
                connect(pWebSocket, SIGNAL(sslErrors(QList<QSslError>)),
                        this, SLOT(onSslErrors(QList<QSslError>)));
#endif
                pWebSocket->open(QUrl(serverUrl));
            }
        }
    }
}


void
ServerDiscoverer::onWebSocketConnected() {
    QString sFunctionName = " ServerDiscoverer::onWebSocketConnected ";
    Q_UNUSED(sFunctionName)
    QWebSocket *pWebSocket = qobject_cast<QWebSocket *>(sender());
    logMessage(sFunctionName,
               QString("WebSocket connected to: %1")
               .arg(pWebSocket->peerAddress().toString()));
    disconnect(pWebSocket, 0, 0, 0);
    emit serverConnected(pWebSocket);
}


void
ServerDiscoverer::onWebSocketError(QAbstractSocket::SocketError error) {
    QString sFunctionName = " ServerDiscoverer::onWebSocketError ";
    Q_UNUSED(error)
    Q_UNUSED(sFunctionName)
    QWebSocket *pWebSocket = qobject_cast<QWebSocket *>(sender());
    logMessage(sFunctionName,
               QString("%1 %2")
               .arg(pWebSocket->localAddress().toString())
               .arg(pWebSocket->errorString()));
    if(!disconnect(pWebSocket, 0, 0, 0)) {
        logMessage(sFunctionName, QString("Unable to disconnect signals from WebSocket"));
    }
    pWebSocket->close(QWebSocketProtocol::CloseCodeAbnormalDisconnection, pWebSocket->errorString());
    pWebSocket->deleteLater();
}


void
ServerDiscoverer::onSslErrors(const QList<QSslError> &errors) {
    QString sFunctionName = " ServerDiscoverer::onSslErrors ";
    Q_UNUSED(errors)
    Q_UNUSED(sFunctionName)
    QWebSocket *pWebSocket = qobject_cast<QWebSocket *>(sender());
//    foreach(QSslError sslError, errors) {
//        switch(sslError.error()) {

//        case QSslError::NoError :
//            qDebug() << "NoError";

//        case QSslError::UnableToGetIssuerCertificate :
//            qDebug() << "UnableToGetIssuerCertificate";
//            break;

//        case QSslError::UnableToDecryptCertificateSignature :
//            qDebug() << "UnableToDecryptCertificateSignature";
//            break;

//        case QSslError::UnableToDecodeIssuerPublicKey :
//            qDebug() << "UnableToDecodeIssuerPublicKey";
//            break;

//        case QSslError::CertificateSignatureFailed  :
//            qDebug() << "CertificateSignatureFailed";
//            break;

//        case QSslError::CertificateNotYetValid :
//            qDebug() << "CertificateNotYetValid";
//            break;

//        case QSslError::CertificateExpired :
//            qDebug() << "CertificateExpired";
//            break;

//        case QSslError::InvalidNotBeforeField :
//            qDebug() << "InvalidNotBeforeField";
//            break;

//        case QSslError::InvalidNotAfterField :
//            qDebug() << "InvalidNotAfterField";
//            break;

//        case QSslError::SelfSignedCertificate :
//            qDebug() << "SelfSignedCertificate";
//            break;

//        case QSslError::SelfSignedCertificateInChain :
//            qDebug() << "SelfSignedCertificateInChain";
//            break;

//        case QSslError::UnableToGetLocalIssuerCertificate :
//            qDebug() << "UnableToGetLocalIssuerCertificate";
//            break;

//        case QSslError::UnableToVerifyFirstCertificate :
//            qDebug() << "UnableToVerifyFirstCertificate";
//            break;

//        case QSslError::CertificateRevoked :
//            qDebug() << "CertificateRevoked";
//            break;

//        case QSslError::InvalidCaCertificate :
//            qDebug() << "InvalidCaCertificate";
//            break;

//        case QSslError::PathLengthExceeded :
//            qDebug() << "PathLengthExceeded";
//            break;

//        case QSslError::InvalidPurpose :
//            qDebug() << "InvalidPurpose";
//            break;

//        case QSslError::CertificateUntrusted :
//            qDebug() << "CertificateUntrusted";
//            break;

//        case QSslError::CertificateRejected :
//            qDebug() << "CertificateRejected";
//            break;

//        case QSslError::SubjectIssuerMismatch :
//            qDebug() << "SubjectIssuerMismatch";
//            break;

//        case QSslError::AuthorityIssuerSerialNumberMismatch :
//            qDebug() << "AuthorityIssuerSerialNumberMismatch";
//            break;

//        case QSslError::NoPeerCertificate :
//            qDebug() << "NoPeerCertificate";
//            break;

//        case QSslError::HostNameMismatch :
//            qDebug() << "HostNameMismatch";
//            break;

//        case QSslError::UnspecifiedError :
//            qDebug() << "UnspecifiedError";
//            break;

//        case QSslError::NoSslSupport :
//            qDebug() << "NoSslSupport";
//            break;

//        case QSslError::CertificateBlacklisted :
//            qDebug() << "CertificateBlacklisted";
//            break;

//        default:
//            qDebug() << "???";
//        }

//        qDebug() << "QSslError= " << sslError.error();
//    }
//    if(logFile) {
//      logFile->write(sInformation.toUtf8().data());
//      logFile->flush();
//    }

#pragma message "WARNING: Never ignore SSL errors in production code."
#pragma message "The proper way to handle self-signed certificates is"
#pragma message "to add a custom root to the CA store."

    pWebSocket->ignoreSslErrors();
}



QString
ServerDiscoverer::XML_Parse(QString input_string, QString token) {
    QString sFunctionName = " ServerDiscoverer::XML_Parse ";
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
        if(len>0)
            result = input_string.mid(start_pos, len);
        else
            result = "";
    }

    return result;
}

