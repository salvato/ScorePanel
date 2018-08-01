#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QUdpSocket>
#include <QWebSocket>
#include <QHostInfo>

#include "serverdiscoverer.h"
#include "utility.h"

#define DISCOVERY_PORT 45453
#define SERVER_PORT    45454



ServerDiscoverer::ServerDiscoverer(QFile *_logFile, QObject *parent)
    : QObject(parent)
    , logFile(_logFile)
    , discoveryPort(DISCOVERY_PORT)
    , serverPort(SERVER_PORT)
    , discoveryAddress(QHostAddress("224.0.0.1"))
{
    connect(this, SIGNAL(checkServerAddress()),
            this, SLOT(onCheckServerAddress()));
}


bool
ServerDiscoverer::Discover() {
    qint64 written;
    
    Q_UNUSED(written)
    bool bStarted = false;

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
                    this, SLOT(onProcessDiscoveryPendingDatagrams()));
            if(!pDiscoverySocket->bind()) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Unable to bind the Discovery Socket"));
                continue;
            }
            pDiscoverySocket->setMulticastInterface(iface);
            pDiscoverySocket->setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
            written = pDiscoverySocket->writeDatagram(datagram.data(), datagram.size(),
                                                      discoveryAddress, discoveryPort);
#ifdef LOG_VERBOSE
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Writing %1 to %2 - interface# %3/%4 : %5")
                       .arg(sMessage)
                       .arg(discoveryAddress.toString())
                       .arg(i)
                       .arg(ifaces.count())
                       .arg(iface.humanReadableName()));
#endif
            if(written != datagram.size()) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Unable to write to Discovery Socket"));
            }
            else
                bStarted = true;
        }
    }
    return bStarted;
}


void
ServerDiscoverer::onDiscoverySocketError(QAbstractSocket::SocketError socketError) {
    Q_UNUSED(socketError)
    QUdpSocket *pClient = qobject_cast<QUdpSocket *>(sender());
    logMessage(logFile, Q_FUNC_INFO, pClient->errorString());
    return;
}


void
ServerDiscoverer::onProcessDiscoveryPendingDatagrams() {
    QUdpSocket* pSocket = qobject_cast<QUdpSocket*>(sender());
    QByteArray datagram, answer;
    QString sToken;
    QString sNoData = QString("NoData");
    while(pSocket->hasPendingDatagrams()) {
        datagram.resize(int(pSocket->pendingDatagramSize()));
        if(pSocket->readDatagram(datagram.data(), datagram.size()) == -1) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Error reading from udp socket: %1")
                       .arg(serverUrl));
        }
        answer.append(datagram);
    }
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("pDiscoverySocket Received: %1")
               .arg(answer.data()));
#endif
    sToken = XML_Parse(answer.data(), "serverIP");
    if(sToken != sNoData) {
        serverList = QStringList(sToken.split(";",QString::SkipEmptyParts));
        if(serverList.isEmpty())
            return;
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Found %1 addresses")
                   .arg(serverList.count()));
#endif
        emit checkServerAddress();
    }
}


void
ServerDiscoverer::onCheckServerAddress() {
    while(!serverList.isEmpty()) {
        QStringList arguments = QStringList(serverList.at(0).split(",",QString::SkipEmptyParts));
        if(arguments.count() > 1) {
            serverUrl= QString("ws://%1:%2").arg(arguments.at(0)).arg(serverPort);
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Trying Server URL: %1")
                       .arg(serverUrl));
            pPanelServerSocket = new QWebSocket();
            connect(pPanelServerSocket, SIGNAL(connected()),
                    this, SLOT(onPanelServerConnected()));
            connect(pPanelServerSocket, SIGNAL(error(QAbstractSocket::SocketError)),
                    this, SLOT(onPanelServerSocketError(QAbstractSocket::SocketError)));
            pPanelServerSocket->open(QUrl(serverUrl));
            break;
        }
        else {
            serverList.removeFirst();
        }
    }
}


void
ServerDiscoverer::onPanelServerConnected() {
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Connected to Server URL: %1")
               .arg(serverUrl));
    if(pPanelServerSocket != Q_NULLPTR) {
        if(pPanelServerSocket->isValid())
            pPanelServerSocket->close();
        pPanelServerSocket->deleteLater();
        pPanelServerSocket = Q_NULLPTR;
    }
    QStringList arguments = QStringList(serverList.at(0).split(",",QString::SkipEmptyParts));
    emit serverFound(serverUrl, arguments.at(1).toInt());
}


void
ServerDiscoverer::onPanelServerSocketError(QAbstractSocket::SocketError error) {
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("%1 %2 Error %3")
               .arg(pPanelServerSocket->peerAddress().toString())
               .arg(pPanelServerSocket->errorString())
               .arg(error));
    serverList.removeFirst();
    if(pPanelServerSocket != Q_NULLPTR) {
        if(pPanelServerSocket->isValid())
            pPanelServerSocket->close();
        pPanelServerSocket->deleteLater();
        pPanelServerSocket = Q_NULLPTR;
    }
    onCheckServerAddress();
}
