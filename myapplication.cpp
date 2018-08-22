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
#include <QSettings>

#include "myapplication.h"
#include "serverdiscoverer.h"
#include "messagewindow.h"
#include "utility.h"


#define NETWORK_CHECK_TIME    3000 // In msec

/*!
 * \brief MyApplication::MyApplication The client part of the ScorePanel System.
 * \param argc Unused
 * \param argv Unused
 *
 * It is responsible to start the "Server Discovery" process.
 * It allows the client to connect to the Server without knowing
 * its network address;
 * It wait until a Network connection is Up and Ready and then
 * allows to start the panel requested by the Server.
 */
MyApplication::MyApplication(int& argc, char ** argv)
    : QApplication(argc, argv)
    , logFile(Q_NULLPTR)
    , pServerDiscoverer(Q_NULLPTR)
    , pNoNetWindow(Q_NULLPTR)
{
    pSettings = new QSettings("Gabriele Salvato", "Score Panel");
    sLanguage = pSettings->value("language/current",  QString("Italiano")).toString();
    if(sLanguage == QString("Italiano")) {
        QCoreApplication::removeTranslator(&Translator);
    }
    else if(sLanguage == QString("English")) {
        Translator.load(":/panelChooser_en");
        QCoreApplication::installTranslator(&Translator);
    }
    else {
        QCoreApplication::removeTranslator(&Translator);
    }

    // We want the cursor set for all widgets,
    // even when outside the window then:
    setOverrideCursor(Qt::BlankCursor);

    // Initialize the random number generator
    QTime time(QTime::currentTime());
    qsrand(uint(time.msecsSinceStartOfDay()));

    // Starts a timer to check for a ready network connection
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
    pNoNetWindow = new MessageWindow(Q_NULLPTR);
    pNoNetWindow->setDisplayedText(tr("In Attesa della Connessione con la Rete"));

    // Create a "PanelServer Discovery Service" but not start it
    // until we are sure that there is an active network connection
    pServerDiscoverer = new ServerDiscoverer(logFile);
    connect(pServerDiscoverer, SIGNAL(checkNetwork()),
            this, SLOT(onRecheckNetwork()));

    pNoNetWindow->showFullScreen();

    // When the network becomes available we will start the
    // "PanelServer Discovery Service".
    // Let's start the periodic check for the network
    networkReadyTimer.start(NETWORK_CHECK_TIME);

    // And now it is time to check if the Network
    // is already up and working
    onTimeToCheckNetwork();
}


/*!
 * \brief MyApplication::onTimeToCheckNetwork Periodic "Network Available" check.
 *
 * Start the "ServerDiscovery" Service when the Network is Up
 */
void
MyApplication::onTimeToCheckNetwork() {
    networkReadyTimer.stop();
    if(isConnectedToNetwork()) {
        // Let's start the "Server Discovery Service"
        if(!pServerDiscoverer->Discover()) {
            // If the service is unable to start then probably
            // The network connection went down.
            pNoNetWindow->setDisplayedText(tr("Errore: Server Discovery Non Avviato"));
            // then restart checking...
            networkReadyTimer.start(NETWORK_CHECK_TIME);
        }
        else {
            // Network ready and Server Discovery started !
            if(pNoNetWindow->isVisible())
                pNoNetWindow->hide();
        }
    }
    else {// The network connection is down !
        if(!pNoNetWindow->isVisible())
            // No other window should obscure this one
            pNoNetWindow->showFullScreen();
    }
}


/*!
 * \brief MyApplication::onRecheckNetwork Invoked when the Server disconnect
 *
 * Invoked by the "ServerDiscover" Service if the Server
 * disconnected, in order to restart the periodic check for
 * network interfaces Up and Ready.
 */
void
MyApplication::onRecheckNetwork() {
    // No other window should obscure this one
    if(!pNoNetWindow->isVisible())
        pNoNetWindow->showFullScreen();
    networkReadyTimer.start(NETWORK_CHECK_TIME);
}


/*!
 * \brief MyApplication::isConnectedToNetwork
 * \return true if the Network is Up and Running.
 *
 * It checks if there are network interfaces ready to
 * connect to a server.
 */
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


/*!
 * \brief MyApplication::PrepareLogFile Prepare a log file for the session log
 * \return true
 *
 * This function create the file only if enabled at compilation time
 * by defining the macro LOG_MSG.
 */
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
