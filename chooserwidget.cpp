#include <QNetworkInterface>
#include <QWebSocket>
#include <QFile>
#include <QMessageBox>
#include <QDir>


#include "chooserwidget.h"
#include "serverdiscoverer.h"
#include "nonetwindow.h"
#include "scorepanel.h"
#include "segnapuntivolley.h"
#include "segnapuntibasket.h"


#define VOLLEY_PANEL 0
#define FIRST_PANEL  VOLLEY_PANEL
#define BASKET_PANEL 1
#define LAST_PANEL   BASKET_PANEL

#define CONNECTION_TIME       10000  // Not to be set too low for coping with slow networks
#define PING_PERIOD           3000
#define NETWORK_CHECK_TIME    3000
#define CONNECTION_CHECK_TIME 30000
#define LOG_MESG

chooserWidget::chooserWidget(QWidget *parent)
    : QWidget(parent)
    , logFile(Q_NULLPTR)
    , pWebSocket(Q_NULLPTR)
    , pScorePanel(Q_NULLPTR)
{
    QString sFunctionName = QString(" chooserWidget::chooserWidget ");
    Q_UNUSED(sFunctionName)

    sDebugInformation.setString(&sDebugMessage);
    QString sBaseDir;
#ifdef Q_OS_ANDROID
    sBaseDir = QString("/storage/extSdCard/");
#else
    sBaseDir = QDir::homePath();
    if(!sBaseDir.endsWith(QString("/"))) sBaseDir+= QString("/");
#endif
    logFileName = QString("%1score_panel.txt").arg(sBaseDir);

#ifndef Q_OS_ANDROID
    PrepareLogFile();
#endif

    sNoData = QString("NoData");
    pNoNetWindow = new NoNetWindow(this);

    // Creating a periodic Server Discovery Service
    connect(&connectionTimer, SIGNAL(timeout()),
            this, SLOT(onConnectionTimerElapsed()));
    pServerDiscoverer = new ServerDiscoverer();
    connect(pServerDiscoverer, SIGNAL(serverConnected(QWebSocket*)),
            this, SLOT(onServerConnected(QWebSocket*)));
    pServerDiscoverer->Discover();

    // Is the network available ?
    connect(&networkReadyTimer, SIGNAL(timeout()),
            this, SLOT(onTimeToCheckNetwork()));
    if(!isConnectedToNetwork()) {// No. Wait until network become ready
        pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con la Rete"));
        networkReadyTimer.start(NETWORK_CHECK_TIME);
        logMessage(sFunctionName,  QString(" waiting for network..."));
    }
    else {// Yes. Start the connection attempts
        networkReadyTimer.stop();
        connectionTime = int(CONNECTION_TIME * (1.0 + double(qrand())/double(RAND_MAX)));
        connectionTimer.start(connectionTime);
        pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con il Server"));
    }

    // Creating a Ping-pong timer
    nPong = 0;
    pTimerPing = new QTimer(this);
    connect(pTimerPing, SIGNAL(timeout()),
            this, SLOT(onTimeToEmitPing()));

    //  Creating a  timer to Check Connection
    pTimerCheckConnection = new QTimer(this);
    connect(pTimerCheckConnection, SIGNAL(timeout()),
            this, SLOT(onTimeToCheckConnection()));

    pNoNetWindow->showFullScreen();
}


chooserWidget::~chooserWidget() {
}


bool
chooserWidget::PrepareLogFile() {
#ifdef LOG_MESG
    QString sFunctionName = " chooserWidget::PrepareLogFile ";
    Q_UNUSED(sFunctionName)

    QFileInfo checkFile(logFileName);
    if(checkFile.exists() && checkFile.isFile()) {
        QDir renamed;
        renamed.remove(logFileName+QString(".bkp"));
        renamed.rename(logFileName, logFileName+QString(".bkp"));
    }
    logFile = new QFile(logFileName);
    if (!logFile->open(QIODevice::WriteOnly)) {
        QMessageBox::information(this, tr("Segnapunti Volley"),
                                 tr("Impossibile aprire il file %1: %2.")
                                 .arg(logFileName).arg(logFile->errorString()));
        delete logFile;
        logFile = NULL;
    }
#endif
    return true;
}


bool
chooserWidget::isConnectedToNetwork() {
    QString sFunctionName = " chooserWidget::isConnectedToNetwork ";
    Q_UNUSED(sFunctionName)
    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    bool result = false;

    for(int i=0; i<ifaces.count(); i++) {
        QNetworkInterface iface = ifaces.at(i);
        if(iface.flags().testFlag(QNetworkInterface::IsUp) &&
           iface.flags().testFlag(QNetworkInterface::IsRunning) &&
           iface.flags().testFlag(QNetworkInterface::CanMulticast) &&
          !iface.flags().testFlag(QNetworkInterface::IsLoopBack))
        {
            for(int j=0; j<iface.addressEntries().count(); j++) {
                // we have an interface that is up, and has an ip address
                // therefore the link is present
                if(result == false)
                    result = true;
            }
        }
    }
    logMessage(sFunctionName, result ? QString("true") : QString("false"));
    return result;
}


void
chooserWidget::onTimeToCheckNetwork() {
    QString sFunctionName = " chooserWidget::onTimeToCheckNetwork ";
    Q_UNUSED(sFunctionName)
    if(!isConnectedToNetwork()) {
        logMessage(sFunctionName, QString("Waiting for network..."));
    }
    else {
        networkReadyTimer.stop();
        connectionTime = int(CONNECTION_TIME * (1.0 + double(qrand())/double(RAND_MAX)));
        connectionTimer.start(connectionTime);
        pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con il Server"));
    }
    pNoNetWindow->showFullScreen();
}


void
chooserWidget::logMessage(QString sFunctionName, QString sMessage) {
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
chooserWidget::onServerConnected(QWebSocket* pSocket) {
    QString sFunctionName = " chooserWidget::onServerConnected ";
    Q_UNUSED(sFunctionName)
    connectionTimer.stop();

    pWebSocket = pSocket;
    serverUrl = pWebSocket->requestUrl();
    logMessage(sFunctionName,
               QString("WebSocket connected to: %1")
               .arg(serverUrl.toString()));

    connect(pWebSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onWebSocketError(QAbstractSocket::SocketError)));
    connect(pWebSocket, SIGNAL(textMessageReceived(QString)),
            this, SLOT(onTextMessageReceived(QString)));
    connect(pWebSocket, SIGNAL(disconnected()),
            this, SLOT(onServerDisconnected()));
    connect(pWebSocket, SIGNAL(pong(quint64, QByteArray)),
            this, SLOT(onPongReceived(quint64, QByteArray)));

    pNoNetWindow->setDisplayedText(tr("In Attesa della Configurazione"));

    QString sMessage;
    sMessage = QString("<getConf>1</getConf>");
    qint64 bytesSent = pWebSocket->sendTextMessage(sMessage);
    if(bytesSent != sMessage.length()) {
        logMessage(sFunctionName, QString("Unable to ask panel configuration"));
    }

    pingPeriod = int(PING_PERIOD * (1.0 + double(qrand())/double(RAND_MAX)));
    pTimerPing->start(pingPeriod);

    // Periodic check of connection
    pTimerCheckConnection->start(CONNECTION_CHECK_TIME);
}


void
chooserWidget::onWebSocketError(QAbstractSocket::SocketError error) {
    QString sFunctionName = " chooserWidget::onWebSocketError ";

    logMessage(sFunctionName,
               QString("%1 %2 Error %3")
               .arg(pWebSocket->peerAddress().toString())
               .arg(pWebSocket->errorString())
               .arg(error));
    if(!disconnect(pWebSocket, 0, 0, 0)) {
        logMessage(sFunctionName, QString("Unable to disconnect signals from WebSocket"));
    }
    if(pWebSocket->isValid())
        pWebSocket->close(QWebSocketProtocol::CloseCodeAbnormalDisconnection, pWebSocket->errorString());
    pTimerPing->stop();
    pTimerCheckConnection->stop();

    if(!isConnectedToNetwork()) {
        pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con la Rete"));
        networkReadyTimer.start(NETWORK_CHECK_TIME);
        logMessage(sFunctionName, QString("Waiting for network..."));
    }
    else {
        networkReadyTimer.stop();
        connectionTime = int(CONNECTION_TIME * (1.0 + double(qrand())/double(RAND_MAX)));
        connectionTimer.start(connectionTime);
        pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con il Server"));
    }
    pNoNetWindow->showFullScreen();
}


void
chooserWidget::onServerDisconnected() {
    QString sFunctionName = " ScorePanel::onServerDisconnected ";

    pTimerPing->stop();
    pTimerCheckConnection->stop();

    pWebSocket = Q_NULLPTR;
    logMessage(sFunctionName, QString("WebSocket disconnected !"));
    connectionTime = int(CONNECTION_TIME * (1.0 + double(qrand())/double(RAND_MAX)));

    if(!isConnectedToNetwork()) {
        pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con la Rete"));
        networkReadyTimer.start(NETWORK_CHECK_TIME);
        logMessage(sFunctionName, QString("Waiting for network..."));
    }
    else {
        networkReadyTimer.stop();
        connectionTime = int(CONNECTION_TIME * (1.0 + double(qrand())/double(RAND_MAX)));
        connectionTimer.start(connectionTime);
        pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con il Server"));
    }
    pNoNetWindow->showFullScreen();
}


void
chooserWidget::onConnectionTimerElapsed() {
    QString sFunctionName = " chooserWidget::onConnectionTimerElapsed ";
    Q_UNUSED(sFunctionName)

    if(pWebSocket) pWebSocket->close();
    pWebSocket = Q_NULLPTR;
    if(!isConnectedToNetwork()) {
        pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con la Rete"));
        connectionTimer.stop();
        networkReadyTimer.start(NETWORK_CHECK_TIME);
        logMessage(sFunctionName, QString("Waiting for network..."));
    }
    else {
        logMessage(sFunctionName, QString("Connection time out... retrying"));
        pNoNetWindow->showFullScreen();
        pServerDiscoverer->Discover();
    }
}


void
chooserWidget::onTimeToEmitPing() {
    QString sFunctionName = " chooserWidget::onTimeToEmitPing ";
    Q_UNUSED(sFunctionName)
    pWebSocket->ping();
}


void
chooserWidget::onPongReceived(quint64 elapsed, QByteArray payload) {
    QString sFunctionName = " chooserWidget::onPongReceived ";
    nPong++;
    Q_UNUSED(sFunctionName)
    Q_UNUSED(elapsed)
    Q_UNUSED(payload)
}


void
chooserWidget::onTimeToCheckConnection() {
    QString sFunctionName = " chooserWidget::onTimeToCheckConnection ";
    Q_UNUSED(sFunctionName)
    if(nPong > 0) {
        nPong = 0;
        return;
    }// else nPong==0
    logMessage(sFunctionName, QString(": Pong took too long. Disconnecting !"));
    pTimerPing->stop();
    pTimerCheckConnection->stop();
    nPong = 0;
    pWebSocket->close(QWebSocketProtocol::CloseCodeGoingAway, tr("Pong time too long"));
}


void
chooserWidget::onTextMessageReceived(QString sMessage) {
    QString sFunctionName = " chooserWidget::onTextMessageReceived ";

    QString sToken;
    bool ok;
    int iVal;

    // Which game we would like to play ?
    sToken = XML_Parse(sMessage, "setConf");
    if(sToken != sNoData) {
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<FIRST_PANEL || iVal>LAST_PANEL) {
            logMessage(sFunctionName,
                       QString("Illegal configuration received: %1")
                       .arg(iVal));
            iVal = FIRST_PANEL;
        }
        disconnect(pWebSocket, SIGNAL(textMessageReceived(QString)),
                 this, SLOT(onTextMessageReceived(QString)));
        if(pScorePanel) {
            delete pScorePanel;
            pScorePanel = Q_NULLPTR;
        }
        if(iVal == VOLLEY_PANEL) {
            pScorePanel = new SegnapuntiVolley(pWebSocket, logFile, true);
            pScorePanel->showFullScreen();
            pNoNetWindow->hide();
        }
        else if(iVal == BASKET_PANEL) {
            pScorePanel = new SegnapuntiBasket(pWebSocket, logFile, true);
            pScorePanel->showFullScreen();
            pNoNetWindow->hide();
        }
    }// getConf

}


QString
chooserWidget::XML_Parse(QString input_string, QString token) {
    QString sFunctionName = " chooserWidget::XML_Parse ";
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

