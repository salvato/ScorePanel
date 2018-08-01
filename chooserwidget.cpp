#include <QNetworkInterface>
//#include <QWebSocket>
#include <QFile>
#include <QMessageBox>
#include <QDir>
//#include <QProcessEnvironment>
#include <QStandardPaths>
//#include <QApplication>
//#include <QDesktopWidget>

#include "chooserwidget.h"
#include "serverdiscoverer.h"
#include "nonetwindow.h"
//#include "scorepanel.h"
//#include "segnapuntivolley.h"
//#include "segnapuntibasket.h"
//#include "segnapuntihandball.h"
#include "utility.h"


#define NETWORK_CHECK_TIME    3000


chooserWidget::chooserWidget(QWidget *parent)
    : QObject(parent)
    , logFile(Q_NULLPTR)
    , pServerDiscoverer(Q_NULLPTR)
    , pNoNetWindow(Q_NULLPTR)
//    , pServerSocket(Q_NULLPTR)
//    , pScorePanel(Q_NULLPTR)
{
    QTime time(QTime::currentTime());
    qsrand(uint(time.msecsSinceStartOfDay()));
// On Android, no Log Files !
#ifndef Q_OS_ANDROID
    QString sBaseDir;
    sBaseDir = QDir::homePath();
    if(!sBaseDir.endsWith(QString("/"))) sBaseDir+= QString("/");
    logFileName = QString("%1score_panel.txt").arg(sBaseDir);
    PrepareLogFile();
#endif
}


void
chooserWidget::start() {
    // Create a message window
    pNoNetWindow = new NoNetWindow(Q_NULLPTR);
    pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con la Rete"));

    // Creating a PanelServer Discovery Service
    pServerDiscoverer = new ServerDiscoverer(logFile);
//    connect(pServerDiscoverer, SIGNAL(serverFound(QString, int)),
//            this, SLOT(onServerFound(QString, int)));

    // This timer allow periodic check of a ready network
    connect(&networkReadyTimer, SIGNAL(timeout()),
            this, SLOT(onTimeToCheckNetwork()));

    pNoNetWindow->showFullScreen();
    networkReadyTimer.start(NETWORK_CHECK_TIME);
    onTimeToCheckNetwork();
}


chooserWidget::~chooserWidget() {
}


// Periodic Network Available retry check
void
chooserWidget::onTimeToCheckNetwork() {
    if(isConnectedToNetwork()) {
        networkReadyTimer.stop();
        if(!pServerDiscoverer->Discover())
            pNoNetWindow->setDisplayedText(tr("Errore: Server Discovery Non Avviato"));
        else
            pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con il Server"));
    }
    // No other window should obscure this one
    pNoNetWindow->showFullScreen();
}


/*
        connectionTime = int(CONNECTION_TIME * (1.0 + double(qrand())/double(RAND_MAX)));
        connectionTimer.start(connectionTime);

        // This timer allow retrying connection attempts
        connect(&connectionTimer, SIGNAL(timeout()),
                this, SLOT(onConnectionTimerElapsed()));
        startServerDiscovery();
        connectionTime = int(CONNECTION_TIME * (1.0 + (double(qrand())/double(RAND_MAX))));
        connectionTimer.start(connectionTime);
*/



// We prepare a file to write the session log
bool
chooserWidget::PrepareLogFile() {
#ifdef LOG_MESG
    QFileInfo checkFile(logFileName);
    if(checkFile.exists() && checkFile.isFile()) {
        QDir renamed;
        renamed.remove(logFileName+QString(".bkp"));
        renamed.rename(logFileName, logFileName+QString(".bkp"));
    }
    logFile = new QFile(logFileName);
    if (!logFile->open(QIODevice::WriteOnly)) {
        QMessageBox::information(Q_NULLPTR, "Segnapunti Volley",
                                 QString("Impossibile aprire il file %1: %2.")
                                 .arg(logFileName).arg(logFile->errorString()));
        delete logFile;
        logFile = Q_NULLPTR;
    }
#endif
    return true;
}


// returns true if the network is available
bool
chooserWidget::isConnectedToNetwork() {
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
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               result ? QString("true") : QString("false"));
#endif
    return result;
}

/*
void
chooserWidget::onServerFound(QString serverUrl, int panelType) {
    connectionTimer.stop();
    if(pScorePanel) {// Delete old instance to prevent memory leaks
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
    pNoNetWindow->hide();
}


void
chooserWidget::onExitProgram() {
    logMessage(logFile, Q_FUNC_INFO, QString("Exiting..."));
    connectionTimer.stop();
    if(pServerDiscoverer) {
        pServerDiscoverer->deleteLater();
        pServerDiscoverer = Q_NULLPTR;
    }
    if(pNoNetWindow) delete pNoNetWindow;
    pNoNetWindow = Q_NULLPTR;
    if(logFile)
        logFile->close();
    delete logFile;
    exit(0);
}


void
chooserWidget::onPanelClosed() {
    startServerDiscovery();
    logMessage(logFile, Q_FUNC_INFO, QString("Restarting..."));
}


void
chooserWidget::onConnectionTimerElapsed() {
    if(!isConnectedToNetwork()) {
        pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con la Rete"));
        connectionTimer.stop();
        networkReadyTimer.start(NETWORK_CHECK_TIME);
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Waiting for network..."));
#endif
    }
    else {
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Connection time out... retrying"));
#endif
        pNoNetWindow->showFullScreen();
        pServerDiscoverer->Discover();
    }
}
*/
