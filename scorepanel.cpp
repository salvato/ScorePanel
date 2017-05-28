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

#include <QtGlobal>
#include <QtNetwork>
#include <QtWidgets>
#include <QProcess>
#include <QWebSocket>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QTime>
#include <QSettings>


#if defined(Q_PROCESSOR_ARM) & !defined(Q_OS_ANDROID)
    #include "pigpiod_if2.h"
#endif

#include "slidewindow.h"
#include "fileupdater.h"
#include "scorepanel.h"
#include "utility.h"
#include "panelorientation.h"



#define SPOT_UPDATE_PORT      45455
#define SLIDE_UPDATE_PORT     45456
#define PING_PERIOD           3000
#define PONG_CHECK_TIME       30000



// For Raspberry Pi GPIO pin numbering see https://pinout.xyz/
//
// +5V pins 2 or 4 in the 40 pin GPIO connector.
// GND on pins 6, 9, 14, 20, 25, 30, 34 or 39
// in the 40 pin GPIO connector.
//
// Samwa servo pinout
// 1) PWM Signal
// 2) GND
// 3) +5V


ScorePanel::ScorePanel(QUrl serverUrl, QFile *_logFile, QWidget *parent)
    : QWidget(parent)
    , isMirrored(false)
    , isScoreOnly(false)
    , pPanelServerSocket(Q_NULLPTR)
    , videoPlayer(NULL)
    , cameraPlayer(NULL)
    , iCurrentSpot(0)
    , iCurrentSlide(0)
    , logFile(_logFile)
    , panPin(14) // GPIO Numbers are Broadcom (BCM) numbers
                 // BCM14 is Pin 8 in the 40 pin GPIO connector.
    , tiltPin(26)// GPIO Numbers are Broadcom (BCM) numbers
                 // BCM26 IS Pin 37 in the 40 pin GPIO connector.
    , gpioHostHandle(-1)
{
    QString sFunctionName = " ScorePanel::ScorePanel ";
    Q_UNUSED(sFunctionName)

    pPanel = new QWidget(this);

    // Turns off the default window title hints.
    setWindowFlags(Qt::CustomizeWindowHint);

    pSettings = new QSettings(tr("Gabriele Salvato"), tr("Score Panel"));
    isMirrored  = pSettings->value(tr("panel/orientation"),  false).toBool();
    isScoreOnly = pSettings->value(tr("panel/scoreOnly"),  false).toBool();

    QString sBaseDir;
#ifdef Q_OS_ANDROID
    sBaseDir = QString("/storage/extSdCard/");
#else
    sBaseDir = QDir::homePath();
#endif
    if(!sBaseDir.endsWith(QString("/"))) sBaseDir+= QString("/");

    // Spot management
    pSpotUpdaterThread = Q_NULLPTR;
    pSpotUpdater   = Q_NULLPTR;
    spotUpdatePort = SPOT_UPDATE_PORT;
    sSpotDir = QString("%1spots/").arg(sBaseDir);

    // Slide management
    pSlideUpdaterThread = Q_NULLPTR;
    pSlideUpdater   = Q_NULLPTR;
    slideUpdatePort = SLIDE_UPDATE_PORT;
    sSlideDir= QString("%1slides/").arg(sBaseDir);

    // Camera management
    initCamera();

    // Ping pong to check the server status
    pTimerPing = new QTimer(this);
    connect(pTimerPing, SIGNAL(timeout()),
            this, SLOT(onTimeToEmitPing()));
    pTimerCheckPong = new QTimer(this);
    connect(pTimerCheckPong, SIGNAL(timeout()),
            this, SLOT(onTimeToCheckPong()));

    // Slide Window
    pMySlideWindow = new SlideWindow();
    pMySlideWindow->setSlideDir(sSlideDir);
    pMySlideWindow->stopSlideShow();
    pMySlideWindow->hide();

    // We are ready to  connect to the remote Panel Server
    pPanelServerSocket = new QWebSocket();
    connect(pPanelServerSocket, SIGNAL(connected()),
            this, SLOT(onPanelServerConnected()));
    connect(pPanelServerSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onPanelServerSocketError(QAbstractSocket::SocketError)));
    pPanelServerSocket->open(QUrl(serverUrl));
}


ScorePanel::~ScorePanel() {
    if(pPanelServerSocket)
        disconnect(pPanelServerSocket, 0, 0, 0);
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    if(gpioHostHandle>=0) {
        pigpio_stop(gpioHostHandle);
    }
#endif
    if(pSettings) delete pSettings;
    pSettings = Q_NULLPTR;
    if(pTimerPing) delete pTimerPing;
    pTimerPing = Q_NULLPTR;
    if(pTimerCheckPong) delete pTimerCheckPong;
    pTimerCheckPong = Q_NULLPTR;

    doProcessCleanup();

    if(pPanelServerSocket) delete pPanelServerSocket;
    pPanelServerSocket = Q_NULLPTR;
}


void
ScorePanel::buildLayout() {
    QWidget* oldPanel = pPanel;
    pPanel = new QWidget(this);
    QVBoxLayout *panelLayout = new QVBoxLayout();
    panelLayout->addLayout(createPanel());
    pPanel->setLayout(panelLayout);
    if(!layout()) {
        QVBoxLayout *mainLayout = new QVBoxLayout();
        setLayout(mainLayout);
     }
    layout()->addWidget(pPanel);
    if(oldPanel != Q_NULLPTR)
        delete oldPanel;
}


// Ping pong managemet
void
ScorePanel::onTimeToEmitPing() {
    QString sFunctionName = " ScorePanel::onTimeToEmitPing ";
    Q_UNUSED(sFunctionName)
    pPanelServerSocket->ping();
}


void
ScorePanel::onPongReceived(quint64 elapsed, QByteArray payload) {
    QString sFunctionName = " ScorePanel::onPongReceived ";
    Q_UNUSED(sFunctionName)
    Q_UNUSED(elapsed)
    Q_UNUSED(payload)
    nPong++;
}


void
ScorePanel::onTimeToCheckPong() {
    QString sFunctionName = " ScorePanel::onTimeToCheckPong ";
    Q_UNUSED(sFunctionName)
    if(nPong > 0) {
        nPong = 0;
        return;
    }// else nPong==0
    logMessage(logFile,
               sFunctionName,
               QString(": Pong took too long. Disconnecting !"));
    pTimerPing->stop();
    pTimerCheckPong->stop();
    nPong = 0;
    // Cleanup will be done in the close
    pPanelServerSocket->close(QWebSocketProtocol::CloseCodeGoingAway, tr("Pong time too long"));
}
// End Ping pong management


// Spot Server Management routines
void
ScorePanel::onSpotUpdaterClosed(bool bError) {
    QString sFunctionName = " ScorePanel::onSpotUpdaterClosed ";
    Q_UNUSED(sFunctionName)
    if(pSpotUpdater) disconnect(pSpotUpdater, 0, 0, 0);
    if(pSpotUpdaterThread) {
        pSpotUpdaterThread->exit(0);
        if(pSpotUpdaterThread->wait(3000)) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Spot Update Thread regularly closed"));
        }
        else {
            logMessage(logFile,
                       sFunctionName,
                       QString("Spot Update Thread forced to close"));
        }
        if(bError) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Spot Updater closed with errors"));
        }
        else {
            logMessage(logFile,
                       sFunctionName,
                       QString("Spot Updater closed without errors"));
        }
    }
    if(pSpotUpdater) pSpotUpdater->deleteLater();
    pSpotUpdater = Q_NULLPTR;
    pSpotUpdaterThread = Q_NULLPTR;
}


void
ScorePanel::onSpotUpdaterThreadDone() {
    QString sFunctionName = " ScorePanel::onSpotUpdaterThreadDone ";
    Q_UNUSED(sFunctionName)
    logMessage(logFile,
               sFunctionName,
               QString("Spot Update Thread regularly closed"));
    if(pSpotUpdater) {
        disconnect(pSpotUpdater, 0, 0, 0);
        pSpotUpdater->deleteLater();
    }
    pSpotUpdater = Q_NULLPTR;
    pSpotUpdaterThread->deleteLater();
    pSpotUpdaterThread = Q_NULLPTR;
}


void
ScorePanel::closeSpotUpdaterThread() {
    QString sFunctionName = " ScorePanel::closeSpotUpdaterThread ";
    if(pSpotUpdaterThread) {
        if(pSpotUpdaterThread->isRunning()) {
            disconnect(pSpotUpdaterThread, 0, 0, 0);
            logMessage(logFile,
                       sFunctionName,
                       QString("Closing Spot Update Thread"));
            emit terminateUpdateSpots();
            pSpotUpdaterThread->requestInterruption();
            if(pSpotUpdaterThread->wait(3000)) {
                logMessage(logFile,
                           sFunctionName,
                           QString("Spot Update Thread regularly closed"));
            }
            else {
                logMessage(logFile,
                           sFunctionName,
                           QString("Spot Update Thread forced to close"));
            }
        }
        pSpotUpdaterThread->deleteLater();
        pSpotUpdaterThread = Q_NULLPTR;
    }
    if(pSpotUpdater) {
        disconnect(pSpotUpdater, 0, 0, 0);
        pSpotUpdater->deleteLater();
    }
}
// End of Spot Server Management routines


// Slide Server Management routines
void
ScorePanel::onSlideUpdaterClosed(bool bError) {
    QString sFunctionName = " ScorePanel::onSlideUpdaterClosed ";
    Q_UNUSED(sFunctionName)
    if(pSlideUpdater) disconnect(pSlideUpdater, 0, 0, 0);
    if(pSlideUpdaterThread) {
        pSlideUpdaterThread->exit(0);
        if(pSlideUpdaterThread->wait(3000)) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Slide Update Thread regularly closed"));
        }
        else {
            logMessage(logFile,
                       sFunctionName,
                       QString("Slide Update Thread forced to close"));
        }
        if(bError) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Slide Updater closed with errors"));
        }
        else {
            logMessage(logFile,
                       sFunctionName,
                       QString("Slide Updater closed without errors"));
        }
    }
    if(pSlideUpdater) pSlideUpdater->deleteLater();
    pSlideUpdater = Q_NULLPTR;
    pSlideUpdaterThread = Q_NULLPTR;
}


void
ScorePanel::onSlideUpdaterThreadDone() {
    QString sFunctionName = " ScorePanel::onSlideUpdaterThreadDone ";
    Q_UNUSED(sFunctionName)
    logMessage(logFile,
               sFunctionName,
               QString("Slide Update Thread regularly closed"));
    if(pSlideUpdater) {
        disconnect(pSlideUpdater, 0, 0, 0);
        pSlideUpdater->deleteLater();
    }
    pSlideUpdater = Q_NULLPTR;
    pSlideUpdaterThread->deleteLater();
    pSlideUpdaterThread = Q_NULLPTR;
}


void
ScorePanel::closeSlideUpdaterThread() {
    QString sFunctionName = " ScorePanel::closeSlideUpdaterThread ";
    if(pSlideUpdaterThread) {
        if(pSlideUpdaterThread->isRunning()) {
            disconnect(pSlideUpdaterThread, 0, 0, 0);
            logMessage(logFile,
                       sFunctionName,
                       QString("Closing Slide Update Thread"));
            emit terminateUpdateSlides();
            pSlideUpdaterThread->requestInterruption();
            if(pSlideUpdaterThread->wait(3000)) {
                logMessage(logFile,
                           sFunctionName,
                           QString("Slide Update Thread regularly closed"));
            }
            else {
                logMessage(logFile,
                           sFunctionName,
                           QString("Slide Update Thread forced to close"));
            }
        }
        pSlideUpdaterThread->deleteLater();
        pSlideUpdaterThread = Q_NULLPTR;
    }
    if(pSlideUpdater) {
        disconnect(pSlideUpdater, 0, 0, 0);
        pSlideUpdater->deleteLater();
    }
}
// End of Slide Server Management routines


//==================
// Panel management
//==================
void
ScorePanel::setScoreOnly(bool bScoreOnly) {
    QString sFunctionName;
    isScoreOnly = bScoreOnly;
    if(isScoreOnly) {
        // Terminate, if running, videos, Slides and Camera
        if(videoPlayer) {
            disconnect(videoPlayer, 0, 0, 0);
    #if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
            videoPlayer->write("q", 1);
            system("xrefresh -display :0");
    #else
            videoPlayer->kill();
    #endif
            logMessage(logFile,
                       sFunctionName,
                       QString("Killing Video Player..."));
            videoPlayer->waitForFinished(3000);
            videoPlayer->deleteLater();
            videoPlayer = Q_NULLPTR;
        }
        if(cameraPlayer) {
            cameraPlayer->kill();
            cameraPlayer->waitForFinished(3000);
            cameraPlayer->deleteLater();
            cameraPlayer = Q_NULLPTR;
        }
        if(pMySlideWindow) {
            pMySlideWindow->deleteLater();
            pMySlideWindow = Q_NULLPTR;
        }
    }
}


bool
ScorePanel::getScoreOnly() {
    return isScoreOnly;
}


void
ScorePanel::onPanelServerConnected() {
    QString sFunctionName = " ScorePanel::onPanelServerConnected ";
    Q_UNUSED(sFunctionName)

    connect(pPanelServerSocket, SIGNAL(disconnected()),
            this, SLOT(onPanelServerDisconnected()));

    QString sMessage;
    sMessage = QString("<getStatus>1</getStatus>");
    qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
    if(bytesSent != sMessage.length()) {
        logMessage(logFile,
                   sFunctionName,
                   QString("Unable to ask the initial status"));
    }

    // Create the Spot Updater Thread
    pSpotUpdaterThread = new QThread();
    connect(pSpotUpdaterThread, SIGNAL(finished()),
            this, SLOT(onSpotUpdaterThreadDone()));
    // And the Spot Update Server
    QString spotUpdateServer;
    spotUpdateServer= QString("ws://%1:%2").arg(pPanelServerSocket->peerAddress().toString()).arg(spotUpdatePort);
    pSpotUpdater = new FileUpdater(QString("SpotUpdater"), spotUpdateServer, logFile);
    pSpotUpdater->moveToThread(pSpotUpdaterThread);
    connect(pSpotUpdater, SIGNAL(connectionClosed(bool)),
            this, SLOT(onSpotUpdaterClosed(bool)));
    connect(this, SIGNAL(updateSpots()),
            pSpotUpdater, SLOT(startUpdate()));
    connect(this, SIGNAL(terminateUpdateSpots()),
            pSpotUpdater, SLOT(terminate()));
    pSpotUpdaterThread->start();
    pSpotUpdater->setDestination(sSpotDir, QString("*.mp4"));
    logMessage(logFile,
               sFunctionName,
               QString("Spot Update thread started"));

    // Create the Slide Updater Thread
    pSlideUpdaterThread = new QThread();
    connect(pSlideUpdaterThread, SIGNAL(finished()),
            this, SLOT(onSlideUpdaterThreadDone()));
    // And the Slide Update Server
    QString slideUpdateServer;
    slideUpdateServer= QString("ws://%1:%2").arg(pPanelServerSocket->peerAddress().toString()).arg(slideUpdatePort);
    pSlideUpdater = new FileUpdater(QString("SlideUpdater"), slideUpdateServer, logFile);
    pSlideUpdater->moveToThread(pSlideUpdaterThread);
    connect(pSlideUpdater, SIGNAL(connectionClosed(bool)),
            this, SLOT(onSlideUpdaterClosed(bool)));
    connect(this, SIGNAL(updateSlides()),
            pSlideUpdater, SLOT(startUpdate()));
    connect(this, SIGNAL(terminateUpdateSlides()),
            pSlideUpdater, SLOT(terminate()));
    pSlideUpdaterThread->start();
    pSlideUpdater->setDestination(sSlideDir, QString("*.jpg *.jpeg *.png"));
    logMessage(logFile,
               sFunctionName,
               QString("Slide Update thread started"));

    // Start the Ping-Pong to check th Panel Server connection
    nPong = 0;
    pingPeriod = int(PING_PERIOD * (1.0 + double(qrand())/double(RAND_MAX)));
    connect(pPanelServerSocket, SIGNAL(pong(quint64,QByteArray)),
            this, SLOT(onPongReceived(quint64,QByteArray)));
    pTimerPing->start(pingPeriod);
    pTimerCheckPong->start(PONG_CHECK_TIME);

    emit updateSpots();
    emit updateSlides();
}


void
ScorePanel::onPanelServerDisconnected() {
    QString sFunctionName = " ScorePanel::onPanelServerDisconnected ";
    Q_UNUSED(sFunctionName)
    pTimerPing->stop();
    pTimerCheckPong->stop();

    doProcessCleanup();
#ifdef LOG_VERBOSE
    logMessage(logFile,
               sFunctionName,
               QString("emitting panelClosed()"));
#endif
    pPanelServerSocket->deleteLater();
    pPanelServerSocket =Q_NULLPTR;
    emit panelClosed();
}


void
ScorePanel::doProcessCleanup() {
    QString sFunctionName = " ScorePanel::doCleanup ";
    logMessage(logFile,
               sFunctionName,
               QString("Cleaning all processes"));
    if(videoPlayer) {
        disconnect(videoPlayer, 0, 0, 0);
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
        videoPlayer->write("q", 1);
        system("xrefresh -display :0");
#else
        videoPlayer->kill();
#endif
        logMessage(logFile,
                   sFunctionName,
                   QString("Killing Video Player..."));
        videoPlayer->waitForFinished(3000);
        videoPlayer->deleteLater();
        videoPlayer = Q_NULLPTR;
    }
    if(cameraPlayer) {
        cameraPlayer->kill();
        cameraPlayer->waitForFinished(3000);
        cameraPlayer->deleteLater();
        cameraPlayer = Q_NULLPTR;
    }
    if(pMySlideWindow) {
        pMySlideWindow->deleteLater();
        pMySlideWindow = Q_NULLPTR;
    }

    closeSpotUpdaterThread();
    closeSlideUpdaterThread();
}


void
ScorePanel::onPanelServerSocketError(QAbstractSocket::SocketError error) {
    QString sFunctionName = " ScorePanel::onPanelServerSocketError ";
    pTimerPing->stop();
    pTimerCheckPong->stop();

    doProcessCleanup();

    logMessage(logFile,
               sFunctionName,
               QString("%1 %2 Error %3")
               .arg(pPanelServerSocket->peerAddress().toString())
               .arg(pPanelServerSocket->errorString())
               .arg(error));

    if(!disconnect(pPanelServerSocket, 0, 0, 0)) {
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   sFunctionName,
                   QString("Unable to disconnect signals from Sever Socket"));
#endif
    }
    pPanelServerSocket->deleteLater();
    pPanelServerSocket = Q_NULLPTR;
    emit panelClosed();
}


void
ScorePanel::initCamera() {
    QString sFunctionName = " ScorePanel::initCamera ";
    Q_UNUSED(sFunctionName)
    // Get the initial camera position from the past stored values
    cameraPanAngle  = pSettings->value(tr("camera/panAngle"),  0.0).toDouble();
    cameraTiltAngle = pSettings->value(tr("camera/tiltAngle"), 0.0).toDouble();

    PWMfrequency    = 50;     // in Hz
    pulseWidthAt_90 = 600.0;  // in us
    pulseWidthAt90  = 2200;   // in us

#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    gpioHostHandle = pigpio_start((char*)"localhost", (char*)"8888");
    if(gpioHostHandle < 0) {
        logMessage(logFile,
                   sFunctionName,
                   QString("Non riesco ad inizializzare la GPIO."));
    }
    int iResult;
    if(gpioHostHandle >= 0) {
        iResult = set_PWM_frequency(gpioHostHandle, panPin, PWMfrequency);
        if(iResult < 0) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Non riesco a definire la frequenza del PWM per il Pan."));
        }
        double pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraPanAngle+90.0);// In us
        iResult = set_servo_pulsewidth(gpioHostHandle, panPin, u_int32_t(pulseWidth));
        if(iResult < 0) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Non riesco a far partire il PWM per il Pan."));
        }
        iResult = set_PWM_frequency(gpioHostHandle, tiltPin, PWMfrequency);
        if(iResult < 0) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Non riesco a definire la frequenza del PWM per il Tilt."));
        }
        pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraTiltAngle+90.0);// In us
        iResult = set_servo_pulsewidth(gpioHostHandle, tiltPin, u_int32_t(pulseWidth));
        if(iResult < 0) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Non riesco a far partire il PWM per il Tilt."));
        }
    }
#endif
}


void
ScorePanel::resizeEvent(QResizeEvent *event) {
    event->accept();
}


void
ScorePanel::closeEvent(QCloseEvent *event) {
    pSettings->setValue(tr("camera/panAngle"),  cameraPanAngle);
    pSettings->setValue(tr("camera/tiltAngle"), cameraTiltAngle);
    pSettings->setValue(tr("panel/orientation"), isMirrored);

    doProcessCleanup();

#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    if(gpioHostHandle>=0) {
        pigpio_stop(gpioHostHandle);
        gpioHostHandle = -1;
    }
#endif
    event->accept();
}


void
ScorePanel::keyPressEvent(QKeyEvent *event) {
    QString sFunctionName = " ScorePanel::keyPressEvent ";
    Q_UNUSED(sFunctionName);
    if(event->key() == Qt::Key_Escape) {

        doProcessCleanup();

        if(pPanelServerSocket) {
            disconnect(pPanelServerSocket, 0, 0, 0);
            pPanelServerSocket->close(QWebSocketProtocol::CloseCodeNormal, "Client switched off");
        }
        close();
    }
}


void
ScorePanel::onSpotClosed(int exitCode, QProcess::ExitStatus exitStatus) {
    QString sFunctionName = " ScorePanel::onSpotClosed ";
    Q_UNUSED(sFunctionName)
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    if(videoPlayer) {
        videoPlayer->deleteLater();
        videoPlayer = Q_NULLPTR;
        //To avoid a blank screen that sometime appear at the end of omxplayer
        system("xrefresh -display :0");
        QString sMessage = "<closed_spot>1</closed_spot>";
        qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Unable to send %1")
                       .arg(sMessage));
        }
    }
}


void
ScorePanel::onLiveClosed(int exitCode, QProcess::ExitStatus exitStatus) {
    QString sFunctionName = " ScorePanel::onLiveClosed ";
    Q_UNUSED(sFunctionName)
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    if(cameraPlayer) {
        delete cameraPlayer;
        cameraPlayer = NULL;
        //To avoid a blank screen that sometime appear at the end of omxplayer
        system("xrefresh -display :0");
        QString sMessage = "<closed_live>1</closed_live>";
        qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Unable to send %1")
                       .arg(sMessage));
        }
    }
}


void
ScorePanel::onStartNextSpot(int exitCode, QProcess::ExitStatus exitStatus) {
    QString sFunctionName = " ScorePanel::onStartNextSpot ";
    Q_UNUSED(sFunctionName)
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    // Update spot list just in case we are updating the spot list...
    QDir spotDir(sSpotDir);
    spotList = QFileInfoList();
    QStringList nameFilter(QStringList() << "*.mp4");
    spotDir.setNameFilters(nameFilter);
    spotDir.setFilter(QDir::Files);
    spotList = spotDir.entryInfoList();
    if(spotList.count() == 0) {
        return;
    }

    iCurrentSpot = iCurrentSpot % spotList.count();
    if(!videoPlayer) {
        videoPlayer = new QProcess(this);
        connect(videoPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                this, SLOT(onStartNextSpot(int, QProcess::ExitStatus)));
    }
    QString sCommand;
    #ifdef Q_PROCESSOR_ARM
        sCommand = "/usr/bin/omxplayer -o hdmi -r " + spotList.at(iCurrentSpot).absoluteFilePath();
    #else
        sCommand = "/usr/bin/cvlc --no-osd -f " + spotList.at(iCurrentSpot).absoluteFilePath() + " vlc://quit";
    #endif
    videoPlayer->start(sCommand);
#ifdef LOG_VERBOSE
    logMessage(logFile,
               sFunctionName,
               QString("Now playing: %1")
               .arg(spotList.at(iCurrentSpot).absoluteFilePath()));
#endif
    iCurrentSpot = (iCurrentSpot+1) % spotList.count();// Prepare Next Spot
    if(!videoPlayer->waitForStarted(3000)) {
        videoPlayer->kill();
        logMessage(logFile,
                   sFunctionName,
                   QString("Impossibile mandare lo spot"));
        delete videoPlayer;
        videoPlayer = NULL;
    }
}


void
ScorePanel::onBinaryMessageReceived(QByteArray baMessage) {
    QString sFunctionName = " ScorePanel::onBinaryMessageReceived ";
    Q_UNUSED(sFunctionName)
    logMessage(logFile,
               sFunctionName,
               QString("Received %1 bytes").arg(baMessage.size()));
}


void
ScorePanel::onTextMessageReceived(QString sMessage) {
    QString sFunctionName = " ScorePanel::onTextMessageReceived ";
    Q_UNUSED(sFunctionName)
    QString sToken;
    bool ok;
    int iVal;
    QString sNoData = QString("NoData");

    sToken = XML_Parse(sMessage, "kill");
    if(sToken != sNoData){
      iVal = sToken.toInt(&ok);
      if(!ok || iVal<0 || iVal>1)
        iVal = 0;
      if(iVal == 1) {
          disconnect(pPanelServerSocket, 0, 0, 0);
    #ifdef Q_PROCESSOR_ARM
              system("sudo halt");
    #endif
          close();// emit the QCloseEvent that is responsible
                  // to clean up all pending processes
          emit exitRequest();
      }
    }// kill

    sToken = XML_Parse(sMessage, "endspot");
    if(sToken != sNoData) {
        if(videoPlayer) {
            #ifdef Q_PROCESSOR_ARM
                videoPlayer->write("q", 1);
            #else
                videoPlayer->kill();
            #endif
        }
    }// endspot

    sToken = XML_Parse(sMessage, "spot");
    if(sToken != sNoData && !isScoreOnly) {
        QDir spotDir(sSpotDir);
        spotList = QFileInfoList();
        if(spotDir.exists()) {
            QStringList nameFilter(QStringList() << "*.mp4");
            spotDir.setNameFilters(nameFilter);
            spotDir.setFilter(QDir::Files);
            spotList = spotDir.entryInfoList();
        }
        logMessage(logFile,
                   sFunctionName,
                   QString("Found %1 spots").arg(spotList.count()));
        if(!spotList.isEmpty()) {
            iCurrentSpot = iCurrentSpot % spotList.count();
            if(!videoPlayer) {
                videoPlayer = new QProcess(this);
                connect(videoPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                        this, SLOT(onSpotClosed(int, QProcess::ExitStatus)));
                QString sCommand;
                #ifdef Q_PROCESSOR_ARM
                    sCommand = "/usr/bin/omxplayer -o hdmi -r " + spotList.at(iCurrentSpot).absoluteFilePath();
                #else
                    sCommand = "/usr/bin/cvlc --no-osd -f " + spotList.at(iCurrentSpot).absoluteFilePath() + " vlc://quit";
                #endif
                videoPlayer->start(sCommand);
#ifdef LOG_VERBOSE
                logMessage(logFile,
                           sFunctionName,
                           QString("Now playing: %1")
                           .arg(spotList.at(iCurrentSpot).absoluteFilePath()));
#endif
                iCurrentSpot = (iCurrentSpot+1) % spotList.count();// Prepare Next Spot
                if(!videoPlayer->waitForStarted(3000)) {
                    videoPlayer->kill();
                    logMessage(logFile,
                               sFunctionName,
                               QString("Impossibile mandare lo spot."));
                    delete videoPlayer;
                    videoPlayer = NULL;
                }
            }
        }
    }// spot

    sToken = XML_Parse(sMessage, "spotloop");
    if(sToken != sNoData && !isScoreOnly) {
        QDir spotDir(sSpotDir);
        spotList = QFileInfoList();
        if(spotDir.exists()) {
            QStringList nameFilter(QStringList() << "*.mp4");
            spotDir.setNameFilters(nameFilter);
            spotDir.setFilter(QDir::Files);
            spotList = spotDir.entryInfoList();
        }
        logMessage(logFile,
                   sFunctionName,
                   QString("Found %1 spots").arg(spotList.count()));
        if(!spotList.isEmpty()) {
            iCurrentSpot = iCurrentSpot % spotList.count();
            if(!videoPlayer) {
                videoPlayer = new QProcess(this);
                connect(videoPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                        this, SLOT(onStartNextSpot(int, QProcess::ExitStatus)));
                QString sCommand;
                #ifdef Q_PROCESSOR_ARM
                    sCommand = "/usr/bin/omxplayer -o hdmi -r " + spotList.at(iCurrentSpot).absoluteFilePath();
                #else
                    sCommand = "/usr/bin/cvlc --no-osd -f " + spotList.at(iCurrentSpot).absoluteFilePath() + " vlc://quit";
                #endif
                videoPlayer->start(sCommand);
#ifdef LOG_VERBOSE
                logMessage(logFile,
                           sFunctionName,
                           QString("Now playing: %1")
                           .arg(spotList.at(iCurrentSpot).absoluteFilePath()));
#endif
                iCurrentSpot = (iCurrentSpot+1) % spotList.count();// Prepare Next Spot
                if(!videoPlayer->waitForStarted(3000)) {
                    videoPlayer->kill();
                    logMessage(logFile,
                               sFunctionName,
                               QString("Impossibile mandare lo spot."));
                    delete videoPlayer;
                    videoPlayer = NULL;
                }
            }
        }
    }// spotloop

    sToken = XML_Parse(sMessage, "endspotloop");
    if(sToken != sNoData) {
        if(videoPlayer) {
            disconnect(videoPlayer, 0, 0, 0);
            connect(videoPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                    this, SLOT(onSpotClosed(int, QProcess::ExitStatus)));
            #ifdef Q_PROCESSOR_ARM
                videoPlayer->write("q", 1);
            #else
                videoPlayer->kill();
            #endif
        }
    }// endspoloop

    sToken = XML_Parse(sMessage, "slideshow");
    if(sToken != sNoData && !isScoreOnly){
        if(videoPlayer || cameraPlayer)
            return;// No Slide Show if movies are playing or camera is active
#ifdef QT_DEBUG
        pMySlideWindow->showMaximized();
#else
        pMySlideWindow->showFullScreen();
#endif
        pMySlideWindow->startSlideShow();
    }// slideshow

    sToken = XML_Parse(sMessage, "endslideshow");
    if(sToken != sNoData){
        pMySlideWindow->stopSlideShow();
        pMySlideWindow->hide();
    }// endslideshow

    sToken = XML_Parse(sMessage, "live");
    if(sToken != sNoData && !isScoreOnly) {
        if(!cameraPlayer) {
            cameraPlayer = new QProcess(this);
            connect(cameraPlayer, SIGNAL(finished(int, QProcess::ExitStatus)),
                    this, SLOT(onLiveClosed(int, QProcess::ExitStatus)));
            QString sCommand = QString();
            #ifdef Q_PROCESSOR_ARM
                sCommand = tr("/usr/bin/raspivid -f -t 0 -awb auto --vflip");
            #else
                if(!spotList.isEmpty()) {
                    sCommand = "/usr/bin/cvlc --no-osd -f " + spotList.at(iCurrentSpot).absoluteFilePath() + " vlc://quit";
                    iCurrentSpot = (iCurrentSpot+1) % spotList.count();// Prepare Next Spot
                }
            #endif
            if(sCommand != QString()) {
                cameraPlayer->start(sCommand);
                if(!cameraPlayer->waitForStarted(3000)) {
                    cameraPlayer->kill();
                    logMessage(logFile,
                               sFunctionName,
                               QString("Impossibile mandare lo spot."));
                    delete cameraPlayer;
                    cameraPlayer = NULL;
                }
#ifdef LOG_VERBOSE
                else {
                    logMessage(logFile,
                               sFunctionName,
                               QString("Live Show is started."));
                }
#endif
            }
        }// live
    }

    sToken = XML_Parse(sMessage, "endlive");
    if(sToken != sNoData) {
        if(cameraPlayer) {
            cameraPlayer->kill();
#ifdef LOG_VERBOSE
            logMessage(logFile,
                       sFunctionName,
                       QString("Live Show has been closed."));
#endif
        }
    }// endlive

    sToken = XML_Parse(sMessage, "pan");
    if(sToken != sNoData) {
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    if(gpioHostHandle >= 0) {
        cameraPanAngle = sToken.toDouble();
        pSettings->setValue(tr("camera/panAngle"),  cameraPanAngle);
        double pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraPanAngle+90.0);// In ms
        int iResult = set_servo_pulsewidth(gpioHostHandle, panPin, u_int32_t(pulseWidth));
        if(iResult < 0) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Non riesco a far partire il PWM per il Pan."));
        }
    }
#endif
    }// pan

    sToken = XML_Parse(sMessage, "tilt");
    if(sToken != sNoData) {
#if defined(Q_PROCESSOR_ARM) && !defined(Q_OS_ANDROID)
    if(gpioHostHandle >= 0) {
        cameraTiltAngle = sToken.toDouble();
        pSettings->setValue(tr("camera/tiltAngle"), cameraTiltAngle);
        double pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraTiltAngle+90.0);// In ms
        int iResult = set_servo_pulsewidth(gpioHostHandle, tiltPin, u_int32_t(pulseWidth));
        if(iResult < 0) {
          logMessage(logFile,
                     sFunctionName,
                     QString("Non riesco a far partire il PWM per il Tilt."));
        }
    }
#endif
    }// tilt

    sToken = XML_Parse(sMessage, "getPanTilt");
    if(sToken != sNoData) {
        if(pPanelServerSocket->isValid()) {
            QString sMessage;
            sMessage = QString("<pan_tilt>%1,%2</pan_tilt>").arg(int(cameraPanAngle)).arg(int(cameraTiltAngle));
            qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
            if(bytesSent != sMessage.length()) {
                logMessage(logFile,
                           sFunctionName,
                           QString("Unable to send pan & tilt values."));
            }
        }
    }// getPanTilt

    sToken = XML_Parse(sMessage, "getOrientation");
    if(sToken != sNoData) {
        if(pPanelServerSocket->isValid()) {
            QString sMessage;
            if(isMirrored)
                sMessage = QString("<orientation>%1</orientation>").arg(static_cast<int>(PanelOrientation::Reflected));
            else
                sMessage = QString("<orientation>%1</orientation>").arg(static_cast<int>(PanelOrientation::Normal));
            qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
            if(bytesSent != sMessage.length()) {
                logMessage(logFile,
                           sFunctionName,
                           QString("Unable to send orientation value."));
            }
        }
    }// getOrientation

    sToken = XML_Parse(sMessage, "setOrientation");
    if(sToken != sNoData) {
        bool ok;
        int iVal = sToken.toInt(&ok);
        if(!ok) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Illegal orientation value received: %1")
                               .arg(sToken));
            return;
        }
        try {
            PanelOrientation newOrientation = static_cast<PanelOrientation>(iVal);
            if(newOrientation == PanelOrientation::Reflected)
                isMirrored = true;
            else
                isMirrored = false;
        } catch(...) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Illegal orientation value received: %1")
                               .arg(sToken));
            return;
        }
        pSettings->setValue(tr("panel/orientation"), isMirrored);
        buildLayout();
    }// setOrientation

    sToken = XML_Parse(sMessage, "getScoreOnly");
    if(sToken != sNoData) {
        if(pPanelServerSocket->isValid()) {
            QString sMessage;
            sMessage = QString("<isScoreOnly>%1</isScoreOnly>").arg(static_cast<int>(getScoreOnly()));
            qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
            if(bytesSent != sMessage.length()) {
                logMessage(logFile,
                           sFunctionName,
                           QString("Unable to send scoreOnly configuration"));
            }
        }
    }// getScoreOnly

    sToken = XML_Parse(sMessage, "setScoreOnly");
    if(sToken != sNoData) {
        bool ok;
        int iVal = sToken.toInt(&ok);
        if(!ok) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Illegal value fo ScoreOnly received: %1")
                               .arg(sToken));
            return;
        }
        if(iVal==0) {
            setScoreOnly(false);
        }
        else {
            setScoreOnly(true);
        }
        pSettings->setValue(tr("panel/scoreOnly"), isScoreOnly);
    }// setScoreOnly
}


QGridLayout*
ScorePanel::createPanel() {
    return new QGridLayout();
}
