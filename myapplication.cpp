/*
 *
Copyright (C) 2016  Gabriele Salvato

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/
#include <QNetworkInterface>
#include <QFile>
#include <QMessageBox>
#include <QDir>
#include <QStandardPaths>

#include "myapplication.h"
#include "serverdiscoverer.h"
#include "nonetwindow.h"
#include "utility.h"


#define NETWORK_CHECK_TIME    3000


MyApplication::MyApplication(int& argc, char ** argv)
    : QApplication(argc, argv)
    , logFile(Q_NULLPTR)
    , pServerDiscoverer(Q_NULLPTR)
    , pNoNetWindow(Q_NULLPTR)
{
    // We want the cursor set for all widgets,
    // even when outside the window then:
    setOverrideCursor(Qt::BlankCursor);

    // Initialize the random number generator
    QTime time(QTime::currentTime());
    qsrand(uint(time.msecsSinceStartOfDay()));

    // This timer allow periodic check of a ready network
    connect(&networkReadyTimer, SIGNAL(timeout()),
            this, SLOT(onTimeToCheckNetwork()));

    // On Android, no Log Files !
    #ifndef Q_OS_ANDROID
        QString sBaseDir;
        sBaseDir = QDir::homePath();
        if(!sBaseDir.endsWith(QString("/"))) sBaseDir+= QString("/");
        logFileName = QString("%1score_panel.txt").arg(sBaseDir);
        PrepareLogFile();
    #endif

    // Create a message window
    pNoNetWindow = new NoNetWindow(Q_NULLPTR);
    pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con la Rete"));

    // Creating a PanelServer Discovery Service
    pServerDiscoverer = new ServerDiscoverer(logFile);
    connect(pServerDiscoverer, SIGNAL(checkNetwork()),
            this, SLOT(onRecheckNetwork()));

    pNoNetWindow->showFullScreen();
    // When the Network becomes available start the
    // "ServerDiscovery" service.

    // Let's start the periodic check for the network
    networkReadyTimer.start(NETWORK_CHECK_TIME);

    // And now it is time to check if the network is already up and working
    onTimeToCheckNetwork();
}


// Periodic Network Available retry check.
// If the network is available start the "server discovery"
void
MyApplication::onTimeToCheckNetwork() {
    // No other window should obscure this one
    if(!pNoNetWindow->isVisible())
        pNoNetWindow->showFullScreen();
    if(isConnectedToNetwork()) {
        networkReadyTimer.stop();
        if(!pServerDiscoverer->Discover())
            pNoNetWindow->setDisplayedText(tr("Errore: Server Discovery Non Avviato"));
        else
            pNoNetWindow->hide();
    }
}


// Invoked by the "ServerDiscover" service when no
// network interface is ready to connect
void
MyApplication::onRecheckNetwork() {
    // No other window should obscure this one
    if(!pNoNetWindow->isVisible())
        pNoNetWindow->showFullScreen();
    networkReadyTimer.start(NETWORK_CHECK_TIME);
}


// We prepare a file to write the session log
bool
MyApplication::PrepareLogFile() {
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
MyApplication::isConnectedToNetwork() {
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
