#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QUdpSocket>
#include <QWebSocket>
#include <QHostInfo>

#include "serverdiscoverer.h"
#include "nonetwindow.h"
#include "utility.h"
#include "scorepanel.h"
#include "segnapuntivolley.h"
#include "segnapuntibasket.h"
#include "segnapuntihandball.h"

#define DISCOVERY_PORT 45453
#define SERVER_PORT    45454

#define SERVER_CONNECTION_TIMEOUT 3000

// This class manage the Discovery process.
// It send a multicast message and listen for a correct answer
// The it try to connect to the server and if it succeed create and
// show the rigth Score Panel

ServerDiscoverer::ServerDiscoverer(QFile *_logFile, QObject *parent)
    : QObject(parent)
    , logFile(_logFile)
    , discoveryPort(DISCOVERY_PORT)
    , serverPort(SERVER_PORT)
    , discoveryAddress(QHostAddress("224.0.0.1"))
    , pNoServerWindow(Q_NULLPTR)
    , pScorePanel(Q_NULLPTR)
{
    pNoServerWindow = new NoNetWindow(Q_NULLPTR);
    pNoServerWindow->setDisplayedText(tr("In Attesa della Connessione con il Server"));
    // To manage the various timeouts that can occour
    connect(&serverConnectionTimeoutTimer, SIGNAL(timeout()),
            this, SLOT(onServerConnectionTimeout()));
}


// Multicast the "sever discovery" message on all the usable
// network interfaces.
// Retrns true if at least a message has been sent.
// If a message has been sent, a "connection timeout" timer is started.
bool
ServerDiscoverer::Discover() {
    qint64 written;
    bool bStarted = false;
    QString sMessage = "<getServer>"+ QHostInfo::localHostName() + "</getServer>";
    QByteArray datagram = sMessage.toUtf8();

    // No other window should obscure this one
    pNoServerWindow->showFullScreen();
    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for(int i=0; i<ifaces.count(); i++) {
        QNetworkInterface iface = ifaces.at(i);
        if(iface.flags().testFlag(QNetworkInterface::IsUp) &&
           iface.flags().testFlag(QNetworkInterface::IsRunning) &&
           iface.flags().testFlag(QNetworkInterface::CanMulticast) &&
          !iface.flags().testFlag(QNetworkInterface::IsLoopBack))
        {
            QUdpSocket* pDiscoverySocket = new QUdpSocket(this);
            // We need to save all the created sockets...
            discoverySocketArray.append(pDiscoverySocket);
            // To manage socket errors
            connect(pDiscoverySocket, SIGNAL(error(QAbstractSocket::SocketError)),
                    this, SLOT(onDiscoverySocketError(QAbstractSocket::SocketError)));
            // To manage the messages from the socket
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
            else {
                bStarted = true;
            }
        }
    }
    if(bStarted) {
        serverConnectionTimeoutTimer.start(SERVER_CONNECTION_TIMEOUT);
    }
    return bStarted;
}


void
ServerDiscoverer::onDiscoverySocketError(QAbstractSocket::SocketError socketError) {
    Q_UNUSED(socketError)
    QUdpSocket *pClient = qobject_cast<QUdpSocket *>(sender());
    logMessage(logFile,
               Q_FUNC_INFO,
               pClient->errorString());
    return;
}


// A Panel Server sent back an answer...
void
ServerDiscoverer::onProcessDiscoveryPendingDatagrams() {
    QUdpSocket* pSocket = qobject_cast<QUdpSocket*>(sender());
    QByteArray datagram = QByteArray();
    QByteArray answer = QByteArray();
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
        // A well formed answer has been received.
        serverConnectionTimeoutTimer.stop();
        // Remove all the "discovery sockets" to avoid overlapping
        cleanDiscoverySockets();
        checkServerAddresses();
    }
}


// Try to connect to all the Panel Server addresses
void
ServerDiscoverer::checkServerAddresses() {
    panelType = FIRST_PANEL;
    serverConnectionTimeoutTimer.start(SERVER_CONNECTION_TIMEOUT);
    for(int i=0; i<serverList.count(); i++) {
        QStringList arguments = QStringList(serverList.at(i).split(",",QString::SkipEmptyParts));
        if(arguments.count() > 1) {
            serverUrl= QString("ws://%1:%2").arg(arguments.at(0)).arg(serverPort);
            // Last Panel Type will win (is this right ?)
            panelType = arguments.at(1).toInt();
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Trying Server URL: %1")
                       .arg(serverUrl));
            pPanelServerSocket = new QWebSocket();
            pPanelServerSocket->ignoreSslErrors();
            serverSocketArray.append(pPanelServerSocket);
            connect(pPanelServerSocket, SIGNAL(connected()),
                    this, SLOT(onPanelServerConnected()));
            connect(pPanelServerSocket, SIGNAL(error(QAbstractSocket::SocketError)),
                    this, SLOT(onPanelServerSocketError(QAbstractSocket::SocketError)));
            pPanelServerSocket->open(QUrl(serverUrl));
        }
    }
}


// First come ... first served !
void
ServerDiscoverer::onPanelServerConnected() {
    serverConnectionTimeoutTimer.stop();
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Connected to Server URL: %1")
               .arg(serverUrl));
    QWebSocket* pSocket = qobject_cast<QWebSocket*>(sender());
    serverUrl = pSocket->requestUrl().toString();
    cleanServerSockets();

    // Delete old Panel instance to prevent memory leaks
    if(pScorePanel) {
        pScorePanel->disconnect();
        delete pScorePanel;
        pScorePanel = Q_NULLPTR;
    }

    if(panelType == VOLLEY_PANEL) {
        pScorePanel = new SegnapuntiVolley(serverUrl, logFile);
    }
    else if(panelType == BASKET_PANEL) {
        pScorePanel = new SegnapuntiBasket(serverUrl, logFile);
    }
    else if(panelType == HANDBALL_PANEL) {
        pScorePanel = new SegnapuntiHandball(serverUrl, logFile);
    }
    connect(pScorePanel, SIGNAL(panelClosed()),
            this, SLOT(onPanelClosed()));
    connect(pScorePanel, SIGNAL(exitRequest()),
            this, SLOT(onExitProgram()));

    pScorePanel->showFullScreen();
    pNoServerWindow->hide();
}


void
ServerDiscoverer::onPanelServerSocketError(QAbstractSocket::SocketError error) {
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("%1 %2 Error %3")
               .arg(pPanelServerSocket->requestUrl().toString())
               .arg(pPanelServerSocket->errorString())
               .arg(error));
}


// Called when no messages or connections are received in the
// SERVER_CONNECTION_TIMEOUT time
void
ServerDiscoverer::onServerConnectionTimeout() {
    serverConnectionTimeoutTimer.stop();
    // No other window should obscure this one
    pNoServerWindow->showFullScreen();
    cleanDiscoverySockets();
    cleanServerSockets();
    // Restart the discovery process
    if(!Discover()) {
        pNoServerWindow->hide();
        emit checkNetwork();
    }
}


// Emitted from the Score Panel when it closes
void
ServerDiscoverer::onPanelClosed() {
    pNoServerWindow->showFullScreen();
    cleanDiscoverySockets();
    cleanServerSockets();
    if(!Discover()) {
        pNoServerWindow->hide();
        emit checkNetwork();
    }
}


void
ServerDiscoverer::cleanDiscoverySockets() {
    for(int i=0; i<discoverySocketArray.count(); i++) {
        QUdpSocket *pDiscovery = qobject_cast<QUdpSocket *>(discoverySocketArray.at(i));
        pDiscovery->disconnect();
        pDiscovery->disconnectFromHost();
        pDiscovery->abort();
        pDiscovery->deleteLater();
    }
    discoverySocketArray.clear();
}


void
ServerDiscoverer::cleanServerSockets() {
    for(int i=0; i<serverSocketArray.count(); i++) {
        QWebSocket *pDiscovery = qobject_cast<QWebSocket *>(serverSocketArray.at(i));
        pDiscovery->disconnect();
        pDiscovery->abort();
        pDiscovery->deleteLater();
    }
    serverSocketArray.clear();
}


void
ServerDiscoverer::onExitProgram() {

}
